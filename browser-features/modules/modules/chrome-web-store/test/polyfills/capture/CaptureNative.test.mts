// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
import type { CaptureNativeWindow } from "../../helpers/capture-native-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

type CaptureContext = {
  viewType: string;
  withCallContextData<T>(
    options: { isHandlingUserInput: boolean },
    run: () => T,
  ): T;
  xulBrowser: {
    contentTitle: string;
    messageManager: { loadFrameScript(script: string, delayed: boolean): void };
  };
};
type Extension = {
  views: Set<CaptureContext>;
  backgroundContext: CaptureContext;
  backgroundState: string;
  terminateBackground(): Promise<void>;
  apiManager: {
    asyncGetAPI(name: string, extension: Extension, scope: string): Promise<{
      action: {
        triggerClickOrPopup(tab: unknown): void;
        setActiveTabForPreload(tab: unknown): void;
      };
      getAPI(context: unknown): {
        tabCapture: {
          getCapturedTabs(): Promise<{ tabId: number; status: string }[]>;
          _consumeStreamId(token: string): Promise<{ sessionId: string }>;
          _streamStopped(sessionId: string): Promise<void>;
          getMediaStreamId(options?: { targetTabId?: number }): Promise<string>;
        };
        offscreen: { closeDocument(): Promise<void> };
      };
    }>;
  };
};
async function waitFor<T>(
  read: () => T | undefined | false | Promise<T | undefined | false>,
  description: string,
): Promise<T> {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${description}`);
}
function post(context: CaptureContext, value: unknown) {
  context.xulBrowser.messageManager.loadFrameScript(
    "data:application/javascript," +
      encodeURIComponent(`content.postMessage(${JSON.stringify(value)}, "*");`),
    false,
  );
}
async function testCapture(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const originalId = "abcdabcdabcdabcdabcdabcdabcdabcd";
  const id = `${originalId}@chromium-extension`;
  const win = Services.wm.getMostRecentWindow("navigator:browser") as
    | CaptureNativeWindow
    | null;
  assert(win, "a real browser window exists for native tab capture");
  const tab = win.gBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  win.gBrowser.selectedTab = tab;
  const html =
    '<!doctype html><body style="background:red"><h1>Native tab capture</h1><script>setInterval(()=>document.body.style.background=document.body.style.background==="red"?"blue":"red",100)</script>';
  tab.linkedBrowser.fixupAndLoadURIString(
    "data:text/html," + encodeURIComponent(html),
    {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    },
  );
  await new Promise((resolve) => setTimeout(resolve, 300));
  const manifest = {
    manifest_version: 3,
    name: "Native capture fixture",
    version: "1.0",
    permissions: ["tabCapture", "offscreen", "activeTab"],
    action: {},
    background: { scripts: ["background.js"] },
    browser_specific_settings: {
      gecko: {
        id,
        floorp_chrome_extension: { schema_version: 1, original_id: originalId },
      },
    },
  };
  const encoder = new TextEncoder();
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-capture-${Services.uuid.generateUUID()}.xpi`,
  );
  const files = {
    "manifest.json": JSON.stringify(manifest),
    "background.js": `
      let mode = "record";
      window.addEventListener("message", event => {mode=event.data; document.title="mode:"+mode;});
      browser.tabCapture.getMediaStreamId().then(() => document.title="GESTURE-BYPASS", error => document.title="gesture-denied:"+error.message);
      browser.action.onClicked.addListener(async tab => {
        try {
          if(mode==="direct") {
            const stream=await browser.tabCapture.capture({video:true,audio:false,videoConstraints:{mandatory:{maxFrameRate:10}}});
            let bytes=0;
            const recorder=new MediaRecorder(stream,{mimeType:"video/webm"});
            const done=new Promise(resolve=>{recorder.ondataavailable=event=>bytes+=event.data.size;recorder.onstop=resolve;});
            recorder.start(100);await new Promise(resolve=>setTimeout(resolve,300));recorder.stop();await done;
            stream.getTracks().forEach(track=>track.stop());
            document.title=JSON.stringify({directBytes:bytes,isStream:stream instanceof MediaStream});
            return;
          }
          const pendingToken = browser.tabCapture.getMediaStreamId({targetTabId:tab.id});
          const token = await pendingToken;
          if(mode==="hold") {document.title=JSON.stringify({token}); return;}
          if(!await browser.offscreen.hasDocument()) await browser.offscreen.createDocument({url:"offscreen.html",reasons:["USER_MEDIA"],justification:"Native tab video recording test"});
          browser.runtime.sendMessage({token}).catch(error=>document.title="send:"+error.message);
        } catch(error) {document.title="FAIL:"+error.message;}
      });
    `,
    "offscreen.html":
      '<!doctype html><title>loading</title><script src="offscreen.js"></script>',
    "offscreen.js": `
      const probeErrors=[];
      async function rejected(operation) {
        try {const stream=await operation();stream?.getTracks().forEach(track=>track.stop());return false;}
        catch {return true;}
      }
      async function gestureDenied(operation) {
        try {const stream=await operation();stream?.getTracks().forEach(track=>track.stop());return false;}
        catch(error) {probeErrors.push({name:error.name,message:error.message});return error.name==="InvalidStateError" && /gesture|activation/i.test(error.message);}
      }
      async function record(token) {
        try {
          const ordinaryDenied=await gestureDenied(()=>navigator.mediaDevices.getUserMedia({audio:false,video:{mediaSource:"browser",deviceId:{exact:"${
      String(tab.linkedBrowser.browsingContext.browserId)
    }"}}}));
          const screenDenied=await gestureDenied(()=>navigator.mediaDevices.getUserMedia({audio:false,video:{mediaSource:"screen"}}));
          const constraints={audio:false,video:{mandatory:{chromeMediaSource:"tab",chromeMediaSourceId:token,maxFrameRate:10}}};
          let audioDenied=false;
          try {await navigator.mediaDevices.getUserMedia({...constraints,audio:true});}
          catch(error) {audioDenied=/same authorized tab stream ID/i.test(error.message);}
          const stream=await navigator.mediaDevices.getUserMedia(constraints);
          const video=document.createElement("video");
          video.muted=true; video.srcObject=stream;
          await video.play();
          await new Promise(resolve=>setTimeout(resolve,150));
          const canvas=document.createElement("canvas");canvas.width=2;canvas.height=2;
          const ctx=canvas.getContext("2d");ctx.drawImage(video,0,0,2,2);
          const pixel=Array.from(ctx.getImageData(1,1,1,1).data);
          let bytes=0;
          const recorder=new MediaRecorder(stream,{mimeType:"video/webm"});
          const done=new Promise((resolve,reject)=>{recorder.ondataavailable=event=>bytes+=event.data.size;recorder.onstop=resolve;recorder.onerror=reject;});
          recorder.start(100);
          document.title=JSON.stringify({phase:"recording",ordinaryDenied,screenDenied,audioDenied});
          await new Promise(resolve=>setTimeout(resolve,1200));
          if(recorder.state!=="inactive") recorder.stop(); await done;
          const settings=stream.getVideoTracks()[0].getSettings();
          const audioTracks=stream.getAudioTracks().length;
          const endedBeforeManualStop=stream.getTracks().every(track=>track.readyState==="ended");
          stream.getTracks().forEach(track=>track.stop());
          const replayDenied=await rejected(()=>navigator.mediaDevices.getUserMedia(constraints));
          document.title=JSON.stringify({phase:"done",bytes,settings,audioTracks,pixel,ordinaryDenied,screenDenied,audioDenied,replayDenied,probeErrors,endedBeforeManualStop});
        } catch(error) {document.title="FAIL:"+error.message;}
      }
      browser.runtime.onMessage.addListener(message=>{if(message.token) record(message.token);});
      window.addEventListener("message",async event=>{
        if(event.data.token) record(event.data.token);
        if(event.data.probeNative) {
          const ordinaryDenied=await gestureDenied(()=>navigator.mediaDevices.getUserMedia({audio:false,video:{mediaSource:"browser",deviceId:{exact:String(event.data.probeNative)}}}));
          const screenDenied=await gestureDenied(()=>navigator.mediaDevices.getUserMedia({audio:false,video:{mediaSource:"screen"}}));
          document.title=JSON.stringify({phase:"permission-probe",ordinaryDenied,screenDenied});
        }
      });
    `,
  };
  await IOUtils.write(
    path,
    zipSync(
      Object.fromEntries(
        Object.entries(files).map((
          [name, text],
        ) => [name, encoder.encode(text)]),
      ),
    ),
  );
  let addon: { uninstall(): Promise<void> } | undefined;
  let sourceTabClosed = false;
  const cleanupErrors: string[] = [];
  const cleanupListener = {
    observe(message: nsIConsoleMessage) {
      if (message instanceof Ci.nsIScriptError) {
        if (
          message.sourceName ===
            "resource://gre/modules/ChromeExtensionCaptureChild.sys.mjs"
        ) cleanupErrors.push(message.errorMessage);
      }
    },
  };
  Services.console.registerListener(cleanupListener);
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor<Extension>(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "capture extension",
    );
    const background = await waitFor(
      () => extension.backgroundContext,
      "background context",
    );
    await waitFor(
      () => background.xulBrowser.contentTitle.startsWith("gesture-denied:"),
      "no-gesture rejection",
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
    const capture = captureModule.getAPI(background).tabCapture;
    action.action.setActiveTabForPreload(tab);
    let preloadRejected = false;
    try {
      await background.withCallContextData(
        { isHandlingUserInput: true },
        () => capture.getMediaStreamId(),
      );
    } catch (error) {
      preloadRejected = /real extension action invocation/.test(String(error));
    }
    assert(
      preloadRejected,
      "native activeTab hover preload never authorizes capture",
    );
    action.action.setActiveTabForPreload(null);
    post(background, "direct");
    await waitFor(
      () => background.xulBrowser.contentTitle === "mode:direct",
      "direct capture mode",
    );
    action.action.triggerClickOrPopup(tab);
    await waitFor(() => {
      const title = background.xulBrowser.contentTitle;
      if (title.startsWith("FAIL:")) throw new Error(title);
      return title.includes('"directBytes":');
    }, "capture() native MediaStream result");
    const direct = JSON.parse(background.xulBrowser.contentTitle) as {
      directBytes: number;
      isStream: boolean;
    };
    assert(
      direct.isStream && direct.directBytes > 0,
      "capture() returns a real recordable MediaStream",
    );
    await waitFor(
      async () => (await capture.getCapturedTabs()).length === 0,
      "direct capture stop",
    );
    post(background, "record");
    await waitFor(
      () => background.xulBrowser.contentTitle === "mode:record",
      "offscreen recording mode",
    );
    action.action.triggerClickOrPopup(tab);
    const offscreen = await waitFor(
      () => [...extension.views].find((view) => view.viewType === "offscreen"),
      "offscreen creation",
    );
    await waitFor(
      () => {
        const title = offscreen.xulBrowser.contentTitle;
        if (title.startsWith("FAIL:")) throw new Error(title);
        return title.includes('"phase":"recording"');
      },
      "live token-based capture",
    );
    const active = await capture.getCapturedTabs();
    assertEquals(active.length, 1, "one live capture session");
    assertEquals(active[0].status, "active", "native stream started");
    await extension.terminateBackground();
    assertEquals(
      extension.backgroundState,
      "stopped",
      "background actually terminated",
    );
    await waitFor(
      () => offscreen.xulBrowser.contentTitle.includes('"phase":"done"'),
      "offscreen native recording after background shutdown",
    );
    const result = JSON.parse(offscreen.xulBrowser.contentTitle) as {
      bytes: number;
      audioTracks: number;
      ordinaryDenied: boolean;
      screenDenied: boolean;
      audioDenied: boolean;
      replayDenied: boolean;
      pixel: number[];
    };
    assert(result.bytes > 0, "native MediaRecorder emits actual WebM data");
    assert(
      result.pixel[0] > 150 && result.pixel[2] < 80 ||
        result.pixel[2] > 150 && result.pixel[0] < 80,
      "capture pixels come from the selected red/blue target tab",
    );
    assertEquals(
      result.audioTracks,
      0,
      "video-only capture has no synthetic audio",
    );
    assert(
      result.ordinaryDenied,
      "ordinary browser gUM remains content-principal permission gated: " +
        offscreen.xulBrowser.contentTitle,
    );
    assert(result.screenDenied, "ordinary screen gUM remains permission gated");
    assert(
      result.audioDenied,
      "audio cannot attach a microphone to a tab token",
    );
    assert(result.replayDenied, "stream token is single use");
    await waitFor(() => {
      const state = capture.getCapturedTabs();
      return state.then((tabs) => tabs.length === 0);
    }, "capture stop status");
    await IOUtils.writeUTF8(
      PathUtils.join(PathUtils.tempDir, "floorp-capture-native-result.json"),
      JSON.stringify(result),
    );
    const consumerAPI = captureModule.getAPI(offscreen).tabCapture;
    const issue = () =>
      offscreen.withCallContextData(
        { isHandlingUserInput: true },
        () => consumerAPI.getMediaStreamId(),
      );
    const pending = await consumerAPI._consumeStreamId(await issue());
    assertEquals(
      (await capture.getCapturedTabs())[0].status,
      "pending",
      "native backend grant pending",
    );
    post(offscreen, {
      probeNative: tab.linkedBrowser.browsingContext.browserId,
    });
    await waitFor(
      () =>
        offscreen.xulBrowser.contentTitle.includes(
          '"phase":"permission-probe"',
        ),
      "ordinary media calls with pending backend grant",
    );
    const pendingProbe = JSON.parse(offscreen.xulBrowser.contentTitle) as {
      ordinaryDenied: boolean;
      screenDenied: boolean;
    };
    assert(
      pendingProbe.ordinaryDenied && pendingProbe.screenDenied,
      "pending one-shot backend permission never elevates ordinary media calls",
    );
    await consumerAPI._streamStopped(pending.sessionId);
    const staleToken = await issue();
    const recordingToken = await issue();
    post(offscreen, { token: recordingToken });
    await waitFor(() => {
      const title = offscreen.xulBrowser.contentTitle;
      if (title.startsWith("FAIL:")) throw new Error(title);
      return title.includes('"phase":"recording"');
    }, "second live capture before target navigation");
    tab.linkedBrowser.fixupAndLoadURIString(
      "data:text/html,<h1>replacement document</h1>",
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    await waitFor(
      async () => (await capture.getCapturedTabs()).length === 0,
      "target navigation revokes active capture",
    );
    await waitFor(() => {
      const title = offscreen.xulBrowser.contentTitle;
      if (title.startsWith("FAIL:")) throw new Error(title);
      return title.includes('"phase":"done"');
    }, "offscreen sees ended native source tracks");
    const navigated = JSON.parse(offscreen.xulBrowser.contentTitle) as {
      endedBeforeManualStop: boolean;
    };
    assert(
      navigated.endedBeforeManualStop,
      "target navigation stops the actual MediaStream tracks",
    );
    let navigationRejected = false;
    try {
      await consumerAPI._consumeStreamId(staleToken);
    } catch (error) {
      navigationRejected = /navigated/.test(String(error));
    }
    assert(
      navigationRejected,
      "unconsumed token cannot capture the replacement document",
    );
    action.action.triggerClickOrPopup(tab);
    await waitFor(
      () => offscreen.xulBrowser.contentTitle.includes('"phase":"recording"'),
      "live capture before Offscreen close",
    );
    const offscreenModule = await extension.apiManager.asyncGetAPI(
      "offscreen",
      extension,
      "addon_parent",
    );
    await offscreenModule.getAPI(offscreen).offscreen.closeDocument();
    await waitFor(
      async () => (await capture.getCapturedTabs()).length === 0,
      "Offscreen owner destruction stops capture",
    );
    action.action.triggerClickOrPopup(tab);
    const replacementOffscreen = await waitFor(
      () =>
        [...extension.views].find((view) =>
          view.viewType === "offscreen" && view !== offscreen
        ),
      "replacement Offscreen capture host",
    );
    await waitFor(
      () =>
        replacementOffscreen.xulBrowser.contentTitle.includes(
          '"phase":"recording"',
        ),
      "capture before source tab close",
    );
    win.gBrowser.removeTab(tab);
    sourceTabClosed = true;
    await waitFor(
      async () => (await capture.getCapturedTabs()).length === 0,
      "source tab closure revokes native capture",
    );
    await waitFor(
      () =>
        replacementOffscreen.xulBrowser.contentTitle.includes('"phase":"done"'),
      "Offscreen observes source tab closure",
    );
    const closedSource = JSON.parse(
      replacementOffscreen.xulBrowser.contentTitle,
    ) as { endedBeforeManualStop: boolean };
    assert(
      closedSource.endedBeforeManualStop,
      "closing source tab ends actual MediaStream tracks",
    );
  } finally {
    try {
      await addon?.uninstall();
      if (!sourceTabClosed) win.gBrowser.removeTab(tab);
      await IOUtils.remove(path, { ignoreAbsent: true });
      // Native context destruction and console delivery can follow uninstall.
      await new Promise((resolve) => setTimeout(resolve, 0));
    } finally {
      Services.console.unregisterListener(cleanupListener);
    }
  }
  assertEquals(
    cleanupErrors,
    [],
    "Offscreen destruction, target closure and uninstall finish capture cleanup without script errors",
  );
}
async function testNamespaceIsolation(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  for (const spoof of [false, true]) {
    const id = `capture-${spoof ? "spoof" : "ordinary"}@example.com`;
    const gecko = {
      id,
      ...(spoof
        ? {
          floorp_chrome_extension: {
            schema_version: 1,
            original_id: "abcdefghijklmnopabcdefghijklmnop",
          },
        }
        : {}),
    };
    const path = PathUtils.join(
      PathUtils.tempDir,
      `capture-isolation-${Services.uuid.generateUUID()}.xpi`,
    );
    const encoder = new TextEncoder();
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": encoder.encode(JSON.stringify({
          manifest_version: 3,
          name: "Capture isolation fixture",
          version: "1.0",
          permissions: ["tabCapture", "desktopCapture"],
          background: { scripts: ["background.js"] },
          browser_specific_settings: { gecko },
        })),
        "background.js": encoder.encode(
          'document.title="capture:"+typeof browser.tabCapture+":"+typeof browser.desktopCapture;',
        ),
      }),
    );
    let addon: { uninstall(): Promise<void> } | undefined;
    try {
      addon = await AddonManager.installTemporaryAddon(
        new FileUtils.File(path),
      );
      const extension = await waitFor<Extension>(
        () => ExtensionParent.GlobalManager.extensionMap.get(id),
        "ordinary extension startup",
      );
      const context = await waitFor(
        () => extension.backgroundContext,
        "ordinary background",
      );
      await waitFor(
        () => context.xulBrowser.contentTitle === "capture:undefined:undefined",
        "capture namespace isolation",
      );
    } finally {
      await addon?.uninstall();
      await IOUtils.remove(path, { ignoreAbsent: true });
    }
  }
}
export async function runAllTests(): Promise<void> {
  await runTests("CaptureNative.test.mts", [{
    name:
      "real tab video token feeds Offscreen MediaRecorder across background shutdown",
    fn: testCapture,
  }, {
    name: "ordinary and spoofed Firefox extensions cannot see capture APIs",
    fn: testNamespaceIsolation,
  }]);
}
