// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
import type {
  ActivationError,
  ComponentAddon,
  ComponentAddonManager,
  ComponentExtensionParent,
  ComponentNativeWindow,
  ComponentTab,
  ConsumerRecording,
} from "../../helpers/capture-component-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function waitFor<T>(
  read: () =>
    | T
    | false
    | undefined
    | null
    | Promise<T | false | undefined | null>,
  description: string,
): Promise<T> {
  const until = Date.now() + 20000;
  while (Date.now() < until) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error("Timed out waiting for " + description);
}
async function consumerTab() {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: ComponentAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: ComponentExtensionParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as ComponentNativeWindow;
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const tabs: ComponentTab[] = [];
  const target = window.gBrowser.addTab("about:blank", {
    triggeringPrincipal: principal,
  });
  tabs.push(target);
  target.linkedBrowser.fixupAndLoadURIString(
    "data:text/html," +
      encodeURIComponent(
        '<!doctype html><title>target</title><body style="background:rgb(255,0,0)">Capture target<script>let value=254;setInterval(()=>document.body.style.background="rgb("+(value=509-value)+",0,0)",100)</script>',
      ),
    { triggeringPrincipal: principal },
  );
  const page = PathUtils.join(
    PathUtils.tempDir,
    "floorp-consumer-" + Services.uuid.generateUUID() + ".html",
  );
  const xpi = PathUtils.join(
    PathUtils.tempDir,
    "floorp-consumer-" + Services.uuid.generateUUID() + ".xpi",
  );
  let addon: ComponentAddon | undefined;
  const post = (tab: ComponentTab, data: unknown) =>
    tab.linkedBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(`content.postMessage(${JSON.stringify(data)},"*")`),
      false,
    );
  try {
    await IOUtils.writeUTF8(
      page,
      `<!doctype html><title>consumer</title><script>
   window.addEventListener("message",async event=>{
    if(event.data.rawProbe){const errors=[];for(const video of [{mediaSource:"browser",deviceId:{exact:String(event.data.rawProbe)}},{mediaSource:"screen"}]){try{const stream=await navigator.mediaDevices.getUserMedia({audio:false,video});stream.getTracks().forEach(track=>track.stop());errors.push({name:"SUCCESS"});}catch(error){errors.push({name:error.name,message:error.message});}}document.title=JSON.stringify({errors});return;}
    if(!event.data.token)return;try{
    const constraints={audio:false,video:{mandatory:{chromeMediaSource:event.data.desktop?"desktop":"tab",chromeMediaSourceId:event.data.token,maxFrameRate:10}}};
    document.title="allocating";const stream=await navigator.mediaDevices.getUserMedia(constraints);document.title="playing";
    if(event.data.hold){document.title="holding";const timer=setInterval(()=>{if(stream.getTracks().every(track=>track.readyState==="ended")){clearInterval(timer);document.title="capture-stopped";}},25);return;}
    const video=document.createElement("video");video.muted=true;video.srcObject=stream;await video.play();document.title="sampling";await new Promise(resolve=>setTimeout(resolve,150));
    const canvas=document.createElement("canvas");canvas.width=2;canvas.height=2;const context=canvas.getContext("2d");context.drawImage(video,0,0,2,2);const pixel=Array.from(context.getImageData(1,1,1,1).data);
    document.title="recording";let bytes=0;const recorder=new MediaRecorder(stream);const done=new Promise(resolve=>{recorder.ondataavailable=event=>bytes+=event.data.size;recorder.onstop=resolve});recorder.start();await new Promise(resolve=>setTimeout(resolve,300));recorder.stop();await done;stream.getTracks().forEach(track=>track.stop());
    document.title="replay";let replayDenied=false;try{const replay=await navigator.mediaDevices.getUserMedia(constraints);replay.getTracks().forEach(track=>track.stop());}catch{replayDenied=true;}
    document.title=JSON.stringify({pixel,bytes,replayDenied,namespace:typeof window.browser});
   }catch(error){document.title="DENIED:"+error.message;}});
  </script>`,
    );
    const makeConsumer = () => {
      const tab = window.gBrowser.addTab("about:blank", {
        triggeringPrincipal: principal,
      });
      tabs.push(tab);
      tab.linkedBrowser.fixupAndLoadURIString(
        Services.io.newFileURI(new FileUtils.File(page)).spec,
        { triggeringPrincipal: principal },
      );
      return tab;
    };
    const consumer = makeConsumer(), other = makeConsumer();
    await waitFor(
      () =>
        consumer.linkedBrowser.contentTitle === "consumer" &&
        other.linkedBrowser.contentTitle === "consumer" &&
        target.linkedBrowser.contentTitle === "target",
      "fixture pages",
    );
    const originalId = "ffffffffffffffffffffffffffffffff",
      id = originalId + "@chromium-extension";
    const files = {
      "manifest.json": JSON.stringify({
        manifest_version: 3,
        name: "Consumer capture fixture",
        version: "1.0",
        permissions: ["tabCapture"],
        action: {},
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
        `let consumer;window.addEventListener("message",event=>{consumer=event.data.consumer;document.title="consumer:"+consumer;});browser.action.onClicked.addListener(async tab=>{try{const token=await browser.tabCapture.getMediaStreamId({targetTabId:tab.id,consumerTabId:consumer});document.title=JSON.stringify({token});}catch(error){document.title="FAIL:"+error.message;}});`,
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
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "extension",
    );
    const background = await waitFor(
      () => extension.backgroundContext,
      "background",
    );
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
    const issue = async (tab: ComponentTab): Promise<string> => {
      await waitFor(
        async () =>
          !(await captureModule.getAPI(background).tabCapture.getCapturedTabs())
            .length,
        "previous native source cleanup",
      );
      const consumerId = extension.tabManager.getWrapper(tab).id;
      background.xulBrowser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            `content.postMessage({consumer:${consumerId}},"*")`,
          ),
        false,
      );
      await waitFor(
        () => background.xulBrowser.contentTitle === "consumer:" + consumerId,
        "consumer selection",
      );
      window.gBrowser.selectedTab = target;
      action.action.triggerClickOrPopup(target);
      await waitFor(() => {
        const title = background.xulBrowser.contentTitle;
        if (title.startsWith("FAIL:")) throw new Error(title);
        return title.startsWith("{");
      }, "document-bound stream ID");
      return JSON.parse(background.xulBrowser.contentTitle).token;
    };
    const token = await issue(consumer);
    const otherToken = await issue(other);
    post(other, { token });
    await waitFor(
      () => other.linkedBrowser.contentTitle.startsWith("DENIED:"),
      "wrong consumer rejection",
    );
    assert(
      /consumer document/i.test(other.linkedBrowser.contentTitle),
      "another same-origin tab cannot consume token",
    );
    post(consumer, { token, desktop: true });
    await waitFor(
      () => consumer.linkedBrowser.contentTitle.startsWith("DENIED:"),
      "cross-API token rejection",
    );
    assert(
      /another API/i.test(consumer.linkedBrowser.contentTitle),
      "desktop consumer cannot reinterpret a tab capture token",
    );
    window.gBrowser.selectedTab = consumer;
    const previousDenial = consumer.linkedBrowser.contentTitle;
    post(consumer, { token });
    await waitFor(
      () => consumer.linkedBrowser.contentTitle !== previousDenial,
      "authorized consumer starts after cross-API rejection",
    );
    await waitFor(() => {
      const title = consumer.linkedBrowser.contentTitle;
      if (title.startsWith("DENIED:")) throw new Error(title);
      return title.startsWith("{");
    }, "site actual native stream");
    const result = JSON.parse(
      consumer.linkedBrowser.contentTitle,
    ) as ConsumerRecording;
    assert(result.bytes > 0, "real site-consumer WebM");
    assert(
      result.pixel[0] > 200 && result.pixel[1] < 20,
      "real authorized source pixels",
    );
    assert(result.replayDenied, "site token remains single use");
    assertEquals(
      result.namespace,
      "undefined",
      "site receives no browser extension APIs",
    );
    const heldToken = await issue(consumer);
    const { ChromeExtensionCaptureRegistry } = ChromeUtils.importESModule(
      "resource://gre/modules/ChromeExtensionCaptureRegistry.sys.mjs",
    );
    const consumerActor = consumer.linkedBrowser.browsingContext
      .currentWindowGlobal.getActor("ChromeExtensionCaptureConsumer");
    const pending = await ChromeExtensionCaptureRegistry.call(
      consumerActor,
      "_consumeStreamId",
      [heldToken, { audio: false, video: true }],
    );
    post(consumer, {
      rawProbe: target.linkedBrowser.browsingContext.browserId,
    });
    await waitFor(
      () => consumer.linkedBrowser.contentTitle.includes('"errors"'),
      "ordinary-site principal probe with pending backend permit",
    );
    const errors = (JSON.parse(consumer.linkedBrowser.contentTitle) as {
      errors: ActivationError[];
    }).errors;
    assert(
      errors.every((error) =>
        error.name === "InvalidStateError" &&
        /gesture|activation/i.test(error.message)
      ),
      "ordinary site capture keeps content activation gate even during internal backend permit",
    );
    await ChromeExtensionCaptureRegistry.call(consumerActor, "_streamStopped", [
      pending.sessionId,
    ]);
    // An outstanding token is bound to the old document even for same-origin reload.
    other.linkedBrowser.reload();
    await waitFor(
      () => other.linkedBrowser.contentTitle === "consumer",
      "new consumer document",
    );
    const third = await issue(other);
    post(other, { token: otherToken });
    await waitFor(
      () => other.linkedBrowser.contentTitle.startsWith("DENIED:"),
      "stale consumer document rejection",
    );
    assert(
      /consumer document|invalid|expired/i.test(
        other.linkedBrowser.contentTitle,
      ),
      "same-origin replacement invalidates old consumer token",
    );
    window.gBrowser.selectedTab = other;
    post(other, { token: third });
    await waitFor(
      () => other.linkedBrowser.contentTitle.startsWith("{"),
      "new document's own token succeeds",
    );
    const navigating = await issue(consumer);
    window.gBrowser.selectedTab = consumer;
    post(consumer, { token: navigating, hold: true });
    await waitFor(
      () => consumer.linkedBrowser.contentTitle === "holding",
      "active site capture before consumer navigation",
    );
    consumer.linkedBrowser.reload();
    await waitFor(
      () => consumer.linkedBrowser.contentTitle === "consumer",
      "replacement consumer document",
    );
    await waitFor(
      async () =>
        !(await captureModule.getAPI(background).tabCapture.getCapturedTabs())
          .length,
      "consumer navigation revokes actual native source",
    );
    const last = await issue(consumer);
    window.gBrowser.selectedTab = consumer;
    post(consumer, { token: last, hold: true });
    await waitFor(
      () => consumer.linkedBrowser.contentTitle === "holding",
      "active site capture before extension unload",
    );
    await addon.uninstall();
    addon = undefined;
    await waitFor(
      () => consumer.linkedBrowser.contentTitle === "capture-stopped",
      "extension unload ends real site consumer tracks",
    );
  } finally {
    await IOUtils.writeUTF8(
      PathUtils.join(PathUtils.tempDir, "floorp-consumer-native-titles.json"),
      JSON.stringify(tabs.map((tab) => tab.linkedBrowser.contentTitle)),
    );
    await addon?.uninstall();
    for (const tab of tabs) window.gBrowser.removeTab(tab);
    await IOUtils.remove(page, { ignoreAbsent: true });
    await IOUtils.remove(xpi, { ignoreAbsent: true });
  }
}
export async function runAllTests() {
  await runTests("CaptureConsumerNative.test.mts", [{
    name: "consumerTabId binds actual capture to exact secure site document",
    fn: consumerTab,
  }]);
}
