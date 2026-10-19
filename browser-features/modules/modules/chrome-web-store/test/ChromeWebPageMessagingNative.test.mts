// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
type Reply = {
  requestId: number;
  result?: Record<string, unknown>;
  error?: string;
};
type Browser = {
  contentTitle: string;
  messageManager: { loadFrameScript(script: string, delayed: boolean): void };
  goBack(): void;
  currentURI: { spec: string };
};
type Tab = { linkedBrowser: Browser };
type Host = {
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  gBrowser: {
    addTab(url: string, options: { triggeringPrincipal: nsIPrincipal }): Tab;
    removeTab(tab: Tab): void;
  };
};
type Extension = { backgroundContext?: { xulBrowser: Browser } };

async function waitFor<T>(
  read: () => T | false | undefined,
  description: string,
): Promise<T> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${description}`);
}

let sequence = 0;
async function command(
  browser: Browser,
  operation: string,
  target?: string,
): Promise<Reply> {
  const requestId = ++sequence;
  const data = { floorpWebPageFixture: true, requestId, operation, target };
  browser.messageManager.loadFrameScript(
    `data:application/javascript,${encodeURIComponent(
      `content.postMessage(${JSON.stringify(data)}, '*');`,
    )}`,
    false,
  );
  return await waitFor(() => {
    try {
      const value = JSON.parse(browser.contentTitle) as Reply;
      return value.requestId === requestId ? value : false;
    } catch {
      return false;
    }
  }, `${operation} web page response`);
}

async function testWebPageMessaging(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const extensions = ExtensionParent.GlobalManager.extensionMap as Map<
    string,
    Extension
  >;
  const host = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as Host;
  const addons: { addon: { uninstall(): Promise<void> }; path: string }[] = [];
  const tabs: Tab[] = [];
  const a = "a".repeat(32),
    b = "b".repeat(32),
    c = "c".repeat(32),
    d = "d".repeat(32),
    e = "e".repeat(32);
  const encode = (value: string) => new TextEncoder().encode(value);
  async function install(
    id: string,
    matches?: string[],
    marker = true,
  ): Promise<Extension> {
    const addonId = `${id}@chromium-extension`;
    const manifest = {
      manifest_version: 2,
      name: "Native external web page messaging fixture",
      version: "1.0",
      background: { scripts: ["background.js"] },
      ...(matches ? { externally_connectable: { matches } } : {}),
      browser_specific_settings: {
        gecko: {
          id: addonId,
          ...(marker
            ? {
                floorp_chrome_extension: { schema_version: 1, original_id: id },
              }
            : {}),
        },
      },
    };
    const path = PathUtils.join(
      PathUtils.tempDir,
      `floorp-webpage-${Services.uuid.generateUUID()}.xpi`,
    );
    const background = `
      let disconnected = 0;
      function reply(event, value, sender) {
        return { event, value: value.value, hasId: sender.id !== undefined,
          url: sender.url, origin: sender.origin, frameId: sender.frameId,
          documentId: sender.documentId, tabId: sender.tab?.id };
      }
      browser.runtime.onMessage.addListener((value, sender) => Promise.resolve(reply('internal', value, sender)));
      browser.runtime.onMessageExternal.addListener((value, sender) => Promise.resolve(reply('external', value, sender)));
      browser.runtime.onConnectExternal.addListener(port => {
        port.onMessage.addListener(value => port.postMessage(reply('external', value, port.sender)));
        port.onDisconnect.addListener(() => { disconnected++; document.title = JSON.stringify({ disconnected }); });
      });
      document.title = JSON.stringify({ ready: true });
    `;
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": encode(JSON.stringify(manifest)),
        "background.js": encode(background),
      }),
    );
    const addon = await AddonManager.installTemporaryAddon(
      new FileUtils.File(path),
    );
    addons.push({ addon, path });
    const extension = await waitFor(
      () => extensions.get(addonId),
      "installed receiver",
    );
    await waitFor(
      () =>
        extension.backgroundContext?.xulBrowser.contentTitle.includes(
          '"ready":true',
        ),
      "receiver ready",
    );
    return extension;
  }
  async function page(hostname: string): Promise<Tab> {
    const url = new URL(
      "webpage-messaging.html",
      host.wrappedJSObject.__floorpRuntimeFixtureURL,
    );
    url.hostname = hostname;
    const tab = host.gBrowser.addTab(url.href, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    tabs.push(tab);
    await waitFor(
      () => tab.linkedBrowser.contentTitle.includes('"ready":true'),
      "actual HTTP page ready",
    );
    return tab;
  }
  function denied(reply: Reply): void {
    assert(
      !!reply.error?.includes("Could not establish connection"),
      `Allowlist rejects call: ${JSON.stringify(reply)}`,
    );
  }
  try {
    const receiver = await install(a, ["http://127.0.0.1/*"]);
    await install(b, ["http://localhost/*"]);
    await install(c);
    await install(d, ["http://127.0.0.1/*"], false);
    await install(e, ["http://*/*"]);
    const tab = await page("127.0.0.1");
    const inspect = await command(tab.linkedBrowser, "inspect");
    assert(!inspect.error, inspect.error ?? "Minimal runtime exposed");
    assertEquals(
      inspect.result?.runtime,
      "object",
      "Allowed page receives runtime",
    );
    for (const key of ["native", "listener", "storage"])
      assertEquals(
        inspect.result?.[key],
        "undefined",
        `Page cannot access ${key}`,
      );
    assert(
      !("extensionId" in inspect.result!),
      "Page has no extension identity",
    );
    for (const operation of ["message", "callback", "port"]) {
      const reply = await command(tab.linkedBrowser, operation, a);
      assert(!reply.error, reply.error ?? "External messaging succeeds");
      assertEquals(
        reply.result?.event,
        "external",
        "Native external event is used",
      );
      assertEquals(
        reply.result?.hasId,
        false,
        "A website cannot impersonate an extension sender",
      );
      assertEquals(
        reply.result?.value,
        "webpage",
        "Message payload roundtrips",
      );
      const expected = new URL(
        "webpage-messaging.html",
        host.wrappedJSObject.__floorpRuntimeFixtureURL,
      );
      assertEquals(
        reply.result?.url,
        expected.href,
        "Sender URL comes from the actual WindowGlobal",
      );
      assertEquals(
        reply.result?.origin,
        expected.origin,
        "Sender origin is authenticated",
      );
      assertEquals(
        reply.result?.frameId,
        0,
        "Top-level frame identity is native",
      );
      assert(
        typeof reply.result?.documentId === "string",
        "Native document ID supplied",
      );
      assert(
        typeof reply.result?.tabId === "number",
        "Native tab identity supplied",
      );
      for (const target of [b, c, d, e])
        denied(await command(tab.linkedBrowser, operation, target));
    }
    const other = await page("localhost");
    denied(await command(other.linkedBrowser, "message", a));
    const otherReply = await command(other.linkedBrowser, "message", b);
    assert(
      !otherReply.error,
      otherReply.error ?? "Separately allowed origin succeeds",
    );
    await command(tab.linkedBrowser, "holdPort", a);
    const navigate = new URL(
      "webpage-messaging.html",
      host.wrappedJSObject.__floorpRuntimeFixtureURL,
    );
    navigate.hostname = "localhost";
    tab.linkedBrowser.messageManager.loadFrameScript(
      `data:application/javascript,${encodeURIComponent(
        `content.location.href = ${JSON.stringify(navigate.href)};`,
      )}`,
      false,
    );
    await waitFor(
      () => tab.linkedBrowser.contentTitle.includes('"ready":true'),
      "Cross-origin navigation completes",
    );
    await waitFor(() => {
      try {
        return (
          Number(
            JSON.parse(receiver.backgroundContext!.xulBrowser.contentTitle)
              .disconnected,
          ) >= 2
        );
      } catch {
        return false;
      }
    }, "Native Port disconnects when its document navigates");
    tab.linkedBrowser.goBack();
    navigate.hostname = "127.0.0.1";
    await waitFor(
      () =>
        tab.linkedBrowser.currentURI.spec === navigate.href &&
        tab.linkedBrowser.contentTitle.includes('"ready":true'),
      "Original HTTP document restored",
    );
    const restored = await command(tab.linkedBrowser, "message", a);
    assert(
      !restored.error,
      restored.error ??
        "Restored document receives a fresh authenticated messaging context",
    );
    await command(tab.linkedBrowser, "holdPort", a);
    host.gBrowser.removeTab(tab);
    tabs.splice(tabs.indexOf(tab), 1);
    await waitFor(() => {
      try {
        return (
          Number(
            JSON.parse(receiver.backgroundContext!.xulBrowser.contentTitle)
              .disconnected,
          ) >= 3
        );
      } catch {
        return false;
      }
    }, "Native Port disconnect after owning page closes");
  } finally {
    for (const tab of tabs.reverse()) host.gBrowser.removeTab(tab);
    for (const { addon, path } of addons.reverse()) {
      await addon.uninstall();
      await IOUtils.remove(path);
    }
  }
}

export async function runAllTests(): Promise<void> {
  try {
    await runTests("Native externally_connectable web pages", [
      {
        name: "real HTTP pages use principal-scoped external messages and Ports with native lifecycle",
        fn: testWebPageMessaging,
      },
    ]);
  } catch (error) {
    for (const message of Services.console.getMessageArray().slice(-40))
      console.error(message.message);
    throw error;
  }
}
