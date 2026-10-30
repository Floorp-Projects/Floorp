// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  assertNotEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";

import type { NativeOffscreenObservation } from "./native-offscreen-test-types.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

type ContextInfo = {
  contextId: string;
  documentId: string;
  contextType: string;
  documentUrl: string;
};
type PageContext = {
  viewType: string;
  unloaded: boolean;
  xulBrowser: {
    contentTitle: string;
    messageManager: {
      loadFrameScript(script: string, delayed: boolean): void;
    };
  };
};
type NativeExtension = {
  views: Set<PageContext>;
  backgroundContext: unknown;
  backgroundState: string;
  terminateBackground(): Promise<void>;
  apiManager: {
    asyncGetAPI(
      name: string,
      extension: NativeExtension,
      scope: string,
    ): Promise<{
      getAPI(context: unknown): {
        offscreen: {
          createDocument(options: {
            url: string;
            reasons: string[];
            justification: string;
          }): Promise<void>;
          closeDocument(): Promise<void>;
          hasDocument(): Promise<boolean>;
        };
        runtime: {
          getContexts(filter: Record<string, unknown>): ContextInfo[];
        };
      };
    }>;
  };
};

async function waitFor<T>(
  read: () => T | undefined | false,
  description: string,
): Promise<T> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  throw new Error(`Timed out waiting for ${description}`);
}

async function testNativeOffscreen(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const id = "ponmlkjihgfedcbaponmlkjihgfedcba";
  const geckoId = `${id}@chromium-extension`;
  const manifest = {
    manifest_version: 3,
    name: "Floorp native offscreen regression fixture",
    version: "1.0",
    permissions: ["offscreen", "storage"],
    background: { scripts: ["background.js"] },
    browser_specific_settings: {
      gecko: {
        id: geckoId,
        floorp_chrome_extension: { schema_version: 1, original_id: id },
      },
    },
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-offscreen-${Services.uuid.generateUUID()}.xpi`,
  );
  const encode = (text: string) => new TextEncoder().encode(text);
  const observationScript = `
    // Observe both root surfaces before any namespace getter can remove itself.
    const namespaces = ["browser", "chrome"].map(api => {
      const root = globalThis[api];
      return {
        api,
        keys: Reflect.ownKeys(root).map(String),
        entries: ["runtime", "tabs", "storage", "windows", "offscreen"].map(name => ({
          name,
          inRoot: name in root,
          own: Object.hasOwn(root, name),
          descriptor: Object.getOwnPropertyDescriptor(root, name) !== undefined
        }))
      };
    });
    const exposed = () => ({
      namespaces,
      storage: typeof browser.storage,
      runtime: typeof browser.runtime.sendMessage,
      background: typeof browser.runtime.getBackgroundPage,
      chromeStorage: typeof chrome.storage,
      chromeRuntime: typeof chrome.runtime.sendMessage,
      chromeBackground: typeof chrome.runtime.getBackgroundPage
    });
  `;
  const pageScript = observationScript + `
    const iframe = document.createElement("iframe");
    iframe.src = "frame.html";
    iframe.onload = () => {
      document.title = JSON.stringify({main: exposed(), frame: JSON.parse(iframe.contentDocument.title)});
    };
    document.body.appendChild(iframe);
    window.addEventListener("message", async event => {
      if (event.data === "wake-background") {
        const response = await browser.runtime.sendMessage("offscreen-ping");
        const port = browser.runtime.connect({ name: "offscreen-port" });
        const portResponse = await new Promise(resolve => {
          port.onMessage.addListener(resolve);
          port.postMessage("ping");
        });
        port.disconnect();
        document.title = response + "/" + portResponse;
      }
    });
  `;
  const xpi = zipSync({
    "manifest.json": encode(JSON.stringify(manifest)),
    "background.js": encode(`
      browser.runtime.onMessage.addListener(message => {
        if (message === "offscreen-ping") return Promise.resolve("offscreen-pong");
      });
      browser.runtime.onConnect.addListener(port => {
        port.onMessage.addListener(() => port.postMessage("offscreen-port-pong"));
      });
    `),
    "offscreen.html": encode(
      '<!doctype html><title>loading</title><body><script src="offscreen.js"></script>',
    ),
    "offscreen.js": encode(pageScript),
    "frame.html": encode(
      '<!doctype html><script src="frame.js"></script>',
    ),
    "frame.js": encode(
      observationScript + `
      document.title = JSON.stringify(exposed());
    `,
    ),
  });
  await IOUtils.write(path, xpi);
  let addon: { uninstall(): Promise<void> } | undefined;
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor<NativeExtension>(
      () => ExtensionParent.GlobalManager.extensionMap.get(geckoId),
      "fixture startup",
    );
    await waitFor(() => extension.backgroundContext, "background startup");
    const offscreenModule = await extension.apiManager.asyncGetAPI(
      "offscreen",
      extension,
      "addon_parent",
    );
    const api = offscreenModule.getAPI(extension.backgroundContext).offscreen;
    assert(
      api,
      "native offscreen API should be available to converted fixtures",
    );
    const runtimeModule = await extension.apiManager.asyncGetAPI(
      "runtime",
      extension,
      "addon_parent",
    );
    const runtime = runtimeModule.getAPI(extension.backgroundContext).runtime;
    const options = {
      url: "offscreen.html",
      reasons: ["DOM_PARSER"],
      justification: "Verify native independent host and API bindings",
    };
    await api.createDocument(options);
    assertEquals(await api.hasDocument(), true, "native host exists");
    const page = await waitFor(
      () => [...extension.views].find((view) => view.viewType === "offscreen"),
      "real offscreen context",
    );
    await waitFor(
      () =>
        page.xulBrowser.contentTitle?.startsWith("{main") ||
        page.xulBrowser.contentTitle?.startsWith('{"main"'),
      "offscreen and iframe scripts",
    );
    const exposed = JSON.parse(page.xulBrowser.contentTitle) as {
      main: NativeOffscreenObservation;
      frame: NativeOffscreenObservation;
    };
    for (const context of [exposed.main, exposed.frame]) {
      assertEquals(
        JSON.stringify(context.namespaces.map((root) => root.api)),
        '["browser","chrome"]',
        "Both real API root surfaces were observed before namespace reads",
      );
      for (const root of context.namespaces) {
        assert(
          root.keys.includes("runtime") &&
            root.keys.every((name) => name === "runtime" || name === "test"),
          root.api + " exposes only allowed offscreen root namespace keys",
        );
        for (const entry of root.entries) {
          const allowed = entry.name === "runtime";
          assertEquals(
            entry.inRoot,
            allowed,
            root.api + "." + entry.name +
              " namespace presence before first read",
          );
          assertEquals(
            entry.own,
            allowed,
            root.api + "." + entry.name + " own property before first read",
          );
          assertEquals(
            entry.descriptor,
            allowed,
            root.api + "." + entry.name + " own descriptor before first read",
          );
        }
      }
      assertEquals(context.storage, "undefined", "storage API is unavailable");
      assertEquals(
        context.runtime,
        "function",
        "runtime messaging is available",
      );
      assertEquals(
        context.background,
        "undefined",
        "background window is unavailable",
      );
      assertEquals(
        context.chromeStorage,
        "undefined",
        "Chrome storage API is unavailable",
      );
      assertEquals(
        context.chromeRuntime,
        "function",
        "Chrome runtime messaging is available",
      );
      assertEquals(
        context.chromeBackground,
        "undefined",
        "Chrome background window is unavailable",
      );
    }
    const contexts = runtime.getContexts({
      contextTypes: ["OFFSCREEN_DOCUMENT"],
    });
    assert(
      contexts.length > 0,
      "native getContexts returns the offscreen document",
    );
    const mainContext = contexts.find((entry) =>
      entry.documentUrl.endsWith("offscreen.html")
    );
    assert(mainContext, "top level native document has an ID");
    assertEquals(
      runtime.getContexts({ contextIds: [mainContext.contextId] }).length,
      1,
      "public contextId can be used as a filter",
    );
    await extension.terminateBackground();
    assertEquals(
      extension.backgroundState,
      "stopped",
      "background really terminated",
    );
    assertEquals(
      await api.hasDocument(),
      true,
      "host survives background termination",
    );
    assertEquals(
      page.unloaded,
      false,
      "offscreen actual context remains alive",
    );
    page.xulBrowser.messageManager.loadFrameScript(
      "data:application/javascript," + encodeURIComponent(
        'content.postMessage("wake-background", "*");',
      ),
      false,
    );
    await waitFor(
      () =>
        page.xulBrowser.contentTitle === "offscreen-pong/offscreen-port-pong",
      "offscreen message waking the background and returning a response",
    );
    assertEquals(
      extension.backgroundState,
      "running",
      "real message restarts background",
    );
    assertEquals(
      runtime.getContexts({ documentIds: [mainContext.documentId] }).length,
      1,
      "same offscreen document survives event delivery and restart",
    );
    await api.closeDocument();
    assertEquals(
      runtime.getContexts({ contextTypes: ["OFFSCREEN_DOCUMENT"] }).length,
      0,
      "await closeDocument completes native context teardown",
    );
    await waitFor(() => page.unloaded, "native context cleanup");
    assertEquals(await api.hasDocument(), false, "host closed");
    assertEquals(
      runtime.getContexts({ contextTypes: ["OFFSCREEN_DOCUMENT"] }).length,
      0,
      "closed offscreen contexts disappear",
    );
    await api.createDocument(options);
    const replacement = runtime.getContexts({
      contextTypes: ["OFFSCREEN_DOCUMENT"],
    })
      .find((entry) => entry.documentUrl.endsWith("offscreen.html"));
    assert(replacement, "replacement context exists");
    assertNotEquals(
      replacement.documentId,
      mainContext.documentId,
      "replacement has a new native ID",
    );
    assert(addon, "temporary extension remains installed before shutdown");
    await addon.uninstall();
    addon = undefined;
    assertEquals(
      await api.hasDocument(),
      false,
      "extension shutdown closes host",
    );
  } finally {
    await addon?.uninstall();
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("OffscreenNative.test.mts", [{
    name:
      "native offscreen survives real background shutdown and message wakeup",
    fn: testNativeOffscreen,
  }]);
}
