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

import type {
  CommandResult,
  Fixture,
  FixtureExtension,
} from "./helpers/messaging-identity-test-types.ts";

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
    return { event, value: value.value, senderId: sender.id, senderUrl: sender.url, senderOrigin: sender.origin, ownId: browser.runtime.id };
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
    let invocationReturned = false;
    try {
      let result;
      const payload = { value: 'roundtrip' };
      if (command.operation === 'inspect') {
        const url = browser.runtime.getURL('resource.txt');
        result = { id: browser.runtime.id, url, resource: await (await fetch(url)).text() };
      } else if (command.operation === 'message') {
        const returned = command.target === null
          ? browser.runtime.sendMessage(payload)
          : browser.runtime.sendMessage(command.target, payload);
        invocationReturned = true;
        result = await returned;
      } else if (command.operation === 'port') {
        const port = command.target === null
          ? browser.runtime.connect({ name: 'identity-fixture' })
          : browser.runtime.connect(command.target, { name: 'identity-fixture' });
        invocationReturned = true;
        result = await new Promise((resolve, reject) => {
          port.onMessage.addListener(value => { resolve(value); port.disconnect(); });
          port.onDisconnect.addListener(() => reject(
            new Error(browser.runtime.lastError?.message || port.error?.message || 'Port disconnected')));
          port.postMessage(payload);
        });
      }
      document.title = JSON.stringify({ requestId: command.requestId, result });
    } catch (error) {
      document.title = JSON.stringify({ requestId: command.requestId, error: String(error), errorName: error.name, synchronousThrow: !invocationReturned });
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
    const fixture = { id: addonId, compatible, extension, addon, path };
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
    const from = fixtures.find((fixture) =>
      fixture.compatible
        ? `${senderId}@chromium-extension` === fixture.id
        : senderId === fixture.id
    );
    const to = fixtures.find((fixture) =>
      fixture.compatible
        ? `${ownId}@chromium-extension` === fixture.id
        : ownId === fixture.id
    );
    assert(from && to, "Both real sender/receiver fixtures are installed");
    const nativeUrl =
      from.extension.backgroundContext!.xulBrowser.currentURI.spec;
    const sourceURI = Services.io.newURI(nativeUrl);
    const expectedUrl = from.compatible && to.compatible
      ? `chrome-extension://${senderId}${sourceURI.pathQueryRef}`
      : nativeUrl;
    assertEquals(
      reply.result?.senderUrl,
      expectedUrl,
      "DOC message and Port sender URL presentation is receiver scoped",
    );
    assertEquals(
      reply.result?.senderOrigin,
      from.extension.backgroundContext!.xulBrowser.browsingContext
        .currentWindowGlobal.documentPrincipal.originNoSuffix,
      "Actual sender principal origin remains native",
    );
  }
  function denied(reply: CommandResult): void {
    assert(
      reply.error?.includes("Receiving end does not exist"),
      `Expected connection denial, got ${JSON.stringify(reply)}`,
    );
  }
  function invalidChromeTarget(
    reply: CommandResult,
    target: string,
    operation: "message" | "port",
  ): void {
    assertEquals(
      reply.errorName,
      "TypeError",
      "invalid Chrome target throws TypeError",
    );
    assertEquals(
      reply.synchronousThrow,
      true,
      "target validation rejects before returning",
    );
    const signature = operation === "message"
      ? "runtime.sendMessage(optional string extensionId, any message, optional object options, optional function callback)"
      : "runtime.connect(optional string extensionId, optional object connectInfo)";
    assertEquals(
      reply.error,
      `TypeError: Error in invocation of ${signature}: Invalid extension id: '${target}'`,
      "invalid Chrome target matches the genuine invocation error",
    );
    assert(
      !reply.result,
      "invalid target never delivers a message or Port reply",
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
      inspected.result?.url === `chrome-extension://${a}/resource.txt`,
      "Trusted Chrome getURL exposes its public alias",
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
    assert(
      (await command(firefox, "inspect")).result?.url.startsWith(
        "moz-extension://",
      ),
      "Ordinary Firefox public getURL remains native",
    );
    for (const operation of ["message", "port"] as const) {
      success(await command(source, operation), "internal", a, a);
      success(await command(source, operation, a), "internal", a, a);
      success(await command(source, operation, b), "external", a, b);
      invalidChromeTarget(
        await command(source, operation, allowed.id),
        allowed.id,
        operation,
      );
      success(await command(allowed, operation, a), "external", b, a);
      success(
        await command(firefox, operation, source.id),
        "external",
        firefox.id,
        a,
      );
      invalidChromeTarget(
        await command(source, operation, firefox.id),
        firefox.id,
        operation,
      );
      success(
        await command(firefox, operation),
        "internal",
        firefox.id,
        firefox.id,
      );
      denied(await command(source, operation, c));
      invalidChromeTarget(
        await command(source, operation, deniedTarget.id),
        deniedTarget.id,
        operation,
      );
      denied(await command(source, operation, d));
      denied(await command(fakeSource, operation, allowed.id));
      denied(await command(firefox, operation, b));
      invalidChromeTarget(
        await command(source, operation, source.id),
        source.id,
        operation,
      );
      invalidChromeTarget(
        await command(source, operation, fakeTarget.id),
        fakeTarget.id,
        operation,
      );
      success(
        await command(firefox, operation, fakeTarget.id),
        "external",
        firefox.id,
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
