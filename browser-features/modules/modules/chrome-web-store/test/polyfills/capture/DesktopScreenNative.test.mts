// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Requires a headed, freshly compiled pickerVersion1 browser and real desktop.
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
import type {
  CaptureAddon,
  CaptureAddonManager,
  CaptureBrowser,
  CaptureDialogWindow,
  CapturePermissionPanel,
  DesktopExtensionParent,
  DesktopNativeWindow,
  DesktopRecording,
  DesktopScreenReport,
  DesktopSelectionStatus,
  DesktopSourceChoice,
  DesktopSourceKind,
  DesktopSourceList,
  NativeDialog,
  NativeDialogList,
} from "../../helpers/capture-native-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function until<T>(
  read: () =>
    | T
    | false
    | undefined
    | null
    | Promise<T | false | undefined | null>,
  label: string,
): Promise<T> {
  const end = Date.now() + 30000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw Error("Timed out waiting for " + label);
}

async function desktopScreen(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: CaptureAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: DesktopExtensionParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow("navigator:browser") as
    | DesktopNativeWindow
    | null;
  assert(window, "browser window exists for actual desktop capture");
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "the production candidate must include compiled extension WebIDL bindings",
  );
  assert(
    !Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo).isHeadless,
    "a real headed desktop is required",
  );
  assertEquals(
    window.navigator.mediaDevices.mozFloorpCapturePickerVersion,
    1,
    "compiled native chooser bridge is required",
  );
  const system = Services.scriptSecurityManager.getSystemPrincipal();
  const target = window.gBrowser.addTab("about:blank", {
    triggeringPrincipal: system,
  });
  window.gBrowser.selectedTab = target;
  const original = "ffffffffffffffffffffffffffffffff",
    id = original + "@chromium-extension";
  const xpi = PathUtils.join(
    PathUtils.tempDir,
    "floorp-desktop-screen-" + Services.uuid.generateUUID() + ".xpi",
  );
  const report: DesktopScreenReport = {
    os: AppConstants.platform,
    selections: [],
    recordings: [],
  };
  let addon: CaptureAddon | undefined;
  const dialogs = () =>
    ([...Services.wm.getEnumerator("")] as unknown as CaptureDialogWindow[])
      .filter(
        (item) =>
          !item.closed &&
          item.document.documentURI ===
            "chrome://global/content/selectDialog.xhtml",
      );
  const post = (
    browser: CaptureBrowser,
    command: { kind: DesktopSourceKind },
  ) =>
    browser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          `content.document.title='WAIT';content.postMessage(${
            JSON.stringify(command)
          },'*')`,
        ),
      false,
    );
  function read<Result>(browser: CaptureBrowser): Promise<Result> {
    return until(() => {
      const title = browser.contentTitle;
      if (title.startsWith("FAIL:")) throw Error(title);
      return title.startsWith("{") && JSON.parse(title) as Result;
    }, "extension desktop capture response");
  }
  try {
    const mediaDirectory = PathUtils.join(
      PathUtils.profileDir,
      "chrome-capture-artifacts",
    );
    await IOUtils.makeDirectory(mediaDirectory, { ignoreExisting: true });
    // A visible, distinct source makes a blank/synthetic stream fail the proof.
    target.linkedBrowser.fixupAndLoadURIString(
      "data:text/html," +
        encodeURIComponent(
          '<!doctype html><title>floorp-desktop-screen-green-source</title><body style="margin:0;background:rgb(0,255,0)"><script>setInterval(()=>document.body.style.background=`rgb(0,${254+Date.now()%2},0)`,80)</script>',
        ),
      { triggeringPrincipal: system },
    );
    await until(
      () =>
        target.linkedBrowser.contentTitle ===
          "floorp-desktop-screen-green-source",
      "visible desktop source",
    );
    const files = {
      "manifest.json": JSON.stringify({
        manifest_version: 3,
        name: "Native screen chooser fixture",
        version: "1.0",
        permissions: ["desktopCapture", "offscreen"],
        background: { scripts: ["background.js"] },
        browser_specific_settings: {
          gecko: {
            id,
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: original,
            },
          },
        },
      }),
      "background.js":
        `window.addEventListener('message',event=>{if(!event.data.kind)return;const kind=event.data.kind;try{const requestId=chrome.desktopCapture.chooseDesktopMedia([kind,'audio'],async(token,options)=>{try{if(chrome.runtime.lastError)throw Error(chrome.runtime.lastError.message);if(!token){document.title=JSON.stringify({phase:'denied'});return;}if(!await chrome.offscreen.hasDocument())await chrome.offscreen.createDocument({url:'offscreen.html',reasons:['USER_MEDIA'],justification:'Record explicitly selected native desktop source'});await chrome.runtime.sendMessage({token,kind,options});document.title=JSON.stringify({phase:'selected',options});}catch(error){document.title='FAIL:'+error;}});document.title=JSON.stringify({phase:'requested',requestId});}catch(error){document.title='FAIL:'+error;}});`,
      "offscreen.html": '<script src="offscreen.js"></script>',
      "offscreen.js":
        `chrome.runtime.onMessage.addListener(async message=>{try{document.title='WAIT';if(message.options.canRequestAudioTrack)throw Error('Unimplemented system audio was advertised');const mandatory={chromeMediaSource:'desktop',chromeMediaSourceId:message.token};const stream=await navigator.mediaDevices.getUserMedia({audio:false,video:{mandatory}});const source=document.createElement('video');source.muted=true;source.srcObject=stream;await source.play();await new Promise(resolve=>setTimeout(resolve,800));const pixels=video=>{const canvas=document.createElement('canvas');canvas.width=160;canvas.height=100;const context=canvas.getContext('2d');context.drawImage(video,0,0,160,100);const data=context.getImageData(0,0,160,100).data;let green=0;for(let i=0;i<data.length;i+=4)if(data[i]<40&&data[i+1]>180&&data[i+2]<40)green++;return green/16000;};const liveGreen=pixels(source),chunks=[];const recorder=new MediaRecorder(stream);const stopped=new Promise(resolve=>{recorder.ondataavailable=event=>chunks.push(event.data);recorder.onstop=resolve;});recorder.start();await new Promise(resolve=>setTimeout(resolve,900));recorder.stop();await stopped;stream.getTracks().forEach(track=>track.stop());const blob=new Blob(chunks,{type:recorder.mimeType}),url=URL.createObjectURL(blob),replay=document.createElement('video');replay.muted=true;replay.src=url;await replay.play();await new Promise(resolve=>setTimeout(resolve,100));const replayGreen=pixels(replay),bytes=await blob.arrayBuffer();let encoded='';for(const byte of new Uint8Array(bytes))encoded+=String.fromCharCode(byte);const result={kind:message.kind,liveGreen,replayGreen,width:replay.videoWidth,height:replay.videoHeight,bytes:bytes.byteLength,mimeType:blob.type,base64:btoa(encoded),audio:stream.getAudioTracks().length};replay.pause();URL.revokeObjectURL(url);document.title=JSON.stringify(result);}catch(error){document.title='FAIL:'+error;}});`,
    };
    await IOUtils.write(
      xpi,
      zipSync(
        Object.fromEntries(
          Object.entries(files).map(([name, source]) => [
            name,
            new TextEncoder().encode(source),
          ]),
        ),
      ),
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(xpi));
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "screen chooser extension",
    );
    const background = await until(
      () => extension.backgroundContext,
      "background document",
    );
    const chooseKind = async (kind: DesktopSourceKind): Promise<object> => {
      window.gBrowser.selectedTab = target;
      post(background.xulBrowser, { kind });
      const dialog = await until(
        () =>
          dialogs().find(
            (item) =>
              item.document.readyState === "complete" &&
              (item.document.getElementById("list") as NativeDialogList | null)
                ?.itemCount,
          ),
        "owned native source-kind chooser",
      );
      const list = dialog.document.getElementById("list") as
        | NativeDialogList
        | null;
      assert(list, "native source-kind chooser list exists");
      assertEquals(
        list.itemCount,
        1,
        "only the requested source kind is offered",
      );
      list.selectedIndex = 0;
      const dialogElement = dialog.document.querySelector("dialog") as
        | NativeDialog
        | null;
      assert(dialogElement, "native source-kind chooser dialog exists");
      dialogElement.acceptDialog();
      return until(() => {
        const title = background.xulBrowser.contentTitle;
        if (title.startsWith("FAIL:")) throw Error(title);
        const notification = window.PopupNotifications.getNotification(
          "webRTC-shareDevices",
          target.linkedBrowser,
        );
        return (
          notification &&
          window.PopupNotifications.panel.state === "open" &&
          notification
        );
      }, "actual native WebRTC source chooser");
    };
    await chooseKind("screen");
    const nativePanel = (): CapturePermissionPanel => {
      const panel = window.PopupNotifications.panel.querySelector(
        "popupnotification",
      ) as CapturePermissionPanel | null;
      assert(panel, "native permission prompt remains open");
      return panel;
    };
    nativePanel().secondaryButton.click();
    await until(
      async () =>
        (await read<DesktopSelectionStatus>(background.xulBrowser)).phase ===
          "denied",
      "native screen-sharing denial yields empty token",
    );
    for (const kind of ["screen", "window"] as const) {
      await chooseKind(kind);
      const list = window.document.getElementById(
        "webRTC-selectWindow-menulist",
      ) as DesktopSourceList | null;
      assert(list, "native desktop source menu exists");
      const menu = window.document.getElementById(
        "webRTC-selectWindow-menupopup",
      );
      assert(menu, "native desktop source choices exist");
      const choices = [
        ...menu.children,
      ].filter((item) =>
        (item as DesktopSourceChoice).mediaSource
      ) as DesktopSourceChoice[];
      assert(choices.length > 0, "real native desktop sources were enumerated");
      assert(
        choices.every((item) => item.mediaSource === kind),
        "picker broker restricts native chooser to requested source kind",
      );
      const selected = kind === "window"
        ? choices.find((item) =>
          item.label.includes("floorp-desktop-screen-green-source")
        )
        : choices[0];
      assert(
        selected,
        "actual visible source is offered: " +
          JSON.stringify(choices.map((item) => item.label)),
      );
      report.selections.push({
        kind,
        label: selected.label,
        sources: choices.map((item) => ({
          label: item.label,
          mediaSource: item.mediaSource,
        })),
      });
      list.selectedItem = selected;
      selected.dispatchEvent(new window.Event("command", { bubbles: true }));
      await until(
        () => !nativePanel().button.disabled,
        "native Allow button after explicit source choice",
      );
      nativePanel().button.click();
      const offscreen = await until(
        () =>
          [...extension.views].find((view) => view.viewType === "offscreen"),
        "actual Offscreen stream consumer",
      );
      const result = await until(async () => {
        const value = await read<DesktopRecording>(offscreen.xulBrowser);
        return value.kind === kind && value;
      }, "recorded and decoded native desktop WebM");
      assert(
        result.width > 200 && result.height > 200 && result.bytes > 1000,
        "actual native desktop stream records playable WebM",
      );
      assert(
        result.liveGreen > 0.05 && result.replayGreen > 0.05,
        "visible authored source pixels survive stream and recorded-media decode",
      );
      assertEquals(
        result.audio,
        0,
        "screen/window do not advertise unimplemented system audio",
      );
      const output = PathUtils.join(
        mediaDirectory,
        "floorp-native-desktop-" +
          kind +
          "-" +
          Services.uuid.generateUUID() +
          ".webm",
      );
      const { base64, ...recording } = result;
      const bytes = Uint8Array.from(
        atob(base64),
        (value) => value.charCodeAt(0),
      );
      assertEquals(
        bytes.byteLength,
        result.bytes,
        "retained native recording size",
      );
      await IOUtils.write(output, bytes);
      const sha256 = [
        ...new Uint8Array(
          await crypto.subtle.digest("SHA-256", bytes),
        ),
      ].map((value) => value.toString(16).padStart(2, "0")).join("");
      report.recordings.push({ ...recording, path: output, sha256 });
    }
    await IOUtils.writeUTF8(
      PathUtils.join(
        mediaDirectory,
        "floorp-desktop-screen-native-result.json",
      ),
      JSON.stringify(report, null, 2),
    );
    console.log("[DesktopScreenNative]", JSON.stringify(report));
  } finally {
    for (const dialog of dialogs()) dialog.close();
    await addon?.uninstall();
    if (target.isConnected) window.gBrowser.removeTab(target);
    await IOUtils.remove(xpi, { ignoreAbsent: true });
  }
}

export function runAllTests() {
  return runTests("DesktopScreenNative.test.mts", [
    {
      name:
        "real screen/window chooser consent, denial, native pixels and playable media",
      fn: desktopScreen,
    },
  ]);
}
