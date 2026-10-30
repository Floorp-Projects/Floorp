// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type {
  WorldExtensionParent,
  WorldMessageManager,
  WorldReport,
  WorldWindow,
} from "./helpers/custom-event-detail-test-types.ts";

import { assertDetailContract } from "./helpers/custom-event-detail-assertions.ts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function until<T>(
  read: () => T | false | undefined,
  label: string,
): Promise<T> {
  const end = Date.now() + 12000;
  while (Date.now() < end) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error("Timed out observing " + label);
}
async function readObservation(
  manager: WorldMessageManager,
  expectedUri: string,
): Promise<WorldReport> {
  const name = "owned-world-report-" + Services.uuid.generateUUID();
  return await new Promise<WorldReport>((resolve, reject) => {
    const finish = () => {
      clearTimeout(timer);
      manager.removeMessageListener(name, listener);
    };
    const listener = (
      { data }: { data: { uri: string; json: string | null } },
    ) => {
      finish();
      try {
        assert(
          data.uri === expectedUri && typeof data.json === "string" &&
            data.json.length <= 262144,
          "exact owned document and bounded scalar JSON",
        );
        resolve(JSON.parse(data.json) as WorldReport);
      } catch (error) {
        reject(error);
      }
    };
    const timer = setTimeout(() => {
      finish();
      reject(new Error("Owned document observation message timed out"));
    }, 5000);
    manager.addMessageListener(name, listener);
    try {
      manager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            `sendAsyncMessage(${
              JSON.stringify(name)
            },{uri:content.document.documentURI,json:content.document.getElementById("world-observation-json")?.textContent??null});`,
          ),
        false,
      );
    } catch (error) {
      finish();
      reject(error);
    }
  });
}

async function observe(): Promise<void> {
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as WorldWindow;
  const base = new URL(
    "custom-event-detail/",
    win.wrappedJSObject.__floorpRuntimeFixtureURL,
  ).href;
  const output = PathUtils.join(
    PathUtils.tempDir,
    "floorp-chrome-custom-event-detail-native-result.json",
  );
  assert(
    /^http:\/\/127\.0\.0\.1:\d+\/custom-event-detail\/$/.test(base),
    "exact owned loopback fixture origin required",
  );
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  ) as { AppConstants: { MOZ_WEBEXT_WEBIDL_ENABLED: boolean } };
  assertEquals(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    true,
    "real native bindings compiled",
  );
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "global Worker preference remains false",
  );
  const programResponse = await fetch(base + "probe.js");
  assert(programResponse.ok, "owned shared observation program available");
  const program = await programResponse.text();
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as { ExtensionParent: WorldExtensionParent };
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const cases: object[] = [];
  try {
    for (
      const [family, manifestVersion] of [
        ["chrome", 2],
        ["chrome", 3],
        ["firefox", 2],
        ["firefox", 3],
      ] as const
    ) {
      const originalId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaf";
      const id = family === "chrome"
        ? originalId + "@chromium-extension"
        : "custom-event-detail@ordinary-firefox.invalid";
      let addon: { uninstall(): Promise<void> } | undefined;
      let path: string | undefined;
      let tab: ReturnType<WorldWindow["gBrowser"]["addTab"]> | undefined;
      try {
        {
          path = PathUtils.join(
            PathUtils.tempDir,
            `custom-event-detail-${Services.uuid.generateUUID()}.xpi`,
          );
          const manifest = {
            manifest_version: manifestVersion,
            name: "Authored CustomEvent detail regression",
            version: "1.0",
            content_scripts: [{
              matches: ["http://127.0.0.1/*"],
              js: ["probe.js"],
              run_at: "document_idle",
              all_frames: true,
            }],
            browser_specific_settings: {
              gecko: {
                id,
                ...(family === "chrome"
                  ? {
                    floorp_chrome_extension: {
                      schema_version: 1,
                      original_id: originalId,
                    },
                  }
                  : {}),
              },
            },
          };
          await IOUtils.write(
            path,
            zipSync({
              "manifest.json": new TextEncoder().encode(
                JSON.stringify(manifest),
              ),
              "probe.js": new TextEncoder().encode(program),
            }, {}),
          );
          addon = await AddonManager.installTemporaryAddon(
            new FileUtils.File(path),
          );
          await until(
            () => ExtensionParent.GlobalManager.extensionMap.get(id),
            family + " genuine extension startup",
          );
        }
        const url = base + "page.html?extension=1";
        tab = win.gBrowser.addTab(url, { triggeringPrincipal: principal });
        win.gBrowser.selectedTab = tab;
        const target = tab;
        await until(() => {
          if (target.linkedBrowser.currentURI.spec !== url) return false;
          return /^detail-contract-(?:complete|incomplete)$/.test(
            target.linkedBrowser.contentTitle,
          );
        }, family + " actual top+iframe observations");
        const observation = await readObservation(
          target.linkedBrowser.messageManager as WorldMessageManager,
          url,
        );
        const policy = WebExtensionPolicy.getByID(id) as unknown as {
          backgroundServiceWorkerAllowed: boolean;
        } | null;
        cases.push({
          family,
          manifestVersion,
          addonId: id,
          trustedChromePolicy: policy?.backgroundServiceWorkerAllowed ?? null,
          observation,
        });
        assert(
          observation.observationComplete,
          "both real public detail contracts were collected",
        );
        assertEquals(
          policy?.backgroundServiceWorkerAllowed,
          family === "chrome",
          "validated Chrome versus ordinary Firefox policy",
        );
        for (
          const value of [
            observation.observation.top,
            observation.observation.frame,
          ]
        ) {
          assertEquals(
            value.content.extensionId,
            family === "chrome" ? originalId : id,
            "exact real runtime identity",
          );
          assert(
            value.token && value.token === value.page.token &&
              value.token === value.content.token,
            "same owned document token",
          );
          assertEquals(
            value.page.events.length,
            6,
            "six actual page listener invocations across three payload arms",
          );
          assertEquals(
            value.content.payloads.length,
            3,
            "three original payload kinds",
          );
          assertEquals(
            value.content.payloads.map((payload) => payload.kind).join(","),
            "data,accessor,functions",
            "fixed actual source dispatch order",
          );
          assertEquals(
            value.page.events.map((event) => `${event.kind}:${event.phase}`)
              .join(","),
            "data:1,data:2,accessor:1,accessor:2,functions:1,functions:2",
            "fixed actual page observation order",
          );
          for (const payload of value.content.payloads) {
            assertEquals(
              payload.snapshots.length,
              2,
              "two actual content post-dispatch snapshots per kind",
            );
            assertEquals(
              payload.sourceEvents.length,
              2,
              "two actual source-world listener invocations per kind",
            );
          }
          assertEquals(
            value.page.unexposedGlobals.globalSecretType,
            "undefined",
            "unexported content global stays private from direct page access",
          );
          assertEquals(
            value.page.unexposedGlobals.windowSecretType,
            "undefined",
            "unexported Xray expando stays private from direct page access",
          );
          assertEquals(
            value.content.pageGlobalMarkerType,
            "undefined",
            "page JS global remains separated from isolated content scope",
          );
          assertDetailContract(value, family === "chrome");
        }
      } finally {
        if (tab) win.gBrowser.removeTab(tab, { skipPermitUnload: true });
        await addon?.uninstall();
        if (path) await IOUtils.remove(path, { ignoreAbsent: true });
      }
    }
  } finally {
    await IOUtils.writeJSON(output, {
      componentRegression: true,
      buildId: Services.appinfo.appBuildID,
      compiledNativeBindings: AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
      globalWorkerPreference: WebExtensionPolicy.backgroundServiceWorkerEnabled,
      cases,
    });
  }
}
export async function runAllTests(): Promise<void> {
  await runTests("ChromeCustomEventDetailNative.test.mts", [{
    name:
      "Chrome public detail clones preserve data contracts and ordinary Firefox boundaries",
    fn: observe,
  }]);
}
