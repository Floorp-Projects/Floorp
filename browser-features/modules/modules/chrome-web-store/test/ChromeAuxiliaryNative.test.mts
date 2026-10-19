// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import type {
  AlertWindow,
  NotificationState,
} from "./helpers/debugger-auxiliary-types.ts";
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function waitFor<T>(
  read: () => T | false | undefined,
  description: string,
): Promise<T> {
  const end = Date.now() + 20000;
  while (Date.now() < end) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${description}`);
}

async function testNativeNotificationButtons(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const id = "cd".repeat(16), geckoId = `${id}@chromium-extension`;
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-notifications-${Services.uuid.generateUUID()}.xpi`,
  );
  const hadPref = Services.prefs.prefHasUserValue("alerts.useSystemBackend");
  const oldPref = Services.prefs.getBoolPref("alerts.useSystemBackend");
  Services.prefs.setBoolPref("alerts.useSystemBackend", false);
  const encode = (text: string) => new TextEncoder().encode(text);
  await IOUtils.write(
    path,
    zipSync({
      "manifest.json": encode(JSON.stringify({
        manifest_version: 3,
        name: "Actual native notification controls",
        version: "1.0",
        permissions: ["notifications"],
        background: { scripts: ["background.js"] },
        browser_specific_settings: {
          gecko: {
            id: geckoId,
            floorp_chrome_extension: { schema_version: 1, original_id: id },
          },
        },
      })),
      "background.js": encode(`
      const state={ready:true,buttons:[],closed:[]};
      const publish=()=>document.title=JSON.stringify(state);
      chrome.notifications.onButtonClicked.addListener((id,index)=>{state.buttons.push({id,index});publish()});
      chrome.notifications.onClosed.addListener((id,byUser)=>{state.closed.push({id,byUser});publish()});
      window.addEventListener("message",async({data})=>{
        if(!data?.floorpNotificationsFixture)return;
        try{state.value=await browser.notifications[data.method](...data.args);delete state.error}
        catch(error){state.error=String(error)}
        state.sequence=data.sequence;publish();
      });publish();
    `),
    }),
  );
  let addon;
  const alerts = () =>
    [...Services.wm.getEnumerator("alert:alert")] as unknown as AlertWindow[];
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(geckoId),
      "notification extension startup",
    );
    const state = (): NotificationState | undefined => {
      try {
        return JSON.parse(
          extension.backgroundContext?.xulBrowser.contentTitle,
        ) as NotificationState;
      } catch {
        return undefined;
      }
    };
    await waitFor(() => state()?.ready, "notification listeners");
    let sequence = 0;
    const invoke = async (method: string, ...args: unknown[]) => {
      const current = ++sequence;
      extension.backgroundContext.xulBrowser.messageManager.loadFrameScript(
        `data:application/javascript,${
          encodeURIComponent(
            `content.postMessage(${
              JSON.stringify({
                floorpNotificationsFixture: true,
                sequence: current,
                method,
                args,
              })
            },"*")`,
          )
        }`,
        false,
      );
      const result = await waitFor(
        () => state()?.sequence === current && state(),
        `${method} native result`,
      );
      assert(!result.error, result.error || "notification call succeeds");
      return result.value;
    };
    const icon =
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==";
    assertEquals(
      await invoke("create", "actual-buttons", {
        type: "basic",
        title: "Floorp authored notification controls",
        message: "Before actual update",
        iconUrl: icon,
        requireInteraction: true,
        buttons: [{ title: "First native action" }, {
          title: "Second native action",
        }],
      }),
      "actual-buttons",
      "native notification is created with its public id",
    );
    let alert = await waitFor(
      () =>
        alerts().find((win) =>
          win.document.querySelector("#alertTitleLabel")?.getAttribute(
              "value",
            ) === "Floorp authored notification controls" && win.gIsActive
        ),
      "native browser alert window",
    );
    assert(
      alert.gRequireInteraction,
      "requireInteraction reaches the native alert",
    );
    assertEquals(
      alert.arguments.length,
      13,
      "compiled nsXULAlerts passes native nsIAlertAction controls",
    );
    assertEquals(
      alert.document.querySelectorAll("#alertActions button").length,
      2,
      "real native alert buttons render",
    );
    assertEquals(
      await invoke("update", "actual-buttons", {
        message: "After actual update",
      }),
      true,
      "native notification update succeeds",
    );
    alert = await waitFor(
      () =>
        alerts().find((win) =>
          win.document.querySelector("#alertTextLabel")?.textContent ===
            "After actual update" && win.gIsActive
        ),
      "replacement native alert text",
    );
    assertEquals(
      state()?.closed.length,
      0,
      "replacement does not emit an unrelated closed event",
    );
    await extension.terminateBackground();
    assert(
      !extension.backgroundContext,
      "background is genuinely stopped before notification action",
    );
    const button = alert.document.querySelectorAll(
      "#alertActions button",
    )[1] as unknown as { doCommand(): void };
    button.doCommand();
    const awakened = await waitFor(
      () => Boolean(state()?.buttons.length) && state(),
      "native notification button wakes stopped background",
    );
    assertEquals(
      JSON.stringify(awakened.buttons),
      JSON.stringify([{ id: "actual-buttons", index: 1 }]),
      "native nsIAlertAction routes its exact extension/id/index",
    );
    await waitFor(
      () =>
        state()?.closed.some((value) =>
          value.id === "actual-buttons" && value.byUser
        ),
      "native user-action close event",
    );
    assertEquals(
      JSON.stringify(await invoke("getAll")),
      "{}",
      "native closed notification is removed",
    );
  } finally {
    await addon?.uninstall();
    for (const alert of alerts()) {
      if (
        alert.document.querySelector("#alertTitleLabel")?.getAttribute(
          "value",
        ) === "Floorp authored notification controls"
      ) alert.close();
    }
    await IOUtils.remove(path, { ignoreAbsent: true });
    if (hadPref) Services.prefs.setBoolPref("alerts.useSystemBackend", oldPref);
    else Services.prefs.clearUserPref("alerts.useSystemBackend");
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("Chrome auxiliary native alert controls", [{
    name:
      "Compiled native alert controls, replacement and stopped-background events",
    fn: testNativeNotificationButtons,
  }]);
}
