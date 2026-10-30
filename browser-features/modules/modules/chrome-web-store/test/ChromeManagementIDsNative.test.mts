// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type {
  NativeCorpusExtensionParent,
  NativeCorpusTab,
  NativeCorpusWindow,
} from "../../../../../tools/chrome-extension-runtime/native-test-types.ts";
import type {
  ManagementIDAddonManager,
  ManagementIDEvent,
  ManagementIDExecution,
  ManagementIDFixture,
  ManagementIDInstallOptions,
  ManagementIDOperation,
  ManagementIDOwnedAddon,
  ManagementIDReply,
  ManagementIDResult,
} from "./helpers/management-id-test-types.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

const operations = `
const managementEvents = [];
for (const event of ['onInstalled','onEnabled','onDisabled','onUninstalled']) {
  chrome.management[event].addListener(value => managementEvents.push({event,value}));
}
async function managementOperation(command) {
  const result = {runtimeId:chrome.runtime.id,
    execution:typeof window === 'undefined' ? 'worker' : 'page'};
  switch (command.operation) {
    case 'inspect':
      result.self = await chrome.management.getSelf();
      result.all = await chrome.management.getAll();
      break;
    case 'get':
      result.info = await chrome.management.get(command.target);
      break;
    case 'setEnabled':
      await chrome.management.setEnabled(command.target, command.enabled);
      result.info = await chrome.management.get(command.target);
      break;
    case 'events': result.events = managementEvents.slice(); break;
    default: throw new Error('Unknown authored management command');
  }
  return result;
}
`;
const workerProgram = operations + `
chrome.runtime.onMessage.addListener(command => {
  if (command?.floorpManagementIDs !== true) return undefined;
  return managementOperation(command);
});
`;
const pageProgram = operations + `
window.addEventListener('message', async ({data:command}) => {
  if (command?.floorpManagementIDs !== true) return;
  try {
    const result = command.source === 'worker'
      ? await chrome.runtime.sendMessage(command)
      : await managementOperation(command);
    document.title = JSON.stringify({commandId:command.commandId,result});
  } catch (error) {
    document.title = JSON.stringify({commandId:command.commandId,error:String(error)});
  }
});
document.title = 'management-ids-control-ready';
`;
const selfProgram = `
window.addEventListener('message', ({data}) => {
  if (data?.floorpManagementSelfUninstall !== true) return;
  document.title = JSON.stringify({uninstallRequested:data.commandId});
  chrome.management.uninstallSelf({showConfirmDialog:false}).catch(error => {
    document.title = JSON.stringify({commandId:data.commandId,error:String(error)});
  });
});
document.title = 'management-ids-self-ready';
`;

async function until<T>(
  read: () =>
    | T
    | false
    | undefined
    | null
    | Promise<T | false | undefined | null>,
  name: string,
): Promise<T> {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw Error(`Timed out waiting for ${name}`);
}

async function managementIDs(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: ManagementIDAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as { ExtensionParent: NativeCorpusExtensionParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "native bindings are compiled",
  );
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "the global experimental Worker switch remains disabled",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as NativeCorpusWindow;
  const fixtures: ManagementIDOwnedAddon[] = [], tabs: NativeCorpusTab[] = [];
  const paths: string[] = [];
  const original = "a".repeat(32), targetID = "b".repeat(32);
  const unmarkedID = "c".repeat(32), mismatchedID = "d".repeat(32);
  const native = (id: string) => `${id}@chromium-extension`;
  const controllerID = native(original), targetNativeID = native(targetID);

  async function install(
    options: ManagementIDInstallOptions,
  ): Promise<ManagementIDFixture> {
    const manifest = {
      manifest_version: 3,
      name: `Authored management ID fixture ${options.id}`,
      version: "1.0",
      permissions: options.controller ? ["management"] : [],
      ...(options.worker
        ? { background: { service_worker: "worker.js" } }
        : {}),
      browser_specific_settings: {
        gecko: {
          id: options.id,
          ...(options.markerId
            ? {
              floorp_chrome_extension: {
                schema_version: 1,
                original_id: options.markerId,
              },
            }
            : {}),
        },
      },
    };
    const path = PathUtils.join(
      PathUtils.tempDir,
      `native-management-ids-${Services.uuid.generateUUID()}.xpi`,
    );
    paths.push(path);
    const encode = (value: string) => new TextEncoder().encode(value);
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": encode(JSON.stringify(manifest)),
        "worker.js": encode(workerProgram),
        "control.html": encode(
          '<!doctype html><script src="control.js"></script>',
        ),
        "control.js": encode(pageProgram),
        "self.html": encode('<!doctype html><script src="self.js"></script>'),
        "self.js": encode(selfProgram),
      }),
    );
    const addon = await AddonManager.installTemporaryAddon(
      new FileUtils.File(path),
    );
    fixtures.push({ id: options.id, addon });
    const fixture: ManagementIDFixture = {
      id: options.id,
      addon,
      path,
      extension: await until(
        () => ExtensionParent.GlobalManager.extensionMap.get(options.id),
        "actual installed addon parent",
      ),
    };
    if (options.worker) {
      await until(
        () =>
          fixture.extension.backgroundContext?.viewType === "background_worker",
        "the actual converted MV3 native Worker",
      );
    }
    if (options.controller) {
      fixture.page = window.gBrowser.addTab(
        fixture.extension.baseURI.resolve("control.html"),
        {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        },
      );
      tabs.push(fixture.page);
      await until(
        () =>
          fixture.page?.linkedBrowser.contentTitle ===
            "management-ids-control-ready",
        "the actual extension control page and event listeners",
      );
    }
    return fixture;
  }

  async function reply(
    fixture: ManagementIDFixture,
    source: ManagementIDExecution,
    operation: ManagementIDOperation,
    target?: string,
    enabled?: boolean,
  ): Promise<ManagementIDReply> {
    assert(fixture.page, "the authored controller has a real extension page");
    const commandId = Services.uuid.generateUUID().toString();
    const data = {
      floorpManagementIDs: true,
      commandId,
      source,
      operation,
      target,
      enabled,
    };
    fixture.page.linkedBrowser.messageManager.loadFrameScript(
      "data:application/javascript," + encodeURIComponent(
        `content.postMessage(${JSON.stringify(data)},'*');`,
      ),
      false,
    );
    return await until(() => {
      const title = fixture.page!.linkedBrowser.contentTitle;
      if (!title.startsWith("{")) return false;
      const value = JSON.parse(title) as ManagementIDReply;
      return value.commandId === commandId && value;
    }, `${source} ${operation} command ${commandId}`);
  }
  async function command(
    fixture: ManagementIDFixture,
    source: ManagementIDExecution,
    operation: ManagementIDOperation,
    target?: string,
    enabled?: boolean,
  ): Promise<ManagementIDResult> {
    const value = await reply(fixture, source, operation, target, enabled);
    assert(!value.error, value.error ?? "the public management call succeeded");
    assert(value.result, "the actual extension returns its public API result");
    assertEquals(
      value.result.execution,
      source,
      "the intended native Worker/page executed the API",
    );
    return value.result;
  }
  async function eventCount(
    fixture: ManagementIDFixture,
    source: ManagementIDExecution,
    event: ManagementIDEvent["event"],
    id: string,
  ): Promise<number> {
    const result = await command(fixture, source, "events");
    return result.events?.filter((value) =>
      value.event === event &&
      (typeof value.value === "string" ? value.value : value.value.id) === id
    ).length ?? 0;
  }
  async function expectEvent(
    fixture: ManagementIDFixture,
    source: ManagementIDExecution,
    event: ManagementIDEvent["event"],
    id: string,
    chrome: boolean,
    previousCount = 0,
  ): Promise<ManagementIDEvent> {
    return await until(async () => {
      const result = await command(fixture, source, "events");
      const matching = result.events?.filter((value) =>
        value.event === event &&
        (typeof value.value === "string" ? value.value : value.value.id) ===
          id &&
        typeof value.value ===
          (event === "onUninstalled" && chrome ? "string" : "object")
      );
      return matching?.[previousCount];
    }, `${source} real ${event} ${id}`);
  }

  const failures: unknown[] = [];
  try {
    const chrome = await install({
      id: controllerID,
      markerId: original,
      controller: true,
      worker: true,
    });
    const firefox = await install({
      id: "ordinary-management-ids@floorp.test",
      controller: true,
    });
    for (const source of ["worker", "page"] as const) {
      const actual = await command(chrome, source, "inspect");
      assertEquals(
        actual.runtimeId,
        original,
        "runtime ID is the original Chrome ID",
      );
      assertEquals(actual.self?.id, original, "getSelf ID equals runtime.id");
      assert(
        actual.all?.some((info) => info.id === original),
        "getAll includes the same public self ID",
      );
      assert(
        !actual.self?.permissions?.some((permission) =>
          permission.startsWith("internal:")
        ),
        "private permissions are absent from converted public info",
      );
    }
    const ordinary = await command(firefox, "page", "inspect");
    assertEquals(
      ordinary.self?.id,
      firefox.id,
      "ordinary Firefox getSelf retains the native ID",
    );
    assertEquals(
      ordinary.runtimeId,
      firefox.id,
      "ordinary Firefox runtime ID remains native",
    );
    assert(
      ordinary.all?.some((info) => info.id === controllerID),
      "ordinary Firefox getAll exposes the converted addon's native ID",
    );
    const target = await install({ id: targetNativeID, markerId: targetID });
    for (const source of ["worker", "page"] as const) {
      await expectEvent(chrome, source, "onInstalled", targetID, true);
      assertEquals(
        (await command(chrome, source, "get", targetID)).info?.id,
        targetID,
        "Chrome get accepts public32 IDs",
      );
    }
    await expectEvent(firefox, "page", "onInstalled", targetNativeID, false);
    assertEquals(
      (await command(firefox, "page", "get", targetNativeID)).info?.id,
      targetNativeID,
      "Firefox get preserves native target IDs",
    );
    assert(
      !!(await reply(firefox, "page", "get", targetID)).error,
      "ordinary Firefox cannot use the converted-only schema branch",
    );
    assert(
      !!(await reply(firefox, "page", "setEnabled", targetNativeID, false))
        .error,
      "ordinary Firefox keeps its native setEnabled restriction",
    );
    assertEquals(
      (await AddonManager.getAddonByID(targetNativeID))?.isActive,
      true,
      "denied Firefox change did not disable the target",
    );

    for (const source of ["worker", "page"] as const) {
      const disabledCounts = {
        worker: await eventCount(chrome, "worker", "onDisabled", targetID),
        page: await eventCount(chrome, "page", "onDisabled", targetID),
        firefox: await eventCount(
          firefox,
          "page",
          "onDisabled",
          targetNativeID,
        ),
      };
      const disabled = await command(
        chrome,
        source,
        "setEnabled",
        targetID,
        false,
      );
      assertEquals(
        disabled.info?.id,
        targetID,
        "disable returns the public target ID",
      );
      assertEquals(
        disabled.info?.enabled,
        false,
        "the actual target is disabled",
      );
      const persisted = await AddonManager.getAddonByID(targetNativeID);
      assertEquals(
        persisted?.userPermissions?.floorpChromeOriginalId,
        targetID,
        "the native persisted hint retains the exact original ID",
      );
      assertEquals(
        persisted?.isActive,
        false,
        "native AddonManager confirms the disabled state",
      );
      assert(
        !WebExtensionPolicy.getByID(targetNativeID),
        "the disabled target has no active policy",
      );
      for (const reader of ["worker", "page"] as const) {
        assertEquals(
          (await command(chrome, reader, "get", targetID)).info?.id,
          targetID,
          "disabled lookup still uses the public ID",
        );
        const listed = (await command(chrome, reader, "inspect")).all?.find((
          info,
        ) => info.id === targetID);
        assertEquals(
          listed?.enabled,
          false,
          "getAll preserves disabled public identity",
        );
        await expectEvent(
          chrome,
          reader,
          "onDisabled",
          targetID,
          true,
          disabledCounts[reader],
        );
      }
      await expectEvent(
        firefox,
        "page",
        "onDisabled",
        targetNativeID,
        false,
        disabledCounts.firefox,
      );
      const enabledCounts = {
        worker: await eventCount(chrome, "worker", "onEnabled", targetID),
        page: await eventCount(chrome, "page", "onEnabled", targetID),
        firefox: await eventCount(firefox, "page", "onEnabled", targetNativeID),
      };
      const opposite = source === "worker" ? "page" : "worker";
      const enabled = await command(
        chrome,
        opposite,
        "setEnabled",
        targetID,
        true,
      );
      assertEquals(
        enabled.info?.enabled,
        true,
        "the public ID re-enables the actual addon",
      );
      assertEquals(
        (await AddonManager.getAddonByID(targetNativeID))?.isActive,
        true,
        "native AddonManager confirms re-enabled state",
      );
      for (const reader of ["worker", "page"] as const) {
        await expectEvent(
          chrome,
          reader,
          "onEnabled",
          targetID,
          true,
          enabledCounts[reader],
        );
      }
      await expectEvent(
        firefox,
        "page",
        "onEnabled",
        targetNativeID,
        false,
        enabledCounts.firefox,
      );
    }

    const unmarked = await install({ id: native(unmarkedID) });
    const mismatch = await install({
      id: native(mismatchedID),
      markerId: "e".repeat(32),
    });
    for (const fixture of [unmarked, mismatch]) {
      assertEquals(
        fixture.addon.userPermissions?.floorpChromeOriginalId,
        undefined,
        "unmarked and mismatched native addons receive no validated public-ID hint",
      );
    }
    for (const source of ["worker", "page"] as const) {
      for (
        const id of [
          unmarkedID,
          mismatchedID,
          "e".repeat(32),
          "A".repeat(32),
          "b".repeat(31),
        ]
      ) {
        assert(
          !!(await reply(chrome, source, "get", id)).error,
          "unmarked, unmatched and malformed public IDs cannot route to a lookalike",
        );
        assert(
          !!(await reply(chrome, source, "setEnabled", id, false)).error,
          "unverified public IDs cannot change a lookalike's native enabled state",
        );
      }
      for (const fixture of [unmarked, mismatch]) {
        assertEquals(
          (await command(chrome, source, "get", fixture.id)).info?.id,
          fixture.id,
          "lookalikes retain their native identity",
        );
        assertEquals(
          (await AddonManager.getAddonByID(fixture.id))?.isActive,
          true,
          "denied alias routing leaves the actual unmarked addon enabled",
        );
      }
    }
    const activeTarget = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(targetNativeID),
      "re-enabled target parent",
    );
    const selfPage = window.gBrowser.addTab(
      activeTarget.baseURI.resolve("self.html"),
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    tabs.push(selfPage);
    await until(
      () => selfPage.linkedBrowser.contentTitle === "management-ids-self-ready",
      "target's own public uninstall page",
    );
    const uninstallCommand = Services.uuid.generateUUID().toString();
    selfPage.linkedBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          `content.postMessage(${
            JSON.stringify({
              floorpManagementSelfUninstall: true,
              commandId: uninstallCommand,
            })
          },'*');`,
        ),
      false,
    );
    await until(async () => {
      if (!(await AddonManager.getAddonByID(target.id))) return true;
      if (window.gBrowser.tabs.includes(selfPage)) {
        const title = selfPage.linkedBrowser.contentTitle;
        if (title.startsWith("{")) {
          const value = JSON.parse(title) as ManagementIDReply;
          if (value.commandId === uninstallCommand && value.error) {
            throw Error(value.error);
          }
        }
      }
      return false;
    }, "the real target's public uninstallSelf completion");
    for (const source of ["worker", "page"] as const) {
      const event = await expectEvent(
        chrome,
        source,
        "onUninstalled",
        targetID,
        true,
      );
      assertEquals(
        event.value,
        targetID,
        "Chrome onUninstalled receives exactly an ID string",
      );
      assert(
        !!(await reply(chrome, source, "get", targetID)).error,
        "an uninstalled public ID no longer resolves",
      );
    }
    const nativeEvent = await expectEvent(
      firefox,
      "page",
      "onUninstalled",
      targetNativeID,
      false,
    );
    assertEquals(
      typeof nativeEvent.value,
      "object",
      "ordinary Firefox uninstall event retains its native object",
    );
    assertEquals(
      typeof nativeEvent.value !== "string" && nativeEvent.value.id,
      targetNativeID,
      "ordinary Firefox event object retains native ID",
    );
  } catch (error) {
    failures.push(error);
  } finally {
    for (const tab of tabs.reverse()) {
      try {
        if (window.gBrowser.tabs.includes(tab)) window.gBrowser.removeTab(tab);
      } catch (error) {
        failures.push(error);
      }
    }
    for (const fixture of fixtures.reverse()) {
      try {
        const current = await AddonManager.getAddonByID(fixture.id);
        if (current) await current.uninstall();
      } catch (error) {
        failures.push(error);
      }
    }
    for (const path of paths) {
      try {
        await IOUtils.remove(path, { ignoreAbsent: true });
      } catch (error) {
        failures.push(error);
      }
    }
  }
  if (failures.length) {
    throw new AggregateError(failures, failures.map(String).join("\n"));
  }
}

export function runAllTests(): Promise<void> {
  return runTests("Chrome management public IDs", [{
    name:
      "actual MV3 Worker and pages preserve management identity and native lifecycle boundaries",
    fn: managementIDs,
  }]);
}
