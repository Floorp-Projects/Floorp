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

async function testNativeProgrammaticClose(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const id = "ef".repeat(16), geckoId = `${id}@chromium-extension`;
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
        name: "Actual native notification close contract",
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
    const title = "Floorp authored native programmatic close";
    const icon =
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAL0lEQVR4nO3OIQEAAAgDMFIRjehYiHEzMb/q2UsqAQEBAQEBAQEBAQEBAQGBdOABkcmElw3yDhMAAAAASUVORK5CYII=";
    const options = {
      type: "basic",
      title,
      message: "Before real native clear",
      iconUrl: icon,
      requireInteraction: true,
    };
    assertEquals(
      await invoke("create", "actual-clear", options),
      "actual-clear",
      "actual alert create succeeds",
    );
    const activeAlert = await waitFor(
      () =>
        alerts().find((win) =>
          win.document.querySelector("#alertTitleLabel")?.getAttribute(
              "value",
            ) === title && win.gIsActive
        ),
      "real active native alert before clear",
    );
    assertEquals(
      await invoke("clear", "actual-clear"),
      true,
      "clear accepts the active native notification",
    );
    assertEquals(
      await invoke("clear", "actual-clear"),
      false,
      "a repeated clear sees a pending closure as absent",
    );
    assertEquals(
      JSON.stringify(await invoke("getAll")),
      "{}",
      "a closing notification is absent from the public inventory",
    );
    await waitFor(
      () =>
        activeAlert.closed &&
        state()?.closed.some((value) =>
          value.id === "actual-clear" && !value.byUser
        ),
      "real native asynchronous finish delivers programmatic close",
    );
    assertEquals(
      JSON.stringify(state()?.closed),
      JSON.stringify([{ id: "actual-clear", byUser: false }]),
      "native programmatic close emits exactly one event with byUser false",
    );

    assertEquals(
      await invoke("create", "actual-replacement", {
        ...options,
        message: "Original version",
      }),
      "actual-replacement",
      "replacement source alert is created",
    );
    const oldAlert = await waitFor(
      () =>
        alerts().find((win) =>
          win.document.querySelector("#alertTextLabel")?.textContent ===
            "Original version" && win.gIsActive
        ),
      "actual original native alert",
    );
    assertEquals(
      await invoke("update", "actual-replacement", {
        message: "Current version",
      }),
      true,
      "actual native notification replacement succeeds",
    );
    const currentAlert = await waitFor(
      () =>
        alerts().find((win) =>
          win.document.querySelector("#alertTextLabel")?.textContent ===
            "Current version" && win.gIsActive
        ),
      "actual replacement native alert",
    );
    await waitFor(
      () => oldAlert.closed,
      "old native alert finishes after replacement",
    );
    assertEquals(
      JSON.stringify(await invoke("getAll")),
      JSON.stringify({ "actual-replacement": true }),
      "late old-instance callback preserves the current replacement",
    );
    assertEquals(
      state()?.closed.length,
      1,
      "replacement does not emit a close for the new instance",
    );
    assertEquals(
      await invoke("clear", "actual-replacement"),
      true,
      "current native replacement can be cleared",
    );
    await waitFor(
      () => currentAlert.closed && state()?.closed.length === 2,
      "current replacement receives its real native close callback",
    );
    assertEquals(
      JSON.stringify(state()?.closed),
      JSON.stringify([{ id: "actual-clear", byUser: false }, {
        id: "actual-replacement",
        byUser: false,
      }]),
      "each current native notification closes exactly once",
    );
    assertEquals(
      JSON.stringify(await invoke("getAll")),
      "{}",
      "native callback removes the last current notification",
    );
  } finally {
    await addon?.uninstall();
    for (const alert of alerts()) {
      if (
        alert.document.querySelector("#alertTitleLabel")?.getAttribute(
          "value",
        ) === "Floorp authored native programmatic close"
      ) alert.close();
    }
    await IOUtils.remove(path, { ignoreAbsent: true });
    if (hadPref) Services.prefs.setBoolPref("alerts.useSystemBackend", oldPref);
    else Services.prefs.clearUserPref("alerts.useSystemBackend");
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("Chrome notifications native asynchronous close", [{
    name:
      "Real native close event, exactly-once delivery and replacement ownership",
    fn: testNativeProgrammaticClose,
  }]);
}
