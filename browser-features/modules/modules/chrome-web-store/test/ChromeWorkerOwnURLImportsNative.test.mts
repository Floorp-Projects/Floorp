// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Real native imports, cache/module identity and foreign-resource isolation.
/// <reference path="../../../../../libs/@types/gecko/index.d.ts" />
import { zipSync } from "fflate/browser";
import type {
  NativeURLExtension,
  NativeURLTab,
  NativeURLWindow,
} from "./helpers/public-extension-url-test-types.ts";
import type {
  WorkerOwnImportAddon,
  WorkerOwnImportKind,
  WorkerOwnImportMode,
  WorkerOwnImportReply,
  WorkerOwnImportReport,
  WorkerOwnImportRow,
} from "./helpers/worker-own-url-import-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const pause = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));
async function wait(
  read: () => boolean,
  milliseconds: number,
): Promise<boolean> {
  const deadline = Date.now() + milliseconds;
  while (Date.now() < deadline) {
    if (read()) return true;
    await pause(25);
  }
  return false;
}
function parentProtocol() {
  try {
    return {
      scheme: Services.io.getProtocolHandler("chrome-extension").scheme,
    };
  } catch (error) {
    return { error: String(error) };
  }
}
async function importCase(
  kind: WorkerOwnImportKind,
  mode: WorkerOwnImportMode,
  letter: string,
): Promise<WorkerOwnImportRow> {
  const id = letter.repeat(32),
    gecko = id + "@chromium-extension",
    token = Services.uuid.generateUUID().toString();
  const extensionSpec = mode === "public"
    ? "chrome-extension://" + id + "/"
    : "./";
  const imported = kind === "module" ? "imported.mjs" : "imported.js";
  const prefix = kind === "classic"
    ? `let observedURL;let startup;try{observedURL=chrome.runtime.getURL('imported.js');importScripts(${
      JSON.stringify(extensionSpec + imported)
    });startup={ok:true,value:self.authoredOwnImport};}catch(error){startup={ok:false,phase:observedURL?"importScripts":"runtime.getURL",name:error.name,error:String(error),code:error.code,stack:error.stack};}`
    : mode === "mixed"
    ? `import * as relative from './imported.mjs';import * as alias from ${
      JSON.stringify("chrome-extension://" + id + "/imported.mjs")
    };const observedURL=chrome.runtime.getURL('imported.mjs');const startup={ok:true,value:relative.marker,sameNamespace:relative===alias,evaluations:self.authoredModuleEvaluations};`
    : `import { marker } from ${
      JSON.stringify(extensionSpec + imported)
    };const observedURL=chrome.runtime.getURL('imported.mjs');const startup={ok:true,value:marker};`;
  const denied = kind === "classic"
    ? `const deniedImports=[];for(const url of ${
      JSON.stringify([
        "chrome-extension://" + "p".repeat(32) + "/private.js",
        "chrome-extension://user@" + "{OWN}" + "/imported.js",
        "chrome-extension://" + "{OWN}" + ":9876/imported.js",
      ])
    }.map(url=>url.replace('{OWN}',chrome.runtime.id))){try{importScripts(url);deniedImports.push({url,denied:false});}catch(error){deniedImports.push({url,denied:true,error:String(error),name:error.name});}}startup.deniedImports=deniedImports;startup.foreignPrivateExecuted=self.foreignPrivateImportExecuted===true;`
    : "";
  const worker = prefix + denied +
    `chrome.runtime.onMessage.addListener(message=>message.token===${
      JSON.stringify(token)
    }?Promise.resolve({startup,observedURL,id:chrome.runtime.id,location:location.href}):undefined);`;
  const files = {
    "manifest.json": JSON.stringify({
      manifest_version: 3,
      name: "Authored startup " + kind + " " + mode,
      version: "1.0",
      background: {
        service_worker: "worker.js",
        ...(kind === "module" ? { type: "module" } : {}),
      },
      browser_specific_settings: {
        gecko: {
          id: gecko,
          floorp_chrome_extension: { schema_version: 1, original_id: id },
        },
      },
    }),
    "worker.js": worker,
    "control.html": '<!doctype html><script src="control.js"></script>',
    "control.js":
      `document.title='ready';window.addEventListener('message',async event=>{if(event.data?.token!==${
        JSON.stringify(token)
      })return;const row={token:event.data.token,phase:'beforeMessage',location:location.href};try{const r=await fetch(chrome.runtime.getURL(${
        JSON.stringify(imported)
      }));row.resource={url:r.url,status:r.status,mime:r.headers.get('content-type'),body:await r.text()};}catch(error){row.resource={error:String(error),name:error.name,stack:error.stack};}document.title=JSON.stringify(row);try{row.value=await browser.runtime.sendMessage({token:event.data.token});}catch(error){row.error=String(error);row.stack=error.stack;}row.phase='complete';document.title=JSON.stringify(row);});`,
    "imported.js": 'self.authoredOwnImport="real-authored-import";',
    "imported.mjs":
      'self.authoredModuleEvaluations=(self.authoredModuleEvaluations||0)+1;export const marker="real-authored-import";',
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    "owned-startup-" + token + ".xpi",
  );
  await IOUtils.write(
    path,
    zipSync(
      Object.fromEntries(
        Object.entries(files).map((
          [name, value],
        ) => [name, new TextEncoder().encode(value)]),
      ),
    ),
  );
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as NativeURLWindow;
  let addon: WorkerOwnImportAddon | undefined,
    tab: NativeURLTab | undefined;
  const row: WorkerOwnImportRow = {
    kind,
    mode,
    id,
    parentProtocolBefore: parentProtocol(),
  };
  const errors: object[] = [];
  const listener = {
    observe(message: nsIConsoleMessage) {
      try {
        const e = message.QueryInterface?.(Ci.nsIScriptError);
        if (!e) return;
        errors.push({
          source: e.sourceName,
          message: e.errorMessage,
          line: e.lineNumber,
        });
      } catch {
        /* Non-script console messages or the not-yet-correlated title are not diagnostic errors. */
      }
    },
    QueryInterface: ChromeUtils.generateQI(["nsIConsoleListener"]),
  };
  Services.console.registerListener(listener);
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    await wait(
      () => ExtensionParent.GlobalManager.extensionMap.has(gecko),
      5000,
    );
    const extension = ExtensionParent.GlobalManager.extensionMap.get(gecko) as
      | NativeURLExtension
      | undefined;
    if (!extension) throw Error("Authored extension did not install");
    row.actualNativeBase = extension.baseURI.spec;
    await wait(
      () => extension.backgroundContext?.viewType === "background_worker",
      5000,
    );
    row.context = extension.backgroundContext?.viewType;
    row.parentProtocolAfter = parentProtocol();
    tab = win.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    if (!await wait(() => tab?.linkedBrowser.contentTitle === "ready", 5000)) {
      throw Error("Control DOC did not load");
    }
    tab.linkedBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          `content.postMessage(${JSON.stringify({ token })},'*')`,
        ),
      false,
    );
    row.responseObserved = await wait(() => {
      try {
        const value = JSON.parse(
          tab!.linkedBrowser.contentTitle,
        ) as WorkerOwnImportReply;
        if (value.token === token && value.phase === "complete") {
          row.reply = value;
          return true;
        }
      } catch {
        /* Non-script console messages or the not-yet-correlated title are not diagnostic errors. */
      }
      return false;
    }, 5000);
    if (!row.responseObserved) {
      try {
        row.partialTitle = JSON.parse(tab.linkedBrowser.contentTitle);
      } catch {
        row.partialTitle = tab.linkedBrowser.contentTitle;
      }
    }
    return row;
  } catch (error) {
    row.caseError = String(error);
    return row;
  } finally {
    Services.console.unregisterListener(listener);
    row.consoleErrors = errors;
    if (tab) win.gBrowser.removeTab(tab);
    await addon?.uninstall();
    await IOUtils.remove(path);
  }
}
export async function runAllTests() {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  if (
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED !== true ||
    Services.prefs.getBoolPref(
        "extensions.backgroundServiceWorker.enabled",
        false,
      ) !== false
  ) {
    throw Error(
      "Actual compiled per-extension-only native Worker policy required",
    );
  }
  const output = Services.env.get(
    "FLOORP_PUBLIC_URL_STARTUP_DIAGNOSTIC_OUTPUT",
  );
  const report: WorkerOwnImportReport = {
    scope:
      "Authored compiled Worker own-resource import regression; no production qualification",
    qualified: false,
    parentProtocol: parentProtocol(),
    rows: [],
  };
  const foreignPath = PathUtils.join(
    PathUtils.tempDir,
    "owned-foreign-import-" + Services.uuid.generateUUID().toString() + ".xpi",
  );
  await IOUtils.write(
    foreignPath,
    zipSync(Object.fromEntries(
      Object.entries({
        "manifest.json": JSON.stringify({
          manifest_version: 2,
          name: "Authored real foreign private import owner",
          version: "1.0",
          browser_specific_settings: {
            gecko: {
              id: "p".repeat(32) + "@chromium-extension",
              floorp_chrome_extension: {
                schema_version: 1,
                original_id: "p".repeat(32),
              },
            },
          },
        }),
        "private.js": "self.foreignPrivateImportExecuted=true;",
      }).map(([name, value]) => [name, new TextEncoder().encode(value)]),
    )),
  );
  let foreign: WorkerOwnImportAddon | undefined;
  try {
    foreign = await AddonManager.installTemporaryAddon(
      new FileUtils.File(foreignPath),
    );
    for (
      const [kind, mode, letter] of [
        ["classic", "relative", "a"],
        ["classic", "public", "b"],
        ["module", "relative", "c"],
        ["module", "public", "d"],
        ["module", "mixed", "e"],
      ] as const
    ) {
      report.rows.push(await importCase(kind, mode, letter));
      if (output) await IOUtils.writeJSON(output, report);
    }
    for (const row of report.rows) {
      if (
        row.context !== "background_worker" || row.caseError ||
        row.reply?.error || row.reply?.value?.startup?.ok !== true ||
        row.reply.value.startup.value !== "real-authored-import" ||
        row.reply.value.id !== row.id ||
        row.reply.resource?.status !== 200 ||
        !row.reply.value.location?.startsWith(
          row.actualNativeBase || "<missing native base>",
        )
      ) {
        throw Error(
          "Real native own-resource import failed: " + JSON.stringify(row),
        );
      }
      if (
        row.mode === "mixed" &&
        (row.reply.value.startup.sameNamespace !== true ||
          row.reply.value.startup.evaluations !== 1)
      ) {
        throw Error(
          "Relative+public imports must share one actual module namespace/evaluation: " +
            JSON.stringify(row),
        );
      }
      if (
        row.kind === "classic" &&
        (row.reply.value.startup.foreignPrivateExecuted !== false ||
          row.reply.value.startup.deniedImports?.length !== 3 ||
          row.reply.value.startup.deniedImports.some((value) =>
            value.denied !== true
          ))
      ) {
        throw Error(
          "Foreign/credential/port imports must remain denied: " +
            JSON.stringify(row),
        );
      }
    }
    report.observationCompleted = true;
    if (output) await IOUtils.writeJSON(output, report);
    console.log(
      "OWNED_PUBLIC_URL_STARTUP_OBSERVATIONS",
      JSON.stringify(report),
    );
  } finally {
    await foreign?.uninstall();
    await IOUtils.remove(foreignPath);
  }
}
