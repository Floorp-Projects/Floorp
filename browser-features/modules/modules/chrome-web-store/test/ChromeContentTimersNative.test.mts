// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type { ComponentAddonManager } from "./helpers/capture-component-test-types.ts";
import { contentTimersProbeSource } from "./helpers/content-timers-probe.mjs";
import type {
  ContentTimerMode,
  ContentTimerPageResult,
  ContentTimerWindow,
} from "./helpers/content-timers-test-types.ts";
import type { TtsValidationParent } from "./helpers/tts-validation-test-types.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const original = "ccccccccccccccccdddddddddddddddd";
function fixtureId(mode: ContentTimerMode): string {
  return mode === "converted"
    ? original
    : "content-timers-" + mode + "@example.test";
}
async function until<Value>(
  read: () => Value | false | undefined,
  label: string,
): Promise<Value> {
  const end = Date.now() + 15000;
  while (Date.now() < end) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  throw Error("Timed out waiting for " + label);
}
async function runSharedDocument(): Promise<ContentTimerPageResult> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: ComponentAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: TtsValidationParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as ContentTimerWindow;
  const fixture = window.wrappedJSObject.__floorpRuntimeFixtureURL;
  assert(fixture, "the browser runner provides its owned HTTP fixture");
  const modes: ContentTimerMode[] = ["converted", "ordinary", "mismatched"];
  const addons: Awaited<
    ReturnType<ComponentAddonManager["installTemporaryAddon"]>
  >[] = [];
  const paths: string[] = [];
  let tab: ReturnType<ContentTimerWindow["gBrowser"]["addTab"]> | undefined;
  try {
    for (const mode of modes) {
      const id = mode === "converted"
        ? original + "@chromium-extension"
        : fixtureId(mode);
      const manifest = {
        manifest_version: 3,
        name: "Authored content timers " + mode,
        version: "1",
        content_scripts: [{
          matches: ["http://127.0.0.1/*"],
          js: ["probe.js"],
          run_at: "document_end",
        }],
        browser_specific_settings: {
          gecko: {
            id,
            ...(mode !== "ordinary"
              ? {
                floorp_chrome_extension: {
                  schema_version: 1,
                  original_id: original,
                },
              }
              : {}),
          },
        },
      };
      const path = PathUtils.join(
        PathUtils.tempDir,
        "content-timers-" + Services.uuid.generateUUID() + ".xpi",
      );
      paths.push(path);
      await IOUtils.write(
        path,
        zipSync({
          "manifest.json": new TextEncoder().encode(JSON.stringify(manifest)),
          "probe.js": new TextEncoder().encode(contentTimersProbeSource),
        }),
      );
      addons.push(
        await AddonManager.installTemporaryAddon(new FileUtils.File(path)),
      );
      await until(
        () => ExtensionParent.GlobalManager.extensionMap.get(id),
        "real content-script installation " + mode,
      );
    }
    tab = window.gBrowser.addTab(
      fixture + "content-timers-target.html?convertedId=" + original,
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    const browser = tab.linkedBrowser;
    const result = await until(() => {
      const prefix = "timer-contracts:";
      if (!browser.contentTitle.startsWith(prefix)) return false;
      const page = JSON.parse(
        browser.contentTitle.slice(prefix.length),
      ) as ContentTimerPageResult;
      return modes.every((mode) => page.results[fixtureId(mode)]) && page;
    }, "all three real extension content worlds in the same document");
    console.log("[ChromeContentTimersNative]", JSON.stringify(result));
    return result;
  } finally {
    try {
      await Promise.all(addons.map((addon) => addon.uninstall()));
    } finally {
      if (tab?.isConnected) window.gBrowser.removeTab(tab);
      await Promise.all(
        paths.map((path) => IOUtils.remove(path, { ignoreAbsent: true })),
      );
    }
  }
}
let sharedResult: Promise<ContentTimerPageResult> | undefined;
function results(): Promise<ContentTimerPageResult> {
  return sharedResult ??= runSharedDocument();
}
async function checkTimers(mode: ContentTimerMode): Promise<void> {
  const result = (await results()).results[fixtureId(mode)];
  const converted = mode === "converted";
  assertEquals(result.realm, "isolated-content-script", "actual fixture realm");
  for (const receiver of ["undefined", "null", "object", "htmlDDA"] as const) {
    for (const [method, call] of Object.entries(result.calls[receiver])) {
      const allowed = converted &&
        (receiver === "undefined" || receiver === "null");
      assertEquals(
        call.succeeded,
        allowed,
        mode + " " + receiver + " " + method + " matches its measured contract",
      );
      if (allowed && method.startsWith("set")) {
        assertEquals(call.idType, "number", "native timer ID is numeric");
      }
      if (!allowed) {
        assertEquals(call.name, "TypeError", "invalid receiver still rejects");
      }
    }
  }
  for (const [method, shape] of Object.entries(result.timerShapes)) {
    assertEquals(shape.name, method, "native timer function name");
    assertEquals(
      shape.length,
      method.startsWith("set") ? 1 : 0,
      "measured native timer arity",
    );
    assertEquals(
      shape.ownPrototype,
      false,
      "timer has no constructor prototype",
    );
    assertEquals(shape.constructible, false, "timer remains nonconstructible");
    assertEquals(
      shape.constructError,
      "TypeError",
      "timer constructor check rejects",
    );
  }
  if (converted) {
    assert(!result.operationError, "detached timer effects complete");
    assertEquals(
      result.callbackArgument,
      "authored-timer-73195",
      "native callback primitive argument",
    );
    assertEquals(
      result.callbackArgumentIdentity,
      true,
      "native callback preserves object argument identity",
    );
    assertEquals(
      result.callbackThisWindow,
      true,
      "native callback retains its real Window receiver",
    );
    assertEquals(result.counts.timeout, 1, "native timeout fires exactly once");
    assertEquals(
      result.counts.interval,
      1,
      "native interval clears after one callback",
    );
  } else {
    assertEquals(
      result.operationError?.name,
      "TypeError",
      "Firefox content sandbox retains its receiver checks",
    );
    assertEquals(
      result.counts.timeout,
      0,
      "ordinary timer call remains rejected",
    );
    assertEquals(
      result.counts.interval,
      0,
      "ordinary interval call remains rejected",
    );
  }
  assertEquals(
    result.counts.cancelledTimeout,
    0,
    "cancelled timeout never fires",
  );
  assertEquals(
    result.counts.cancelledInterval,
    0,
    "cancelled interval never fires",
  );
}
export function runAllTests(): Promise<void> {
  return runTests("ChromeContentTimersNative.test.mts", [
    {
      name:
        "converted content scripts follow the measured Chrome timer receiver contract",
      fn: () => checkTimers("converted"),
    },
    {
      name:
        "ordinary Firefox content timers retain their native receiver checks in the same document",
      fn: () => checkTimers("ordinary"),
    },
    {
      name:
        "a mismatched compatibility ID retains ordinary Firefox content timers in the same document",
      fn: () => checkTimers("mismatched"),
    },
    {
      name:
        "converted timer bindings leave main page native timer identities unchanged",
      fn: async () => {
        assertEquals(
          (await results()).pageTimersNative,
          true,
          "main page methods remain native even if an isolated context was created before its parser",
        );
        assertEquals(
          (await results()).pageTimersUnchanged,
          true,
          "all four page native methods retain their original identities",
        );
      },
    },
  ]);
}
