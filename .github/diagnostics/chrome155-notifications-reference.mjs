// SPDX-License-Identifier: MPL-2.0
// Actual authored extension DOC public APIs; no replacement/mock/vendor code.
import { spawn, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { join, resolve } from "node:path";
import { parseArgs } from "node:util";

const { values } = parseArgs({
  options: {
    chrome: { type: "string" },
    output: { type: "string" },
    geometry: { type: "string" },
    python: { type: "string" },
    observer: { type: "string" },
    authorization: { type: "string" },
  },
});
/** @param {string} name */
const argument = (name) => {
  const value = values[/** @type {keyof typeof values} */ (name)];
  if (typeof value !== "string" || !value) throw Error(`Missing ${name}`);
  return value;
};
const geometry = argument("geometry");
if (
  process.platform !== "win32" ||
  (geometry !== "normal" && geometry !== "maximized")
) throw Error("Actual Windows geometry required");
const chrome = argument("chrome"),
  python = argument("python"),
  observer = argument("observer");
const output = resolve(argument("output"));
mkdirSync(output, { recursive: false });
/** @type {{chrome:{sha256:string}}} */
const auth = JSON.parse(readFileSync(argument("authorization"), "utf8"));
if (!/^[a-f0-9]{64}$/.test(auth.chrome.sha256)) {
  throw Error("Invalid owned executable identity");
}
/** @param {string} path */
const sha = (path) =>
  createHash("sha256").update(readFileSync(path)).digest("hex");
if (sha(chrome) !== auth.chrome.sha256) {
  throw Error("Authenticated Windows executable changed");
}
/** @param {string} name @param {unknown} value */
const save = (name, value) =>
  writeFileSync(join(output, name), JSON.stringify(value, null, 2) + "\n");
/** @param {number} ms @returns {Promise<void>} */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {()=>T|false} read @param {string} description @param {number} [milliseconds] @returns {Promise<T>} */
const wait = async (read, description, milliseconds = 8000) => {
  const deadline = Date.now() + milliseconds;
  while (Date.now() < deadline) {
    const result = read();
    if (result) return result;
    await delay(25);
  }
  throw Error(`Timed out waiting for ${description}`);
};
class Client {
  /** @param {WebSocket} socket */
  constructor(socket) {
    this.socket = socket;
    this.sequence = 0;
    /** @type {Map<number,import('../../tools/chrome-extension-runtime/cdp-types.ts').PendingCDPCall>} */
    this.pending = new Map();
    socket.addEventListener("message", (event) => {
      const message =
        /** @type {import('../../tools/chrome-extension-runtime/cdp-types.ts').CDPMessage} */ (JSON
          .parse(String(event.data)));
      const pending = message.id === undefined
        ? undefined
        : this.pending.get(message.id);
      if (!pending) return;
      this.pending.delete(/** @type {number} */ (message.id));
      if (message.error) pending.reject(Error(JSON.stringify(message.error)));
      else pending.resolve(message.result);
    });
    socket.addEventListener("close", () => {
      for (const row of this.pending.values()) {
        row.reject(Error("Owned CDP socket closed"));
      }
      this.pending.clear();
    });
  }
  /**
   * @template {import('../../tools/chrome-extension-runtime/cdp-types.ts').CDPMethod} Method
   * @param {Method} method @param {Record<string,unknown>} [params] @param {string} [sessionId] @param {number} [timeout]
   * @returns {Promise<import('../../tools/chrome-extension-runtime/cdp-types.ts').CDPResult<Method>>}
   */
  call(method, params = {}, sessionId, timeout = 8000) {
    const id = ++this.sequence;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(Error(`CDP ${method} timed out`));
      }, timeout);
      this.pending.set(id, {
        resolve(value) {
          clearTimeout(timer);
          resolve(
            /** @type {import('../../tools/chrome-extension-runtime/cdp-types.ts').CDPResult<Method>} */ (value),
          );
        },
        reject(error) {
          clearTimeout(timer);
          reject(error);
        },
      });
      this.socket.send(
        JSON.stringify({
          id,
          method,
          params,
          ...(sessionId ? { sessionId } : {}),
        }),
      );
    });
  }
}
const title = "Floorp authored native programmatic close";
const publicProgram = String.raw`
const state={ready:true,created:null,events:[],steps:[],errors:[]};
const row=(method,value)=>{state.steps.push({method,value,at:performance.now()});return value};
chrome.notifications.onClosed.addListener((id,byUser)=>state.events.push({id,byUser,at:performance.now()}));
globalThis.authoredProbe={state,async begin(){
 try{state.created=row('create',await chrome.notifications.create('actual-clear',{
 type:'basic',title:'Floorp authored native programmatic close',message:'Before real native clear',
 iconUrl:'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAL0lEQVR4nO3OIQEAAAgDMFIRjehYiHEzMb/q2UsqAQEBAQEBAQEBAQEBAQGBdOABkcmElw3yDhMAAAAASUVORK5CYII=',requireInteraction:true}));}
 catch(error){state.errors.push({method:'create',name:error.name,message:error.message,at:performance.now()})}
 return {runtimeId:chrome.runtime.id,state};
},async finish(){
 for(const [method,args] of [['getAll',[]],['clear',['actual-clear']],['clear',['actual-clear']],['getAll',[]]]){
  try{row(method,await chrome.notifications[method](...args))}
  catch(error){state.errors.push({method,name:error.name,message:error.message,at:performance.now()})}
 }
 const deadline=performance.now()+20000;
 while(!state.events.some(value=>value.id==='actual-clear')&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,25));
 return {runtimeId:chrome.runtime.id,state};
},async replacementBegin(){
 try{row('replacement.create',await chrome.notifications.create('actual-replacement',{
 type:'basic',title:'Floorp authored native programmatic close',message:'Original version',
 iconUrl:'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAL0lEQVR4nO3OIQEAAAgDMFIRjehYiHEzMb/q2UsqAQEBAQEBAQEBAQEBAQGBdOABkcmElw3yDhMAAAAASUVORK5CYII=',requireInteraction:true}));}
 catch(error){state.errors.push({method:'replacement.create',name:error.name,message:error.message,at:performance.now()})}
 return {runtimeId:chrome.runtime.id,state};
},async replacementUpdate(){
 try{row('replacement.update',await chrome.notifications.update('actual-replacement',{message:'Current version'}))}
 catch(error){state.errors.push({method:'replacement.update',name:error.name,message:error.message,at:performance.now()})}
 return {runtimeId:chrome.runtime.id,state};
},async replacementFinish(){
 try{row('replacement.getAll.beforeClear',await chrome.notifications.getAll());row('replacement.closedCount.beforeClear',state.events.length);row('replacement.clear',await chrome.notifications.clear('actual-replacement'))}
 catch(error){state.errors.push({method:'replacement.finish',name:error.name,message:error.message,at:performance.now()})}
 const deadline=performance.now()+20000;
 while(!state.events.some(value=>value.id==='actual-replacement')&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,25));
 try{row('replacement.getAll.afterClear',await chrome.notifications.getAll())}
 catch(error){state.errors.push({method:'replacement.getAll.afterClear',name:error.name,message:error.message,at:performance.now()})}
 return {runtimeId:chrome.runtime.id,state};
}};
`;
/** @type {Record<string,unknown>} */
const result = {
  scope:
    "Genuine Windows Chrome DOC notifications observation; zero qualification credit",
  geometry: geometry,
  observedAt: new Date().toISOString(),
  observationCompleted: false,
  publicProgramSha256: createHash("sha256").update(publicProgram).digest("hex"),
  notificationUiAndPublicApiAreSeparate: true,
  noOsOrNotificationSettingsChanged: true,
};
/** @type {boolean[]} */
const uiObservationResults = [];
/** @param {string} phase @param {number} [processId] @param {number} [uiWait] @param {string} [expectedMessage] @returns {import('./chrome155-notification-reference-types.ts').ChromeOSSnapshot} */
const observe = (phase, processId = 0, uiWait = 0, expectedMessage = "") => {
  const path = join(output, `snapshot-${phase}.json`);
  const argv = [
    observer,
    "--snapshot",
    "--output",
    path,
    "--chrome-pid",
    String(processId),
    "--title",
    title,
    "--ui-wait-ms",
    String(uiWait),
    "--expected-message",
    expectedMessage,
  ];
  const call = spawnSync(python, argv, {
    encoding: "utf8",
    timeout: uiWait + 15000,
    windowsHide: true,
  });
  writeFileSync(
    join(output, `snapshot-${phase}.log`),
    String(call.stdout) + String(call.stderr),
  );
  if (call.status !== 0 || call.error) {
    throw Error(
      `Owned read-only ${phase} observer failed: ${
        call.error?.name ?? call.status
      }`,
    );
  }
  const snapshot =
    /** @type {import('./chrome155-notification-reference-types.ts').ChromeOSSnapshot} */ (JSON
      .parse(readFileSync(path, "utf8")));
  uiObservationResults.push(
    snapshot.authoredUi?.observerExitCode === 0 && !snapshot.authoredUi.error,
  );
  return snapshot;
};
/** @type {import('node:child_process').ChildProcess|undefined} */ let child;
/** @type {Client|undefined} */ let client;
/** @type {WebSocket|undefined} */ let socket;
/** @type {number|undefined} */ let browserLog;
/** @type {{path:string,sha256:string}[]|undefined} */ let authoredSourceRecords;
try {
  result.externalBefore = observe("before-launch");
  const profile = join(output, "chrome-profile");
  browserLog = await import("node:fs").then((fs) =>
    fs.openSync(join(output, "chrome.log"), "wx")
  );
  const argv = [
    "--remote-debugging-port=0",
    `--user-data-dir=${profile}`,
    "--no-first-run",
    "--no-default-browser-check",
    "about:blank",
  ];
  result.browserArgv = [chrome, ...argv];
  const launched = spawn(chrome, argv, {
    stdio: ["ignore", "ignore", browserLog],
  });
  child = launched;
  launched.on("error", (error) => {
    result.spawnError = String(error);
  });
  const pid = launched.pid;
  if (!pid) throw Error("Owned Chrome PID missing");
  result.browserProcessId = pid;
  const portFile = join(profile, "DevToolsActivePort");
  const endpoint = await wait(() => {
    if (result.spawnError) throw Error(String(result.spawnError));
    if (launched.exitCode !== null) {
      throw Error("Owned Chrome exited before DevToolsActivePort");
    }
    if (!existsSync(portFile)) return false;
    const [portText, path] = readFileSync(portFile, "utf8").trim().split(
      /\r?\n/,
    );
    if (
      !/^\d+$/.test(portText) || Number(portText) < 1 ||
      Number(portText) > 65535 ||
      !/^\/devtools\/browser\/[a-z0-9-]+$/i.test(path)
    ) throw Error("Invalid actual owned loopback endpoint");
    return `ws://127.0.0.1:${portText}${path}`;
  }, "real Chrome loopback endpoint");
  const browserSocket = new WebSocket(endpoint);
  socket = browserSocket;
  await new Promise((resolve, reject) => {
    const timer = setTimeout(
      () => reject(Error("Loopback connection timeout")),
      8000,
    );
    browserSocket.addEventListener("open", () => {
      clearTimeout(timer);
      resolve(undefined);
    }, { once: true });
    browserSocket.addEventListener("error", () => {
      clearTimeout(timer);
      reject(Error("Loopback connection error"));
    }, { once: true });
  });
  const controller = new Client(browserSocket);
  client = controller;
  const version = await controller.call("Browser.getVersion");
  result.version = version;
  if (
    version.product !== "Chrome/155.0.8059.84" ||
    version.revision !== "@5869fe313571d75152b77813cf0a4ee26a4e6df0"
  ) throw Error("Exact frozen Windows Chrome version/revision mismatch");
  const extension = join(output, "authored-extension");
  mkdirSync(extension);
  writeFileSync(
    join(extension, "manifest.json"),
    JSON.stringify({
      manifest_version: 3,
      name: "Authored Windows notification reference",
      version: "1.0",
      permissions: ["notifications"],
    }),
  );
  writeFileSync(
    join(extension, "probe.html"),
    '<!doctype html><title>Owned Windows notification reference</title><script src="probe.js"></script>',
  );
  writeFileSync(join(extension, "probe.js"), publicProgram);
  authoredSourceRecords = ["manifest.json", "probe.html", "probe.js"].map(
    (name) => ({
      path: join(extension, name),
      sha256: sha(join(extension, name)),
    }),
  );
  result.authoredSourceRecords = authoredSourceRecords;
  const loaded = await controller.call("Extensions.loadUnpacked", {
    path: extension,
  });
  result.loaded = loaded;
  if (!/^[a-p]{32}$/.test(loaded.id)) {
    throw Error("Real authored Chrome extension ID missing");
  }
  const target = await controller.call("Target.createTarget", {
    url: `chrome-extension://${loaded.id}/probe.html`,
  });
  const attached = await controller.call("Target.attachToTarget", {
    targetId: target.targetId,
    flatten: true,
  });
  /** @template T @param {string} expression @returns {Promise<T>} */
  const evaluate = async (expression) => {
    const response = await controller.call(
      "Runtime.evaluate",
      { expression, awaitPromise: true, returnByValue: true },
      attached.sessionId,
      28000,
    );
    if (response.exceptionDetails) {
      throw Error(JSON.stringify(response.exceptionDetails));
    }
    return /** @type {T} */ (response.result.value);
  };
  await evaluate(
    `new Promise((resolve,reject)=>{const end=Date.now()+8000;const tick=()=>{if(globalThis.authoredProbe?.state.ready)return resolve(true);if(Date.now()>end)return reject(Error('Actual extension DOC not ready'));setTimeout(tick,25)};tick()})`,
  );
  const window = await controller.call("Browser.getWindowForTarget", {
    targetId: target.targetId,
  });
  result.windowId = window.windowId;
  const before = observe("before-placement", pid);
  const own = before.ownedChromeWindows.find((row) => row.workArea);
  if (!own || !own.workArea) throw Error("Actual own Chrome workarea missing");
  if (own.dpi !== 96) {
    throw Error(
      "Observed non96DPI requires reviewed coordinate conversion; no DPI setting is changed",
    );
  }
  const work = own.workArea,
    width = Math.min(800, work.right - work.left - 64),
    height = Math.min(600, work.bottom - work.top - 64);
  if (width < 600 || height < 440) {
    throw Error("Actual Chrome workarea too small");
  }
  const normal = {
    left: Math.round(work.left + ((work.right - work.left) - width) / 2),
    top: Math.round(work.top + ((work.bottom - work.top) - height) / 2),
    width,
    height,
  };
  await controller.call("Browser.setWindowBounds", {
    windowId: window.windowId,
    bounds: { windowState: "normal" },
  });
  await controller.call("Browser.setWindowBounds", {
    windowId: window.windowId,
    bounds: normal,
  });
  if (geometry === "maximized") {
    await controller.call("Browser.setWindowBounds", {
      windowId: window.windowId,
      bounds: { windowState: "maximized" },
    });
  }
  const deadline = Date.now() + 5000;
  while (true) {
    const actual = await controller.call("Browser.getWindowBounds", {
      windowId: window.windowId,
    });
    result.actualWindowBounds = actual.bounds;
    if (
      actual.bounds.windowState === geometry &&
      (geometry === "maximized" ||
        ["left", "top", "width", "height"].every((key) =>
          typeof actual
              .bounds[/** @type {'left'|'top'|'width'|'height'} */ (key)] ===
            "number" &&
          Math.abs(
              /** @type {number} */ (actual
                .bounds[/** @type {'left'|'top'|'width'|'height'} */ (key)]) -
                normal[/** @type {'left'|'top'|'width'|'height'} */ (key)],
            ) <= 2
        ))
    ) break;
    if (Date.now() > deadline) {
      throw Error("Actual owned Chrome geometry readiness failed");
    }
    await delay(25);
  }
  result.requestedNormalBounds = normal;
  result.beforeCreate = observe("before-create", pid);
  result.histogramsBefore = await controller.call("Browser.getHistograms", {
    query: "Notifications.Windows",
  });
  /** @type {import('./chrome155-notification-reference-types.ts').PublicNotificationResult} */
  const begin = await evaluate("authoredProbe.begin()");
  result.begin = begin;
  if (begin.runtimeId !== loaded.id) {
    throw Error("Real Chrome DOC owner mismatch");
  }
  save("partial-result.json", result);
  result.uiWhileNotification = observe(
    "notification-ui",
    pid,
    20000,
    "Before real native clear",
  );
  result.finish = await evaluate("authoredProbe.finish()");
  result.afterClear = observe("after-clear", pid);
  result.replacementBegin = await evaluate("authoredProbe.replacementBegin()");
  result.replacementOldUi = observe(
    "replacement-old-ui",
    pid,
    20000,
    "Original version",
  );
  result.replacementUpdate = await evaluate(
    "authoredProbe.replacementUpdate()",
  );
  result.replacementCurrentUi = observe(
    "replacement-current-ui",
    pid,
    20000,
    "Current version",
  );
  /** @type {import('./chrome155-notification-reference-types.ts').PublicNotificationResult} */
  const replacementFinish = await evaluate("authoredProbe.replacementFinish()");
  result.replacementFinish = replacementFinish;
  result.afterReplacementClear = observe("after-replacement-clear", pid);
  const rows = replacementFinish.state.steps;
  /** @param {string} name */
  const value = (name) => rows.find((row) => row.method === name)?.value;
  result.publicSequenceMatchesOriginalContract =
    !replacementFinish.state.errors.length &&
    value("create") === "actual-clear" &&
    JSON.stringify(value("getAll")) ===
      JSON.stringify({ "actual-clear": true }) &&
    rows.filter((row) => row.method === "clear")[0]?.value === true &&
    rows.filter((row) => row.method === "clear")[1]?.value === false &&
    JSON.stringify(rows.filter((row) => row.method === "getAll")[1]?.value) ===
      "{}" &&
    value("replacement.create") === "actual-replacement" &&
    value("replacement.update") === true &&
    JSON.stringify(value("replacement.getAll.beforeClear")) ===
      JSON.stringify({ "actual-replacement": true }) &&
    value("replacement.closedCount.beforeClear") === 1 &&
    value("replacement.clear") === true &&
    JSON.stringify(value("replacement.getAll.afterClear")) === "{}" &&
    JSON.stringify(
        replacementFinish.state.events.map(({ id, byUser }) => ({
          id,
          byUser,
        })),
      ) ===
      JSON.stringify([{ id: "actual-clear", byUser: false }, {
        id: "actual-replacement",
        byUser: false,
      }]);
  result.publicSequenceLimit =
    "Public contract comparison only. Native bubble instance replacement ownership/visibility needs separate actual UI evidence; no API inventory-only UI PASS.";
  const histogramsAfter = await controller.call("Browser.getHistograms", {
    query: "Notifications.Windows",
  });
  result.histogramsAfter = histogramsAfter;
  result.nativeActivationReadiness = {
    histogram: histogramsAfter.histograms.find((value) =>
      value.name === "Notifications.Windows.SetReadyCallbackStatus2"
    ) ?? null,
    sourceEnum: {
      0: "both existing native activator and Start Menu shortcut present",
      1: "shortcut missing/misconfigured",
      2: "COM server missing/misconfigured",
      3: "both shortcut and COM server missing/misconfigured",
    },
    limit:
      "Raw actual counts; status0 is source-backed prerequisite readiness, not proof a toast was visibly delivered. No registration or backend setting is changed.",
  };
  result.observationCompleted = true;
} catch (error) {
  result.error = String(error);
  result.stack = error instanceof Error ? error.stack : undefined;
} finally {
  if (client) {
    try {
      await client.call("Browser.close");
    } catch (error) {
      result.closeError = String(error);
    }
  }
  if (child) {
    const closing = child;
    try {
      await wait(
        () => closing.exitCode !== null,
        "owned Chrome process closure",
        10000,
      );
    } catch (error) {
      result.normalClosureFailed = String(error);
      closing.kill();
      try {
        await wait(
          () => closing.exitCode !== null,
          "owned killed Chrome closure",
          5000,
        );
      } catch (killError) {
        result.killClosureError = String(killError);
      }
    }
  }
  socket?.close();
  if (browserLog !== undefined) {
    const fs = await import("node:fs");
    fs.closeSync(browserLog);
  }
  result.browserExitCode = child?.exitCode;
  result.binaryUnchanged = sha(chrome) === auth.chrome.sha256;
  result.authoredSourceUnchanged = !authoredSourceRecords ||
    authoredSourceRecords.every((row) => sha(row.path) === row.sha256);
  try {
    result.externalAfter = observe("after-browser-closed");
  } catch (error) {
    result.afterObserverError = String(error);
    result.observationCompleted = false;
  }
  result.uiObserversCompleted = uiObservationResults.every(Boolean);
  result.observationCompleted = result.observationCompleted &&
    result.browserExitCode === 0 && result.uiObserversCompleted &&
    result.binaryUnchanged && result.authoredSourceUnchanged &&
    !result.normalClosureFailed;
  save("result.json", result);
}
process.exitCode = result.observationCompleted ? 0 : 1;
