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

type FixtureContext = {
  xulBrowser: {
    contentTitle: string;
    messageManager: { loadFrameScript(script: string, delayed: boolean): void };
  };
};
type FixtureExtension = { backgroundContext?: FixtureContext };
type Fixture = {
  id: string;
  extension: FixtureExtension;
  addon: { uninstall(): Promise<void> };
  path: string;
};
type Response = {
  event: string;
  senderId: string;
  ownId: string;
  value: string;
};
type CommandResult = {
  requestId: number;
  result?: Response & { id: string; url: string; resource: string };
  error?: string;
};

async function waitFor<T>(
  read: () => T | undefined | false,
  name: string,
): Promise<T> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${name}`);
}

const receiver = `
  function reply(event, value, sender) {
    return { event, value: value.value, senderId: sender.id, ownId: browser.runtime.id };
  }
  browser.runtime.onMessage.addListener((value, sender) =>
    Promise.resolve(reply('internal', value, sender)));
  browser.runtime.onMessageExternal.addListener((value, sender) =>
    Promise.resolve(reply('external', value, sender)));
  for (const [event, api] of [['internal', browser.runtime.onConnect],
                             ['external', browser.runtime.onConnectExternal]]) {
    api.addListener(port => {
      port.onMessage.addListener(value => port.postMessage(reply(event, value, port.sender)));
    });
  }
`;
const background = `
  window.addEventListener('message', async event => {
    const command = event.data;
    if (!command || command.floorpMessagingTest !== true) return;
    try {
      let result;
      const payload = { value: 'roundtrip' };
      if (command.operation === 'inspect') {
        const url = browser.runtime.getURL('resource.txt');
        result = { id: browser.runtime.id, url, resource: await (await fetch(url)).text() };
      } else if (command.operation === 'message') {
        result = command.target === null
          ? await browser.runtime.sendMessage(payload)
          : await browser.runtime.sendMessage(command.target, payload);
      } else if (command.operation === 'port') {
        const port = command.target === null
          ? browser.runtime.connect({ name: 'identity-fixture' })
          : browser.runtime.connect(command.target, { name: 'identity-fixture' });
        result = await new Promise((resolve, reject) => {
          port.onMessage.addListener(value => { resolve(value); port.disconnect(); });
          port.onDisconnect.addListener(() => reject(
            new Error(browser.runtime.lastError?.message || port.error?.message || 'Port disconnected')));
          port.postMessage(payload);
        });
      }
      document.title = JSON.stringify({ requestId: command.requestId, result });
    } catch (error) {
      document.title = JSON.stringify({ requestId: command.requestId, error: String(error) });
    }
  });
  const frame = document.createElement('iframe');
  frame.src = 'receiver.html';
  frame.onload = () => { document.title = JSON.stringify({ ready: true }); };
  document.body.appendChild(frame);
`;

async function testInstalledMessaging(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const registry = ExtensionParent.GlobalManager.extensionMap as Map<
    string,
    FixtureExtension
  >;
  const fixtures: Fixture[] = [];
  const a = "a".repeat(32);
  const b = "b".repeat(32);
  const c = "c".repeat(32);
  const d = "d".repeat(32);
  const e = "e".repeat(32);
  const gecko = (id: string) => `${id}@chromium-extension`;
  const encode = (value: string) => new TextEncoder().encode(value);
  async function install(
    id: string,
    compatible: boolean,
    connectable?: Record<string, unknown>,
  ): Promise<Fixture> {
    const addonId = compatible ? gecko(id) : id;
    const manifest = {
      manifest_version: 3,
      name: `Floorp messaging fixture ${addonId}`,
      version: "1.0",
      background: { scripts: ["background.js"] },
      ...(connectable ? { externally_connectable: connectable } : {}),
      browser_specific_settings: {
        gecko: {
          id: addonId,
          ...(compatible
            ? {
              floorp_chrome_extension: { schema_version: 1, original_id: id },
            }
            : {}),
        },
      },
    };
    const path = PathUtils.join(
      PathUtils.tempDir,
      `floorp-messaging-${Services.uuid.generateUUID()}.xpi`,
    );
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": encode(JSON.stringify(manifest)),
        "background.js": encode(background),
        "receiver.html": encode(
          '<!doctype html><script src="receiver.js"></script>',
        ),
        "receiver.js": encode(receiver),
        "resource.txt": encode("native moz-extension resource"),
      }),
    );
    const addon = await AddonManager.installTemporaryAddon(
      new FileUtils.File(path),
    );
    const extension = await waitFor(
      () => registry.get(addonId),
      "installed extension",
    );
    const fixture = { id: addonId, extension, addon, path };
    fixtures.push(fixture);
    await waitFor(
      () =>
        extension.backgroundContext?.xulBrowser.contentTitle.includes(
          '"ready":true',
        ),
      "fixture receiver ready",
    );
    return fixture;
  }
  let sequence = 0;
  async function command(
    fixture: Fixture,
    operation: string,
    target: string | null = null,
  ): Promise<CommandResult> {
    const requestId = ++sequence;
    const data = { floorpMessagingTest: true, requestId, operation, target };
    fixture.extension.backgroundContext!.xulBrowser.messageManager
      .loadFrameScript(
        `data:application/javascript,${
          encodeURIComponent(
            `content.postMessage(${JSON.stringify(data)}, '*');`,
          )
        }`,
        false,
      );
    return await waitFor(() => {
      try {
        const result = JSON.parse(
          fixture.extension.backgroundContext!.xulBrowser.contentTitle,
        ) as CommandResult;
        return result.requestId === requestId ? result : false;
      } catch {
        return false;
      }
    }, `${operation} response`);
  }
  function success(
    reply: CommandResult,
    event: string,
    senderId: string,
    ownId: string,
  ): void {
    assert(!reply.error, reply.error ?? "message succeeded");
    assertEquals(reply.result?.event, event, "correct internal/external event");
    assertEquals(
      reply.result?.senderId,
      senderId,
      "sender public ID is scoped to receiver",
    );
    assertEquals(reply.result?.ownId, ownId, "receiver public ID");
    assertEquals(reply.result?.value, "roundtrip", "payload round trip");
  }
  function denied(reply: CommandResult): void {
    assert(
      reply.error?.includes("Receiving end does not exist"),
      `Expected connection denial, got ${JSON.stringify(reply)}`,
    );
  }
  try {
    const source = await install(a, true);
    const allowed = await install(b, true, { ids: [a, gecko(e)] });
    const deniedTarget = await install(c, true, {
      matches: ["https://example.com/*"],
    });
    const fakeTarget = await install(gecko(d), false);
    const fakeSource = await install(gecko(e), false);
    const firefox = await install("ordinary@floorp.tests", false);
    const inspected = await command(source, "inspect");
    assertEquals(
      inspected.result?.id,
      a,
      "runtime.id preserves original Chrome ID",
    );
    assert(
      inspected.result?.url.startsWith("moz-extension://"),
      "getURL keeps the native resolver",
    );
    assertEquals(
      inspected.result?.resource,
      "native moz-extension resource",
      "getURL resource actually loads",
    );
    assertEquals(
      (await command(firefox, "inspect")).result?.id,
      firefox.id,
      "Firefox runtime.id unchanged",
    );
    for (const operation of ["message", "port"]) {
      success(await command(source, operation), "internal", a, a);
      success(await command(source, operation, a), "internal", a, a);
      success(await command(source, operation, b), "external", a, b);
      success(await command(source, operation, allowed.id), "external", a, b);
      success(await command(allowed, operation, a), "external", b, a);
      success(
        await command(firefox, operation, source.id),
        "external",
        firefox.id,
        a,
      );
      success(
        await command(source, operation, firefox.id),
        "external",
        source.id,
        firefox.id,
      );
      success(
        await command(firefox, operation),
        "internal",
        firefox.id,
        firefox.id,
      );
      denied(await command(source, operation, c));
      denied(await command(source, operation, deniedTarget.id));
      denied(await command(source, operation, d));
      denied(await command(fakeSource, operation, allowed.id));
      denied(await command(firefox, operation, b));
      success(
        await command(source, operation, fakeTarget.id),
        "external",
        source.id,
        fakeTarget.id,
      );
    }
  } finally {
    for (const fixture of fixtures.reverse()) {
      await fixture.addon.uninstall();
      await IOUtils.remove(fixture.path);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("Chrome extension identity and messaging", [
    {
      name:
        "installed extensions preserve scoped IDs and enforce message/Port allowlists",
      fn: testInstalledMessaging,
    },
  ]);
}
