// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import type {
  DesktopCaptureAddon,
  DesktopCaptureAddonManager,
  DesktopCaptureCommand,
  DesktopCaptureContext,
  DesktopCaptureDialog,
  DesktopCaptureDialogList,
  DesktopCaptureExtensionParent,
  DesktopCaptureWindow,
  DesktopCaptureWindowMediator,
} from "../../helpers/desktop-capture-test-types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function until<T>(
  read: () => T | false | undefined | Promise<T | false | undefined>,
  label: string,
): Promise<T> {
  const limit = Date.now() + 25000;
  while (Date.now() < limit) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error("Timed out waiting for " + label);
}
function post(context: DesktopCaptureContext, data: DesktopCaptureCommand) {
  context.xulBrowser.messageManager.loadFrameScript(
    "data:application/javascript," +
      encodeURIComponent(`content.postMessage(${JSON.stringify(data)},"*")`),
    false,
  );
}
async function desktopAPI() {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: DesktopCaptureAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: DesktopCaptureExtensionParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as DesktopCaptureWindow;
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const target = window.gBrowser.addTab("about:blank", {
    triggeringPrincipal: principal,
  });
  window.gBrowser.selectedTab = target;
  const originalId = "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh",
    id = originalId + "@chromium-extension";
  const xpi = PathUtils.join(
    PathUtils.tempDir,
    "floorp-desktop-api-" + Services.uuid.generateUUID() + ".xpi",
  );
  const html = PathUtils.join(
    PathUtils.tempDir,
    "floorp-desktop-api-target-" + Services.uuid.generateUUID() + ".html",
  );
  const autoplay = Services.prefs.prefHasUserValue("media.autoplay.default")
    ? Services.prefs.getIntPref("media.autoplay.default")
    : null;
  let addon: DesktopCaptureAddon | undefined;
  const dialogs = () =>
    [...(Services.wm as unknown as DesktopCaptureWindowMediator).getEnumerator(
      null,
    )].filter((item) =>
      !item.closed &&
      item.document.documentURI === "chrome://global/content/selectDialog.xhtml"
    );
  const nextDialog = () =>
    until(
      () =>
        dialogs().find((item) =>
          item.document.readyState === "complete" &&
          (item.document.getElementById("list") as unknown as
            | DesktopCaptureDialogList
            | null)?.itemCount
        ),
      "native selection dialog",
    );
  const accept = async (choose: number | string) => {
    const dialog = await nextDialog();
    const list = dialog.document.getElementById(
      "list",
    ) as unknown as DesktopCaptureDialogList;
    const labels = Array.from(
      { length: list.itemCount },
      (_, index) => list.getItemAtIndex(index).label,
    );
    const index = typeof choose === "number"
      ? choose
      : labels.findIndex((label) => label.includes(choose));
    assert(
      index >= 0 && index < labels.length,
      "native chooser offers requested source: " + JSON.stringify(labels),
    );
    list.selectedIndex = index;
    (dialog.document.querySelector("dialog") as unknown as DesktopCaptureDialog)
      .acceptDialog();
    await until(() => dialog.closed, "native dialog accepted");
  };
  try {
    Services.prefs.setIntPref("media.autoplay.default", 0);
    await IOUtils.writeUTF8(
      html,
      '<!doctype html><title>desktop-api-green-source</title><body style="background:rgb(0,255,0)"><script>let value=254;setInterval(()=>document.body.style.background="rgb(0,"+(value=509-value)+",0)",80);const audio=new AudioContext();const osc=audio.createOscillator();osc.frequency.value=739.99;osc.connect(audio.destination);osc.start();audio.resume();</script>',
    );
    target.linkedBrowser.fixupAndLoadURIString(
      Services.io.newFileURI(new FileUtils.File(html)).spec,
      { triggeringPrincipal: principal },
    );
    await until(
      () => target.linkedBrowser.contentTitle === "desktop-api-green-source",
      "source document",
    );
    const files = {
      "manifest.json": JSON.stringify({
        manifest_version: 3,
        name: "Native desktop API fixture",
        version: "1.0",
        permissions: ["desktopCapture", "offscreen"],
        background: { scripts: ["background.js"] },
        browser_specific_settings: {
          gecko: {
            id,
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: originalId,
            },
          },
        },
      }),
      "background.js":
        `let requestId,callbacks=0;window.addEventListener("message",async event=>{try{const command=event.data;if(command.cancel){browser.desktopCapture.cancelChooseDesktopMedia(requestId);document.title=JSON.stringify({phase:"cancelled",callbacks});return;}if(command.inspect){document.title=JSON.stringify({phase:"inspect",callbacks});return;}if(command.start){requestId=browser.desktopCapture.chooseDesktopMedia(command.audio?["tab","audio"]:["tab"],async(token,options)=>{callbacks++;if(browser.runtime.lastError){document.title="FAIL:"+browser.runtime.lastError.message;return;}if(!token){document.title=JSON.stringify({phase:"empty",callbacks});return;}document.title=JSON.stringify({phase:"selected",token,options,callbacks});if(command.record){try{if(!await browser.offscreen.hasDocument())await browser.offscreen.createDocument({url:"offscreen.html",reasons:["USER_MEDIA"],justification:"Native desktop capture recording"});await browser.runtime.sendMessage({token,audio:command.audio,hold:command.hold});}catch(error){document.title="FAIL:"+error.message;}}});document.title=JSON.stringify({phase:"requested",requestId});}}catch(error){document.title="FAIL:"+error.message;}});`,
      "offscreen.html":
        '<!doctype html><title>ready</title><script src="offscreen.js"></script>',
      "offscreen.js":
        `browser.runtime.onMessage.addListener(async message=>{try{const mandatory={chromeMediaSource:"desktop",chromeMediaSourceId:message.token};const stream=await navigator.mediaDevices.getUserMedia({audio:message.audio?{mandatory}:false,video:{mandatory}});const video=document.createElement("video");video.muted=true;video.srcObject=stream;await video.play();const audio=message.audio?new AudioContext():null;let analyser;if(audio){analyser=audio.createAnalyser();analyser.fftSize=8192;audio.createMediaStreamSource(stream).connect(analyser);await audio.resume();}document.title=JSON.stringify({phase:"recording",audio:stream.getAudioTracks().length,video:stream.getVideoTracks().length});if(message.hold){const timer=setInterval(()=>{if(stream.getTracks().every(track=>track.readyState==="ended")){clearInterval(timer);document.title="source-ended";}},25);return;}await new Promise(resolve=>setTimeout(resolve,600));const canvas=document.createElement("canvas");canvas.width=2;canvas.height=2;const context=canvas.getContext("2d");context.drawImage(video,0,0,2,2);const pixel=Array.from(context.getImageData(1,1,1,1).data);let rms=0,frequency=0;if(analyser){const samples=new Float32Array(analyser.fftSize);analyser.getFloatTimeDomainData(samples);rms=Math.sqrt(samples.reduce((sum,value)=>sum+value*value,0)/samples.length);const bins=new Float32Array(analyser.frequencyBinCount);analyser.getFloatFrequencyData(bins);let peak=0;for(let i=1;i<bins.length;i++)if(bins[i]>bins[peak])peak=i;frequency=peak*audio.sampleRate/analyser.fftSize;}let bytes=0;const recorder=new MediaRecorder(stream);const done=new Promise(resolve=>{recorder.ondataavailable=event=>bytes+=event.data.size;recorder.onstop=resolve;});recorder.start();await new Promise(resolve=>setTimeout(resolve,500));recorder.stop();await done;stream.getTracks().forEach(track=>track.stop());let replayDenied=false;try{const duplicate=await navigator.mediaDevices.getUserMedia({audio:false,video:{mandatory}});duplicate.getTracks().forEach(track=>track.stop());}catch{replayDenied=true;}await audio?.close();document.title=JSON.stringify({phase:"done",bytes,pixel,rms,frequency,replayDenied});}catch(error){document.title="FAIL:"+error.message;}});`,
    };
    await IOUtils.write(
      xpi,
      zipSync(
        Object.fromEntries(
          Object.entries(files).map((
            [name, text],
          ) => [name, new TextEncoder().encode(text)]),
        ),
      ),
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(xpi));
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "desktop extension",
    );
    let background = await until(
      () => extension.backgroundContext,
      "background",
    );
    post(background, { start: true });
    const first = await nextDialog();
    const request = JSON.parse(
      await until(() =>
        background.xulBrowser.contentTitle.startsWith("{") &&
        background.xulBrowser.contentTitle, "request id"),
    );
    assert(
      Number.isInteger(request.requestId) && request.requestId > 0,
      "chooseDesktopMedia returns a synchronous integer ID: " +
        JSON.stringify(request),
    );
    (first.document.querySelector("dialog") as unknown as DesktopCaptureDialog)
      .cancelDialog();
    await until(
      () => background.xulBrowser.contentTitle.includes('"phase":"empty"'),
      "native user cancellation callback",
    );
    post(background, { start: true });
    const second = await nextDialog();
    post(background, { cancel: true });
    await until(
      () => second.closed,
      "cancelChooseDesktopMedia closes owned native dialog",
    );
    await new Promise((resolve) => setTimeout(resolve, 100));
    post(background, { inspect: true });
    await until(
      () => background.xulBrowser.contentTitle.includes('"phase":"inspect"'),
      "cancellation callback count",
    );
    assertEquals(
      JSON.parse(background.xulBrowser.contentTitle).callbacks,
      1,
      "programmatic cancellation suppresses callback",
    );
    post(background, { start: true, audio: true, record: true });
    await accept(0);
    await accept("desktop-api-green-source");
    await accept(0);
    const offscreen = await until(() => {
      if (background.xulBrowser.contentTitle.startsWith("FAIL:")) {
        throw new Error(background.xulBrowser.contentTitle);
      }
      return [...extension.views].find((view) => view.viewType === "offscreen");
    }, "offscreen consumer");
    await until(() => {
      const title = offscreen.xulBrowser.contentTitle;
      if (title.startsWith("FAIL:")) throw new Error("consumer: " + title);
      return title.includes('"phase":"recording"');
    }, "native desktop recording");
    await extension.terminateBackground();
    await until(() => {
      const title = offscreen.xulBrowser.contentTitle;
      if (title.startsWith("FAIL:")) throw new Error("consumer: " + title);
      return title.includes('"phase":"done"');
    }, "desktop recording after background suspension");
    const result = JSON.parse(offscreen.xulBrowser.contentTitle);
    assert(result.bytes > 1000, "actual desktop stream records WebM");
    assert(
      result.pixel[1] > 200 && result.pixel[0] < 20,
      "selected native tab pixels reach Offscreen",
    );
    assert(
      result.rms > 0.05 && Math.abs(result.frequency - 739.99) < 25,
      "explicitly approved target audio reaches Offscreen",
    );
    assert(result.replayDenied, "desktop stream ID is single use");
    await IOUtils.writeUTF8(
      PathUtils.join(
        PathUtils.tempDir,
        "floorp-desktop-api-native-result.json",
      ),
      JSON.stringify(result),
    );
    const contexts = await extension.apiManager.asyncGetAPI(
      "runtime",
      extension,
      "addon_parent",
    );
    const listed = await contexts.getAPI(offscreen).runtime.getContexts({});
    assert(
      listed.every((context) =>
        !context.documentUrl?.endsWith("manifest.json")
      ),
      "private capture source stays outside public extension contexts",
    );
    await extension.wakeupBackground();
    background = await until(
      () => extension.backgroundContext,
      "replacement background",
    );
    post(background, { start: true, record: true, hold: true });
    await accept(0);
    await accept("desktop-api-green-source");
    await until(() => {
      const title = offscreen.xulBrowser.contentTitle;
      if (title.startsWith("FAIL:")) throw new Error(title);
      return title.includes('"phase":"recording"');
    }, "new actual desktop stream before source closure");
    window.gBrowser.removeTab(target);
    await until(
      () => offscreen.xulBrowser.contentTitle === "source-ended",
      "source tab closure ends actual desktop consumer tracks",
    );
  } finally {
    for (const dialog of dialogs()) dialog.close();
    await addon?.uninstall();
    if (target.isConnected) window.gBrowser.removeTab(target);
    await IOUtils.remove(xpi, { ignoreAbsent: true });
    await IOUtils.remove(html, { ignoreAbsent: true });
    if (autoplay === null) {
      Services.prefs.clearUserPref("media.autoplay.default");
    } else Services.prefs.setIntPref("media.autoplay.default", autoplay);
  }
}
export async function runAllTests() {
  await runTests("DesktopCaptureNative.test.mts", [{
    name:
      "native desktop chooser cancellation and real tab video/audio to Offscreen",
    fn: desktopAPI,
  }]);
}
