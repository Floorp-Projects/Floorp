/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/Services.jsm");
Cu.importGlobalProperties(["fetch", "TextEncoder"]);

XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ParseSymbols", "resource:///modules/ParseSymbols.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Subprocess", "resource://gre/modules/Subprocess.jsm");

const PREF_ASYNC_STACK = "javascript.options.asyncstack";
const PREF_SYMBOLS_URL = "extensions.geckoProfiler.symbols.url";
const PREF_GET_SYMBOL_RULES = "extensions.geckoProfiler.getSymbolRules";

const ASYNC_STACKS_ENABLED = Services.prefs.getBoolPref(PREF_ASYNC_STACK, false);

var {
  ExtensionError,
} = ExtensionUtils;

function parseSym(data) {
  const worker = new ChromeWorker("resource://app/modules/ParseSymbols-worker.js");
  const promise = new Promise((resolve, reject) => {
    worker.onmessage = (e) => {
      if (e.data.error) {
        reject(e.data.error);
      } else {
        resolve(e.data.result);
      }
    };
  });
  worker.postMessage(data);
  return promise;
}

class NMParser {
  constructor() {
    this._addrToSymMap = new Map();
    this._approximateLength = 0;
  }

  consume(data) {
    const lineRegex = /.*\n?/g;
    const buffer = this._currentLine + data;

    let match;
    while ((match = lineRegex.exec(buffer))) {
      let [line] = match;
      if (line[line.length - 1] === "\n") {
        this._processLine(line);
      } else {
        this._currentLine = line;
        break;
      }
    }
  }

  finish() {
    this._processLine(this._currentLine);
    return {syms: this._addrToSymMap, approximateLength: this._approximateLength};
  }

  _processLine(line) {
    // Example lines:
    // 00000000028c9888 t GrFragmentProcessor::MulOutputByInputUnpremulColor(sk_sp<GrFragmentProcessor>)::PremulFragmentProcessor::onCreateGLSLInstance() const::GLFP::~GLFP()
    // 00000000028c9874 t GrFragmentProcessor::MulOutputByInputUnpremulColor(sk_sp<GrFragmentProcessor>)::PremulFragmentProcessor::onCreateGLSLInstance() const::GLFP::~GLFP()
    // 00000000028c9874 t GrFragmentProcessor::MulOutputByInputUnpremulColor(sk_sp<GrFragmentProcessor>)::PremulFragmentProcessor::onCreateGLSLInstance() const::GLFP::~GLFP()
    // 0000000003a33730 r mozilla::OggDemuxer::~OggDemuxer()::{lambda()#1}::operator()() const::__func__
    // 0000000003a33930 r mozilla::VPXDecoder::Drain()::{lambda()#1}::operator()() const::__func__
    //
    // Some lines have the form
    // <address> ' ' <letter> ' ' <symbol>
    // and some have the form
    // <address> ' ' <symbol>
    // The letter has a meaning, but we ignore it.

    const regex = /([^ ]+) (?:. )?(.*)/;
    let match = regex.exec(line);
    if (match) {
      const [, address, symbol] = match;
      this._addrToSymMap.set(parseInt(address, 16), symbol);
      this._approximateLength += symbol.length;
    }
  }
}

class CppFiltParser {
  constructor(length) {
    this._index = 0;
    this._results = new Array(length);
  }

  consume(data) {
    const lineRegex = /.*\n?/g;
    const buffer = this._currentLine + data;

    let match;
    while ((match = lineRegex.exec(buffer))) {
      let [line] = match;
      if (line[line.length - 1] === "\n") {
        this._processLine(line);
      } else {
        this._currentLine = line;
        break;
      }
    }
  }

  finish() {
    this._processLine(this._currentLine);
    return this._results;
  }

  _processLine(line) {
    this._results[this._index++] = line.trimRight();
  }
}

async function readAllData(pipe, processData) {
  let data;
  while ((data = await pipe.readString())) {
    processData(data);
  }
}

async function spawnProcess(name, cmdArgs, processData, stdin = null) {
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
}

async function getSymbolsFromNM(path, arch) {
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
  let {syms, approximateLength} = parser.finish();

  if (Services.appinfo.OS === "Darwin") {
    const keys = Array.from(syms.keys());
    const values = keys.map(k => syms.get(k));
    const demangler = new CppFiltParser(keys.length);
    await spawnProcess("c++filt", [], data => demangler.consume(data), values.join("\n"));
    const newSymbols = demangler.finish();
    approximateLength = 0;
    for (let [i, symbol] of newSymbols.entries()) {
      approximateLength += symbol.length;
      syms.set(keys[i], symbol);
    }
  }

  return ParseSymbols.convertSymsMapToExpectedSymFormat(syms, approximateLength);
}

function pathComponentsForSymbolFile(debugName, breakpadId) {
  const symName = debugName.replace(/(\.pdb)?$/, ".sym");
  return [debugName, breakpadId, symName];
}

function urlForSymFile(debugName, breakpadId) {
  const profilerSymbolsURL = Services.prefs.getCharPref(PREF_SYMBOLS_URL,
                                                        "http://symbols.mozilla.org/");
  return profilerSymbolsURL + pathComponentsForSymbolFile(debugName, breakpadId).join("/");
}

function getContainingObjdirDist(path) {
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
}

function filePathForSymFileInObjDir(binaryPath, debugName, breakpadId) {
  // `mach buildsymbols` generates symbol files located
  // at /path/to/objdir/dist/crashreporter-symbols/.
  const objDirDist = getContainingObjdirDist(binaryPath);
  if (!objDirDist) {
    return null;
  }
  return OS.Path.join(objDirDist,
                      "crashreporter-symbols",
                      ...pathComponentsForSymbolFile(debugName, breakpadId));
}

const symbolCache = new Map();

function primeSymbolStore(libs) {
  for (const {debugName, breakpadId, path, arch} of libs) {
    symbolCache.set(urlForSymFile(debugName, breakpadId), {path, arch});
  }
}

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

          const cachedLibInfo = symbolCache.get(urlForSymFile(debugName, breakpadId));

          const symbolRules = Services.prefs.getCharPref(PREF_GET_SYMBOL_RULES, "localBreakpad,remoteBreakpad");
          const haveAbsolutePath = cachedLibInfo && OS.Path.split(cachedLibInfo.path).absolute;

          // We have multiple options for obtaining symbol information for the given
          // binary.
          //  "localBreakpad"  - Use existing symbol dumps stored in the object directory of a local
          //      Firefox build, generated using `mach buildsymbols` [requires path]
          //  "remoteBreakpad" - Use symbol dumps from the Mozilla symbol server [only requires
          //      debugName + breakpadId]
          //  "nm"             - Use the command line tool `nm` [linux/mac only, requires path]
          for (const rule of symbolRules.split(",")) {
            try {
              switch (rule) {
                case "localBreakpad":
                  if (haveAbsolutePath) {
                    const {path} = cachedLibInfo;
                    const filepath = filePathForSymFileInObjDir(path, debugName, breakpadId);
                    if (filepath) {
                      // NOTE: here and below, "return await" is used to ensure we catch any
                      // errors in the promise. A simple return would give the error to the
                      // caller.
                      return await parseSym({filepath});
                    }
                  }
                  break;
                case "remoteBreakpad":
                  const url = urlForSymFile(debugName, breakpadId);
                  return await parseSym({url});
                case "nm":
                  if (haveAbsolutePath) {
                    const {path, arch} = cachedLibInfo;
                    return await getSymbolsFromNM(path, arch);
                  }
                  break;
              }
            } catch (e) {
              // Each of our options can go wrong for a variety of reasons, so on failure
              // we will try the next one.
              // "localBreakpad" will fail if this is not a local build that's running from the object
              // directory or if the user hasn't run `mach buildsymbols` on it.
              // "remoteBreakpad" will fail if this is not an official mozilla build (e.g. Nightly) or a
              // known system library.
              // "nm" will fail if `nm` is not available.
            }
          }

          throw new Error(`Ran out of options to get symbols from library ${debugName} ${breakpadId}.`);
        },

        onRunning: new SingletonEventManager(context, "geckoProfiler.onRunning", fire => {
          isRunningObserver.addObserver(fire.async);
          return () => {
            isRunningObserver.removeObserver(fire.async);
          };
        }).api(),
      },
    };
  }
};
