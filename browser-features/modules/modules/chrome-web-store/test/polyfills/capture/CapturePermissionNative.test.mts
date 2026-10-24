// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
import type {
  ComponentAddonManager,
  ComponentExtensionParent,
  ComponentNativeWindow,
} from "../../helpers/capture-component-test-types.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function waitFor<T>(
  read: () => T | undefined | false | null,
  description: string,
): Promise<T> {
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${description}`);
}

async function testCameraPermission(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: ComponentAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: ComponentExtensionParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as ComponentNativeWindow;
  const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  const target = win.gBrowser.addTab("about:blank", {
    triggeringPrincipal: systemPrincipal,
  });
  win.gBrowser.selectedTab = target;
  target.linkedBrowser.fixupAndLoadURIString(
    "data:text/html,<h1>Authorized capture target</h1>",
    { triggeringPrincipal: systemPrincipal },
  );
  await waitFor(
    () => target.linkedBrowser.currentURI.spec.startsWith("data:"),
    "capture target",
  );

  const originalId = "ccddeeccaabbccddeeccaabbccddeecc";
  const id = `${originalId}@chromium-extension`;
  const manifest = {
    manifest_version: 3,
    name: "Camera consent regression fixture",
    version: "1.0",
    permissions: ["tabCapture", "offscreen"],
    action: {},
    background: { scripts: ["background.js"] },
    browser_specific_settings: {
      gecko: {
        id,
        floorp_chrome_extension: { schema_version: 1, original_id: originalId },
      },
    },
  };
  const files = {
    "manifest.json": JSON.stringify(manifest),
    "background.js": 'document.title="background-ready";',
    "offscreen.html": "<!doctype html><title>offscreen-ready</title>",
    "camera.html": '<!doctype html><script src="camera.js"></script>',
    "camera.js": `
      document.title="camera-ready";
      window.addEventListener("message", async event => {
        if(event.data!=="request-camera") return;
        document.title="camera-pending";
        try {
          const stream=await navigator.mediaDevices.getUserMedia({video:true,audio:false});
          stream.getTracks().forEach(track=>track.stop());
          document.title="CAMERA-BYPASS";
        } catch(error) {
          document.title=JSON.stringify({name:error.name,message:error.message});
        }
      });
    `,
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-capture-camera-${Services.uuid.generateUUID()}.xpi`,
  );
  await IOUtils.write(
    path,
    zipSync(Object.fromEntries(
      Object.entries(files).map(([name, text]) => [
        name,
        new TextEncoder().encode(text),
      ]),
    )),
  );
  const prefs = new Map([
    ["media.navigator.streams.fake", true],
    ["media.navigator.permission.fake", true],
    ["media.navigator.permission.disabled", false],
  ]);
  const previous = new Map([...prefs].map(([name]) => [
    name,
    Services.prefs.prefHasUserValue(name)
      ? Services.prefs.getBoolPref(name)
      : undefined,
  ]));
  let addon: { uninstall(): Promise<void> } | undefined;
  let cameraTab: typeof target | undefined;
  try {
    for (const [name, value] of prefs) Services.prefs.setBoolPref(name, value);
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "extension startup",
    );
    const background = await waitFor(
      () => extension.backgroundContext,
      "extension background",
    );
    const offscreenModule = await extension.apiManager.asyncGetAPI(
      "offscreen",
      extension,
      "addon_parent",
    );
    await offscreenModule.getAPI(background).offscreen.createDocument({
      url: "offscreen.html",
      reasons: ["USER_MEDIA"],
      justification: "Verify capture backend permission cannot grant camera",
    });
    const offscreen = await waitFor(
      () => [...extension.views].find((view) => view.viewType === "offscreen"),
      "offscreen consumer",
    );
    cameraTab = win.gBrowser.addTab(extension.baseURI.resolve("camera.html"), {
      triggeringPrincipal: systemPrincipal,
    });
    await waitFor(
      () => cameraTab?.linkedBrowser.contentTitle === "camera-ready",
      "ordinary camera document",
    );
    const principal = cameraTab.linkedBrowser.browsingContext
      .currentWindowGlobal.documentPrincipal;
    assert(
      principal.equals(
        offscreen.browsingContext.currentWindowContext.documentPrincipal,
      ),
      "camera request and capture backend grant have the same principal",
    );
    Services.perms.removeFromPrincipal(principal, "camera");
    Services.perms.removeFromPrincipal(principal, "MediaManagerVideo");

    const action = await extension.apiManager.asyncGetAPI(
      "browserAction",
      extension,
      "addon_parent",
    );
    const captureModule = await extension.apiManager.asyncGetAPI(
      "tabCapture",
      extension,
      "addon_parent",
    );
    const capture = captureModule.getAPI(offscreen).tabCapture;
    action.action.triggerClickOrPopup(target);
    const token = await offscreen.withCallContextData(
      { isHandlingUserInput: true },
      () => capture.getMediaStreamId(),
    );
    const { sessionId } = await capture._consumeStreamId(token);
    assertEquals(
      Services.perms.testExactPermissionFromPrincipal(
        principal,
        "MediaManagerVideo",
      ),
      Services.perms.ALLOW_ACTION,
      "authorized capture installs a pending one-shot backend grant",
    );
    assertEquals(
      Services.perms.testExactPermissionFromPrincipal(principal, "camera"),
      Services.perms.UNKNOWN_ACTION,
      "backend grant does not install camera consent",
    );

    win.gBrowser.selectedTab = cameraTab;
    cameraTab.linkedBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent('content.postMessage("request-camera", "*");'),
      false,
    );
    const notification = await waitFor(() => {
      const title = cameraTab?.linkedBrowser.contentTitle;
      if (title !== "camera-pending" && title !== "camera-ready") {
        throw new Error(`Camera request skipped native consent: ${title}`);
      }
      return win.PopupNotifications.getNotification(
        "webRTC-shareDevices",
        cameraTab?.linkedBrowser,
      );
    }, "native camera consent prompt with fake hardware");
    assertEquals(
      cameraTab.linkedBrowser.contentTitle,
      "camera-pending",
      "ordinary camera stream remains unavailable before consent",
    );
    notification.secondaryActions[0].callback({ checkboxChecked: false });
    win.PopupNotifications.remove(notification);
    const result = await waitFor(() => {
      const title = cameraTab?.linkedBrowser.contentTitle;
      return title?.startsWith("{")
        ? JSON.parse(title) as {
          name: string;
          message: string;
        }
        : false;
    }, "native camera denial");
    assertEquals(
      result.name,
      "NotAllowedError",
      "explicit native camera consent denial",
    );
    assertEquals(
      Services.perms.testExactPermissionFromPrincipal(principal, "camera"),
      Services.perms.UNKNOWN_ACTION,
      "capture never grants persistent camera access",
    );
    await capture._streamStopped(sessionId);
  } finally {
    if (cameraTab) win.gBrowser.removeTab(cameraTab);
    await addon?.uninstall();
    win.gBrowser.removeTab(target);
    await IOUtils.remove(path, { ignoreAbsent: true });
    for (const [name, value] of previous) {
      if (value === undefined) Services.prefs.clearUserPref(name);
      else Services.prefs.setBoolPref(name, value);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("CapturePermissionNative.test.mts", [{
    name:
      "pending tab capture backend grant cannot bypass native camera consent",
    fn: testCameraPermission,
  }]);
}
