// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type {
  WorldCaught,
  WorldExtensionParent,
  WorldGlobalObservation,
  WorldMessageManager,
  WorldReferenceObservation,
  WorldReport,
  WorldWindow,
} from "./helpers/content-worlds-test-types.ts";

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

function assertCapability(
  observation: WorldGlobalObservation,
  ordinaryFirefox: boolean,
): void {
  for (const [name, reference] of Object.entries(observation.references)) {
    const result = name === "window" || name === "globalThis"
      ? reference as WorldReferenceObservation
      : (reference as WorldCaught<WorldReferenceObservation | null>).value;
    // A frame has no nested iframe; top/parent and the top document's iframe
    // are genuine same-origin WindowProxy observations, not substituted objects.
    if (name === "iframe" && !observation.isTop) {
      assertEquals(result, null, "owned child has no nested frame");
      continue;
    }
    assert(result, "real same-origin window observation must succeed");
    const property = result.properties.wrappedJSObject;
    assertEquals(
      property.has.value,
      ordinaryFirefox,
      "waiver availability matches the extension family",
    );
    assertEquals(
      property.read.value?.type,
      ordinaryFirefox ? "object" : "undefined",
      "actual waiver getter availability matches the family",
    );
    const own = ordinaryFirefox && name !== "globalThis";
    assertEquals(
      property.own.value,
      own,
      "waiver own-property visibility matches the native world",
    );
    if (own) {
      const descriptor = property.descriptor.value;
      assert(descriptor, "ordinary Firefox retains its real waiver descriptor");
      assert(
        descriptor.configurable && descriptor.enumerable &&
          descriptor.accessor && descriptor.hasGetter && !descriptor.hasSetter,
        "ordinary Firefox native waiver descriptor remains intact",
      );
    } else {
      assertEquals(
        property.descriptor.value,
        null,
        "no own waiver descriptor is fabricated",
      );
    }
    if (!ordinaryFirefox) {
      const keys = result.ownKeys.value;
      assert(keys, "real own-key enumeration must succeed");
      assert(
        !keys.selected.includes("wrappedJSObject"),
        "waiver is absent from public own keys",
      );
    }
  }
}

async function testContentWorlds(): Promise<void> {
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as WorldWindow;
  const base =
    new URL("content-worlds/", win.wrappedJSObject.__floorpRuntimeFixtureURL)
      .href;
  assert(
    /^http:\/\/127\.0\.0\.1:\d+\/content-worlds\/$/.test(base),
    "exact owned loopback content-world fixture required",
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
      : "world-observation@ordinary-firefox.invalid";
    let addon: { uninstall(): Promise<void> } | undefined;
    let path: string | undefined;
    let tab: ReturnType<WorldWindow["gBrowser"]["addTab"]> | undefined;
    try {
      {
        path = PathUtils.join(
          PathUtils.tempDir,
          `world-observation-${Services.uuid.generateUUID()}.xpi`,
        );
        const manifest = {
          manifest_version: manifestVersion,
          name: "Authored isolated-world observation",
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
        return /^world-observation-(?:complete|incomplete)$/.test(
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
      assert(
        observation.complete,
        "both real top and iframe observations completed",
      );
      assertEquals(
        observation.observation.topPage.publicDomPageMarker,
        "42",
        "same owned top page writes retained",
      );
      assertEquals(
        observation.observation.framePage.publicDomPageMarker,
        "43",
        "same owned child page writes retained",
      );
      assertCapability(observation.observation.topPage, false);
      assertCapability(observation.observation.framePage, false);
      {
        assertEquals(
          policy?.backgroundServiceWorkerAllowed,
          family === "chrome",
          "validated Chrome policy versus ordinary Firefox control with global preference false",
        );
        assert(
          typeof observation.observation.topIsolated?.extensionId ===
              "string" &&
            observation.observation.topIsolated.extensionId.length > 0,
          "actual top content-script runtime identity observed",
        );
        assertEquals(
          observation.observation.topIsolated?.extensionId,
          family === "chrome" ? originalId : id,
          "content script exposes the exact validated runtime identity",
        );
        assertEquals(
          observation.observation.frameIsolated?.extensionId,
          observation.observation.topIsolated.extensionId,
          "same genuine extension owns both isolated contexts",
        );
        assertEquals(
          observation.observation.topIsolated?.publicDomPageMarker,
          "42",
          "public DOM visible in isolated top world",
        );
        assertEquals(
          observation.observation.frameIsolated?.publicDomPageMarker,
          "43",
          "public DOM visible in isolated child world",
        );
        for (
          const isolated of [
            observation.observation.topIsolated,
            observation.observation.frameIsolated,
          ]
        ) {
          assert(isolated, "real isolated document observation required");
          assertCapability(isolated, family === "firefox");
          assert(
            isolated?.privateIsolation.globalWrite &&
              isolated.privateIsolation.windowWrite,
            "real isolated private sentinel writes succeed",
          );
          assertEquals(
            isolated.privateIsolation.pageOwnMarkerType,
            "undefined",
            "page JS globals remain hidden from each isolated realm",
          );
        }
        for (
          const value of Object.values(
            observation.observation.privateMarkersVisibleToPage,
          )
        ) {
          assertEquals(
            value,
            "undefined",
            "isolated global/window expandos do not leak into the real top/iframe page globals",
          );
        }
      }
    } finally {
      if (tab) win.gBrowser.removeTab(tab, { skipPermitUnload: true });
      await addon?.uninstall();
      if (path) await IOUtils.remove(path, { ignoreAbsent: true });
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("ChromeContentWorldsNative.test.mts", [{
    name:
      "Chrome MV2/MV3 omit waiver capability while ordinary Firefox and page isolation remain intact",
    fn: testContentWorlds,
  }]);
}
