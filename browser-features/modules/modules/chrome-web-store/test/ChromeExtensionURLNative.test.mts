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
  result?: { url: string; body: string; id: string };
  error?: string;
};
type Browser = {
  contentTitle: string;
  messageManager: { loadFrameScript(script: string, delayed: boolean): void };
  browsingContext: {
    currentWindowGlobal?: { documentPrincipal: nsIPrincipal };
  };
};
type Tab = { linkedBrowser: Browser };
type Host = {
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  gBrowser: {
    addTab(url: string, options: { triggeringPrincipal: nsIPrincipal }): Tab;
    removeTab(tab: Tab): void;
  };
};
type Extension = { backgroundContext?: { xulBrowser: Browser }; uuid: string };
const ID = "f".repeat(32);

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
async function command(browser: Browser, target: string): Promise<Reply> {
  const requestId = ++sequence;
  const data = {
    floorpWebPageFixture: true,
    requestId,
    operation: "fetch",
    target,
  };
  browser.messageManager.loadFrameScript(
    `data:application/javascript,${encodeURIComponent(
      `content.postMessage(${JSON.stringify(data)}, '*');`,
    )}`,
    false,
  );
  return await waitFor(() => {
    try {
      const reply = JSON.parse(browser.contentTitle) as Reply;
      return reply.requestId === requestId ? reply : false;
    } catch {
      return false;
    }
  }, "native URL fetch result");
}

async function testExtensionAlias(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const host = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as Host;
  const manifest = {
    manifest_version: 3,
    name: "Native Chrome extension URL alias fixture",
    version: "1.0",
    background: { scripts: ["background.js"] },
    web_accessible_resources: [
      {
        resources: ["public.txt", "page.html", "page.js"],
        matches: ["http://127.0.0.1/*"],
      },
    ],
    browser_specific_settings: {
      gecko: {
        id: `${ID}@chromium-extension`,
        floorp_chrome_extension: { schema_version: 1, original_id: ID },
      },
    },
  };
  const background = `
    window.addEventListener('message', async event => {
      const data = event.data;
      if (!data?.floorpWebPageFixture) return;
      try {
        const response = await fetch(data.target);
        document.title = JSON.stringify({ requestId: data.requestId,
          result: { url: response.url, body: await response.text(), id: browser.runtime.id } });
      } catch (error) { document.title = JSON.stringify({ requestId: data.requestId, error: String(error) }); }
    });
    document.title = JSON.stringify({ ready: true });
  `;
  const encode = (value: string) => new TextEncoder().encode(value);
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-url-alias-${Services.uuid.generateUUID()}.xpi`,
  );
  await IOUtils.write(
    path,
    zipSync({
      "manifest.json": encode(JSON.stringify(manifest)),
      "background.js": encode(background),
      "private.txt": encode("private extension resource"),
      "public.txt": encode("public extension resource"),
      "page.html": encode('<!doctype html><script src="page.js"></script>'),
      "page.js": encode(
        "document.title = JSON.stringify({url: location.href, id: browser.runtime.id});",
      ),
    }),
  );
  const tabs: Tab[] = [];
  const addon = await AddonManager.installTemporaryAddon(
    new FileUtils.File(path),
  );
  try {
    const extensions = ExtensionParent.GlobalManager.extensionMap as Map<
      string,
      Extension
    >;
    const extension = await waitFor(
      () => extensions.get(`${ID}@chromium-extension`),
      "native alias extension",
    );
    await waitFor(
      () =>
        extension.backgroundContext?.xulBrowser.contentTitle.includes(
          '"ready":true',
        ),
      "background ready",
    );
    const own = await command(
      extension.backgroundContext!.xulBrowser,
      `chrome-extension://${ID}/private.txt?preserved=yes#fragment`,
    );
    assert(
      !own.error,
      own.error ??
        "Extension can fetch its own private resource through Chrome URL",
    );
    assertEquals(
      own.result?.body,
      "private extension resource",
      "Alias resolves actual package data",
    );
    assertEquals(
      own.result?.id,
      ID,
      "Runtime public identity remains original Chrome ID",
    );
    assertEquals(
      own.result?.url,
      `chrome-extension://${ID}/private.txt?preserved=yes`,
      "Fetch preserves the requested alias without the fragment",
    );
    async function page(hostname: string): Promise<Tab> {
      const url = new URL(
        "webpage-messaging.html",
        host.wrappedJSObject.__floorpRuntimeFixtureURL,
      );
      url.hostname = hostname;
      const tab = host.gBrowser.addTab(url.href, {
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      });
      tabs.push(tab);
      await waitFor(
        () => tab.linkedBrowser.contentTitle.includes('"ready":true'),
        "real external HTTP page",
      );
      return tab;
    }
    const external = await page("127.0.0.1");
    const exposed = await command(
      external.linkedBrowser,
      `chrome-extension://${ID}/public.txt`,
    );
    assert(
      !exposed.error,
      exposed.error ?? "WAR permits the declared HTTP origin",
    );
    assertEquals(
      exposed.result?.body,
      "public extension resource",
      "WAR resource can be read",
    );
    const privateResource = await command(
      external.linkedBrowser,
      `chrome-extension://${ID}/private.txt`,
    );
    assert(
      !!privateResource.error,
      "A webpage cannot bypass private extension resources using an alias",
    );
    const foreign = await page("localhost");
    assert(
      !!(
        await command(
          foreign.linkedBrowser,
          `chrome-extension://${ID}/public.txt`,
        )
      ).error,
      "WAR origin match remains enforced",
    );
    for (const invalid of [
      `chrome-extension://${"e".repeat(32)}/public.txt`,
      `chrome-extension://user@${ID}/public.txt`,
      `chrome-extension://${ID}:1234/public.txt`,
    ])
      assert(
        !!(await command(external.linkedBrowser, invalid)).error,
        `Uninstalled or malformed alias rejected: ${invalid}`,
      );
    const documentTab = host.gBrowser.addTab(
      `chrome-extension://${ID}/page.html`,
      {
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      },
    );
    tabs.push(documentTab);
    const title = await waitFor(
      () =>
        documentTab.linkedBrowser.contentTitle.startsWith('{"url":') &&
        documentTab.linkedBrowser.contentTitle,
      "canonical extension document executes native API",
    );
    const document = JSON.parse(title) as { url: string; id: string };
    assertEquals(
      document.url,
      `moz-extension://${extension.uuid}/page.html`,
      "Document location uses a single native origin",
    );
    assertEquals(
      document.id,
      ID,
      "Native extension API belongs to the actual package",
    );
    assertEquals(
      documentTab.linkedBrowser.browsingContext.currentWindowGlobal
        ?.documentPrincipal.addonId,
      `${ID}@chromium-extension`,
      "Document principal retains the actual Gecko addon identity",
    );
  } finally {
    for (const tab of tabs.reverse()) host.gBrowser.removeTab(tab);
    await addon.uninstall();
    await IOUtils.remove(path);
  }
}

export async function runAllTests(): Promise<void> {
  try {
    await runTests("Native Chrome extension URL aliases", [
      {
        name: "Chrome URL input resolves one native origin while enforcing actual WAR boundaries",
        fn: testExtensionAlias,
      },
    ]);
  } catch (error) {
    for (const message of Services.console.getMessageArray().slice(-40))
      console.error(message.message);
    throw error;
  }
}
