/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
Cu.importGlobalProperties(["TextEncoder", "TextDecoder"]);

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(this, "Subprocess", "resource://gre/modules/Subprocess.jsm");

const PREF_ASYNC_STACK = "javascript.options.asyncstack";
const PREF_GET_SYMBOL_RULES = "extensions.geckoProfiler.getSymbolRules";

const ASYNC_STACKS_ENABLED = Services.prefs.getBoolPref(PREF_ASYNC_STACK, false);

var {
  ExtensionError,
} = ExtensionUtils;

const parseSym = data => {
  const worker = new ChromeWorker("resource://app/modules/ParseBreakpadSymbols-worker.js");
  const promise = new Promise((resolve, reject) => {
    worker.onmessage = (e) => {
      if (e.data.error) {
        reject(e.data.error);
      } else {
        resolve(e.data.result);
      }
    };
  });
  worker.postMessage(data, data.textBuffer ? [data.textBuffer.buffer] : []);
  return promise;
};

class NMParser {
  constructor() {
    this._worker = new ChromeWorker("resource://app/modules/ParseNMSymbols-worker.js");
  }

  consume(buffer) {
    this._worker.postMessage({buffer}, [buffer]);
  }

  finish() {
    const promise = new Promise((resolve, reject) => {
      this._worker.onmessage = (e) => {
        if (e.data.error) {
          reject(e.data.error);
        } else {
          resolve(e.data.result);
        }
      };
    });
    this._worker.postMessage({
      finish: true,
      isDarwin: Services.appinfo.OS === "Darwin",
    });
    return promise;
  }
}

class CppFiltParser {
  constructor() {
    this._worker = new ChromeWorker("resource://app/modules/ParseCppFiltSymbols-worker.js");
  }

  consume(buffer) {
    this._worker.postMessage({buffer}, [buffer]);
  }

  finish() {
    const promise = new Promise((resolve, reject) => {
      this._worker.onmessage = (e) => {
        if (e.data.error) {
          reject(e.data.error);
        } else {
          resolve(e.data.result);
        }
      };
    });
    this._worker.postMessage({
      finish: true,
    });
    return promise;
  }
}

const joinBuffers = function(buffers) {
  const byteLengthSum =
    buffers.reduce((accum, buffer) => accum + buffer.byteLength, 0);
  const joinedBuffer = new Uint8Array(byteLengthSum);
  let offset = 0;
  for (const buffer of buffers) {
    joinedBuffer.set(new Uint8Array(buffer), offset);
    offset += buffer.byteLength;
  }
  return joinedBuffer;
};

const readAllData = async function(pipe, processData) {
  let data;
  while ((data = await pipe.read()) && data.byteLength) {
    processData(data);
  }
};

const spawnProcess = async function(name, cmdArgs, processData, stdin = null) {
  const opts = {
    command: await Subprocess.pathSearch(name),
    arguments: cmdArgs,
  };
  const proc = await Subprocess.call(opts);

  if (stdin) {
    const encoder = new TextEncoder("utf-8");
    proc.stdin.write(encoder.encode(stdin));
    proc.stdin.close();
  }

  await readAllData(proc.stdout, processData);
};

const runCommandAndGetOutputAsString = async function(command, cmdArgs) {
  const opts = {
    command,
    arguments: cmdArgs,
    stderr: "pipe",
  };
  const proc = await Subprocess.call(opts);
  const chunks = [];
  await readAllData(proc.stdout, data => chunks.push(data));
  return (new TextDecoder()).decode(joinBuffers(chunks));
};

const getSymbolsFromNM = async function(path, arch) {
  const parser = new NMParser();

  const args = [path];
  if (Services.appinfo.OS === "Darwin") {
    args.unshift("-arch", arch);
  } else {
    // Mac's `nm` doesn't support the demangle option, so we have to
    // post-process the symbols with c++filt.
    args.unshift("--demangle");
  }

  await spawnProcess("nm", args, data => parser.consume(data));
  await spawnProcess("nm", ["-D", ...args], data => parser.consume(data));
  let result = await parser.finish();
  if (Services.appinfo.OS !== "Darwin") {
    return result;
  }

  const [addresses, symbolsJoinedBuffer] = result;
  const decoder = new TextDecoder();
  const symbolsJoined = decoder.decode(symbolsJoinedBuffer);
  const demangler = new CppFiltParser(addresses.length);
  await spawnProcess("c++filt", [], data => demangler.consume(data), symbolsJoined);
  const [newIndex, newBuffer] = await demangler.finish();
  return [addresses, newIndex, newBuffer];
};

const getEnvVarCaseInsensitive = function(env, name) {
  for (const [varname, value] of Object.entries(env)) {
    if (varname.toLowerCase() == name.toLowerCase()) {
      return value;
    }
  }
  return undefined;
};

const findPotentialMSDIAPaths = async function(env) {
  // dump_syms.exe needs to find msdia*.dll. This DLL is supplied by Microsoft
  // Visual Studio. However, starting with VS2017, this DLL is no longer
  // globally registered and needs to be loaded manually by dump_syms. And
  // dump_syms can only load it if the DLL is somewhere in the default DLL
  // search paths.
  // So we append the paths where the DLL is likely to be to the PATH
  // environment variable in order to increase the chances of dump_syms
  // succeeding.
  const programFilesX86Path =
    getEnvVarCaseInsensitive(env, "ProgramFiles(x86)") ||
    getEnvVarCaseInsensitive(env, "ProgramFiles");
  if (programFilesX86Path) {
    // Check if we have vswhere. This is a utilty that gets installed into a
    // fixed path and can be used to look up the actual location of the Visual
    // Studio installation. It's available starting with VS2017.
    const vswherePath = OS.Path.join(programFilesX86Path,
                                     "Microsoft Visual Studio", "Installer",
                                     "vswhere.exe");
    const args = [
      "-products", "*",
      "-latest",
      "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
      "-property", "installationPath",
      "-format", "value",
    ];
    try {
      const vsInstallationPath =
        (await runCommandAndGetOutputAsString(vswherePath, args)).trim();
      if (vsInstallationPath) {
        const diaSDKPath = OS.Path.join(vsInstallationPath, "DIA SDK");
        return [
          OS.Path.join(diaSDKPath, "bin"),
          OS.Path.join(diaSDKPath, "bin", "amd64"),
        ];
      }
    } catch (e) {
      // Something went wrong. Either Visual Studio isn't installed, or a
      // pre-2017 version is installed, or vswhere wasn't successful.
      // Ignore the error.
    }
  }
  // Add no extra DLL search paths and hope that dump_syms.exe is going to
  // succeed regardless.
  return [];
};

const getSymbolsUsingWindowsDumpSyms = async function(dumpSymsPath, debugPath) {
  const env = Subprocess.getEnvironment();
  const extraPaths = await findPotentialMSDIAPaths(env);
  const existingPaths = env.PATH ? env.PATH.split(";") : [];
  env.PATH = existingPaths.concat(extraPaths).join(";");
  const opts = {
    command: dumpSymsPath,
    arguments: [debugPath],
    environment: env,
    stderr: "pipe",
  };
  const proc = await Subprocess.call(opts);
  const chunks = [];
  await readAllData(proc.stdout, data => chunks.push(data));
  const textBuffer = joinBuffers(chunks);
  if (textBuffer.byteLength === 0) {
    throw new Error("did not receive any stdout from dump_syms.exe");
  }
  return parseSym({textBuffer});
};

const pathComponentsForSymbolFile = (debugName, breakpadId) => {
  const symName = debugName.replace(/(\.pdb)?$/, ".sym");
  return [debugName, breakpadId, symName];
};

const getContainingObjdirDist = path => {
  let curPath = path;
  let curPathBasename = OS.Path.basename(curPath);
  while (curPathBasename) {
    if (curPathBasename === "dist") {
      return curPath;
    }
    const parentDirPath = OS.Path.dirname(curPath);
    if (curPathBasename === "bin") {
      return parentDirPath;
    }
    curPath = parentDirPath;
    curPathBasename = OS.Path.basename(curPath);
  }
  return null;
};

const filePathForSymFileInObjDir = (binaryPath, debugName, breakpadId) => {
  // `mach buildsymbols` generates symbol files located
  // at /path/to/objdir/dist/crashreporter-symbols/.
  const objDirDist = getContainingObjdirDist(binaryPath);
  if (!objDirDist) {
    return null;
  }
  return OS.Path.join(objDirDist,
                      "crashreporter-symbols",
                      ...pathComponentsForSymbolFile(debugName, breakpadId));
};

const dumpSymsPathInObjDir = binaryPath => {
  // `dump_syms.exe` is generated by the build process
  // at /path/to/objdir/dist/host/bin/.
  const objDirDist = getContainingObjdirDist(binaryPath);
  if (!objDirDist) {
    return null;
  }
  return OS.Path.join(objDirDist, "host", "bin", "dump_syms.exe");
};

const symbolCache = new Map();

const primeSymbolStore = libs => {
  for (const {debugName, debugPath, breakpadId, path, arch} of libs) {
    symbolCache.set(`${debugName}/${breakpadId}`, {path, debugPath, arch});
  }
};

let previouslySuccessfulDumpSymsPath = null;

const isRunningObserver = {
  _observers: new Set(),

  observe(subject, topic, data) {
    switch (topic) {
      case "profiler-started":
      case "profiler-stopped":
        // Call observer(false) or observer(true), but do it through a promise
        // so that it's asynchronous.
        // We don't want it to be synchronous because of the observer call in
        // addObserver, which is asynchronous, and we want to get the ordering
        // right.
        const isRunningPromise = Promise.resolve(topic === "profiler-started");
        for (let observer of this._observers) {
          isRunningPromise.then(observer);
        }
        break;
    }
  },

  _startListening() {
    Services.obs.addObserver(this, "profiler-started");
    Services.obs.addObserver(this, "profiler-stopped");
  },

  _stopListening() {
    Services.obs.removeObserver(this, "profiler-started");
    Services.obs.removeObserver(this, "profiler-stopped");
  },

  addObserver(observer) {
    if (this._observers.size === 0) {
      this._startListening();
    }

    this._observers.add(observer);
    observer(Services.profiler.IsActive());
  },

  removeObserver(observer) {
    if (this._observers.delete(observer) && this._observers.size === 0) {
      this._stopListening();
    }
  },
};

this.geckoProfiler = class extends ExtensionAPI {
  getAPI(context) {
    return {
      geckoProfiler: {
        async start(options) {
          const {bufferSize, interval, features, threads} = options;

          Services.prefs.setBoolPref(PREF_ASYNC_STACK, false);
          if (threads) {
            Services.profiler.StartProfiler(bufferSize, interval, features, features.length, threads, threads.length);
          } else {
            Services.profiler.StartProfiler(bufferSize, interval, features, features.length);
          }
        },

        async stop() {
          if (ASYNC_STACKS_ENABLED !== null) {
            Services.prefs.setBoolPref(PREF_ASYNC_STACK, ASYNC_STACKS_ENABLED);
          }

          Services.profiler.StopProfiler();
        },

        async pause() {
          Services.profiler.PauseSampling();
        },

        async resume() {
          Services.profiler.ResumeSampling();
        },

        async getProfile() {
          if (!Services.profiler.IsActive()) {
            throw new ExtensionError("The profiler is stopped. " +
              "You need to start the profiler before you can capture a profile.");
          }

          return Services.profiler.getProfileDataAsync();
        },

        async getProfileAsArrayBuffer() {
          if (!Services.profiler.IsActive()) {
            throw new ExtensionError("The profiler is stopped. " +
              "You need to start the profiler before you can capture a profile.");
          }

          return Services.profiler.getProfileDataAsArrayBuffer();
        },

        async getSymbols(debugName, breakpadId) {
          if (symbolCache.size === 0) {
            primeSymbolStore(Services.profiler.sharedLibraries);
          }

          const cachedLibInfo = symbolCache.get(`${debugName}/${breakpadId}`);
          if (!cachedLibInfo) {
            throw new Error(
              `The library ${debugName} ${breakpadId} is not in the Services.profiler.sharedLibraries list, ` +
              "so the local path for it is not known and symbols for it can not be obtained. " +
              "This usually happens if a content process uses a library that's not used in the parent " +
              "process - Services.profiler.sharedLibraries only knows about libraries in the parent process.");
          }

          const {path, debugPath, arch} = cachedLibInfo;
          if (!OS.Path.split(path).absolute) {
            throw new Error(
              `Services.profiler.sharedLibraries did not contain an absolute path for the library ${debugName} ${breakpadId}, ` +
              "so symbols for this library can not be obtained.");
          }

          const symbolRules = Services.prefs.getCharPref(PREF_GET_SYMBOL_RULES, "localBreakpad");

          // We have multiple options for obtaining symbol information for the given
          // binary.
          //  "localBreakpad"  - Use existing symbol dumps stored in the object directory of a local
          //      Firefox build, generated using `mach buildsymbols`
          //  "nm"             - Use the command line tool `nm` [linux/mac only]
          //  "dump_syms.exe"  - Use the tool dump_syms.exe from the object directory [Windows only]
          for (const rule of symbolRules.split(",")) {
            try {
              switch (rule) {
                case "localBreakpad":
                  const filepath = filePathForSymFileInObjDir(path, debugName, breakpadId);
                  if (filepath) {
                    // NOTE: here and below, "return await" is used to ensure we catch any
                    // errors in the promise. A simple return would give the error to the
                    // caller.
                    return await parseSym({filepath});
                  }
                  break;
                case "nm":
                  return await getSymbolsFromNM(path, arch);
                case "dump_syms.exe":
                  let dumpSymsPath = dumpSymsPathInObjDir(path);
                  if (!dumpSymsPath && previouslySuccessfulDumpSymsPath) {
                    // We may be able to dump symbol for system libraries
                    // (which are outside the object directory, and for
                    // which dumpSymsPath will be null) using dump_syms.exe.
                    // If we know that dump_syms.exe exists, try it.
                    dumpSymsPath = previouslySuccessfulDumpSymsPath;
                  }
                  if (dumpSymsPath) {
                    const result =
                      await getSymbolsUsingWindowsDumpSyms(dumpSymsPath, debugPath);
                    previouslySuccessfulDumpSymsPath = dumpSymsPath;
                    return result;
                  }
                  break;
              }
            } catch (e) {
              // Each of our options can go wrong for a variety of reasons, so on failure
              // we will try the next one.
              // "localBreakpad" will fail if this is not a local build that's running from the object
              // directory or if the user hasn't run `mach buildsymbols` on it.
              // "nm" will fail if `nm` is not available.
              // "dump_syms.exe" will fail if this is not a local build that's running from the object
              // directory, or if dump_syms.exe doesn't exist in the object directory, or if
              // dump_syms.exe failed for other reasons.
            }
          }

          throw new Error(`Ran out of options to get symbols from library ${debugName} ${breakpadId}.`);
        },

        onRunning: new EventManager({
          context,
          name: "geckoProfiler.onRunning",
          register: fire => {
            isRunningObserver.addObserver(fire.async);
            return () => {
              isRunningObserver.removeObserver(fire.async);
            };
          },
        }).api(),
      },
    };
  }
};
