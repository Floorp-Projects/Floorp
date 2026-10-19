// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function waitFor(read, label) {
  const deadline = Date.now() + 30000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error("Timed out waiting for " + label);
}
function audioFrameServer() {
  const { NetUtil } = ChromeUtils.importESModule(
    "resource://gre/modules/NetUtil.sys.mjs",
  );
  const server = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket,
  );
  const connections = new Set();
  server.init(-1, true, -1);
  server.asyncListen({
    onSocketAccepted(_server, transport) {
      connections.add(transport);
      const input = transport.openInputStream(0, 0, 0);
      const pump = Cc["@mozilla.org/network/input-stream-pump;1"]
        .createInstance(Ci.nsIInputStreamPump);
      let request = "", sent = false;
      pump.init(input, 0, 0, true);
      pump.asyncRead({
        onStartRequest() {},
        onDataAvailable(_request, stream, _offset, count) {
          request += NetUtil.readInputStreamToString(stream, count);
          if (sent || !request.includes("\r\n\r\n")) return;
          sent = true;
          const body =
            "<!doctype html><title>frame-tone</title><script>window.ac=new AudioContext();window.tone=ac.createOscillator();window.gain=ac.createGain();gain.gain.value=0.5;tone.frequency.value=880;tone.connect(gain);gain.connect(ac.destination);tone.start();ac.resume();</script>";
          const response =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
            body.length + "\r\nConnection: close\r\n\r\n" + body;
          const output = transport.openOutputStream(0, 0, 0);
          output.write(response, response.length);
          output.close();
        },
        onStopRequest() {
          connections.delete(transport);
        },
      });
    },
    onStopListening() {},
  });
  return {
    port: server.port,
    close() {
      server.close();
      for (const connection of connections) {
        connection.close(
          Cr.NS_BINDING_ABORTED,
        );
      }
      connections.clear();
    },
  };
}
async function audioCapture() {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  const source = window.gBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  window.gBrowser.selectedTab = source;
  const originalId = "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
    id = originalId + "@chromium-extension";
  const xpi = PathUtils.join(
    PathUtils.tempDir,
    "floorp-audio-" + Services.uuid.generateUUID() + ".xpi",
  );
  const html = PathUtils.join(
    PathUtils.tempDir,
    "floorp-audio-" + Services.uuid.generateUUID() + ".html",
  );
  const autoplay = Services.prefs.prefHasUserValue("media.autoplay.default")
    ? Services.prefs.getIntPref("media.autoplay.default")
    : null;
  let addon;
  const frameServer = audioFrameServer();
  try {
    Services.prefs.setIntPref("media.autoplay.default", 0);
    await IOUtils.writeUTF8(
      html,
      '<!doctype html><title>target-tone</title><script>window.ac=new AudioContext();window.tone=ac.createOscillator();tone.frequency.value=523.25;tone.connect(ac.destination);tone.start();ac.resume();window.addEventListener("message",event=>{if(event.data.frame){const frame=document.createElement("iframe");frame.allow="display-capture *";frame.src=event.data.frame;document.body.append(frame);}});</script>',
    );
    source.linkedBrowser.fixupAndLoadURIString(
      Services.io.newFileURI(new FileUtils.File(html)).spec,
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    await waitFor(
      () => source.linkedBrowser.contentTitle === "target-tone",
      "real source document",
    );
    const files = {
      "manifest.json": JSON.stringify({
        manifest_version: 3,
        name: "Native tab audio",
        version: "1.0",
        permissions: ["tabCapture", "offscreen"],
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
        `browser.action.onClicked.addListener(async()=>{try{const token=await browser.tabCapture.getMediaStreamId();if(!await browser.offscreen.hasDocument())await browser.offscreen.createDocument({url:"offscreen.html",reasons:["USER_MEDIA"],justification:"Record actual authorized tab audio"});await browser.runtime.sendMessage({token});}catch(error){document.title="FAIL:"+error.name+":"+error.message;}});`,
      "offscreen.html": '<script src="offscreen.js"></script>',
      "offscreen.js": `
    browser.runtime.onMessage.addListener(message=>{if(message.token)record(message.token);});
    async function record(token){let phase="consume";try{
     const stream=await navigator.mediaDevices.getUserMedia({video:false,audio:{mandatory:{chromeMediaSource:"tab",chromeMediaSourceId:token}}});
     phase="analyse";const context=new AudioContext();const analyser=context.createAnalyser();analyser.fftSize=8192;context.createMediaStreamSource(stream).connect(analyser);await context.resume();
     let bytes=0;const recorder=new MediaRecorder(stream,{mimeType:"audio/webm;codecs=opus"});const done=new Promise((resolve,reject)=>{recorder.ondataavailable=event=>bytes+=event.data.size;recorder.onstop=resolve;recorder.onerror=reject});recorder.start(100);document.title=JSON.stringify({phase:"recording"});
     await new Promise(resolve=>setTimeout(resolve,1300));const samples=new Float32Array(analyser.fftSize);analyser.getFloatTimeDomainData(samples);const rms=Math.sqrt(samples.reduce((sum,value)=>sum+value*value,0)/samples.length);
     const frequencies=new Float32Array(analyser.frequencyBinCount);analyser.getFloatFrequencyData(frequencies);let peakIndex=0;for(let i=1;i<frequencies.length;i++)if(frequencies[i]>frequencies[peakIndex])peakIndex=i;const peak=peakIndex*context.sampleRate/analyser.fftSize;
     const frameBin=Math.round(880*analyser.fftSize/context.sampleRate);const frameLevel=Math.max(...frequencies.slice(frameBin-2,frameBin+3));
     await new Promise(resolve=>setTimeout(resolve,700));if(recorder.state!=="inactive")recorder.stop();await done;
     const endedBeforeManualStop=stream.getTracks().every(track=>track.readyState==="ended");stream.getTracks().forEach(track=>track.stop());await context.close();
     document.title=JSON.stringify({phase:"done",rms,peak,frameLevel,bytes,audioTracks:stream.getAudioTracks().length,videoTracks:stream.getVideoTracks().length,endedBeforeManualStop});
    }catch(error){document.title="FAIL:"+phase+":"+error.name+":"+error.message;}}
   `,
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
      "native extension",
    );
    await waitFor(() => extension.backgroundContext, "background document");
    const action = await extension.apiManager.asyncGetAPI(
      "browserAction",
      extension,
      "addon_parent",
    );
    const capture = await extension.apiManager.asyncGetAPI(
      "tabCapture",
      extension,
      "addon_parent",
    );
    const offscreenModule = await extension.apiManager.asyncGetAPI(
      "offscreen",
      extension,
      "addon_parent",
    );
    const title = () => {
      const background = extension.backgroundContext?.xulBrowser.contentTitle;
      if (background?.startsWith("FAIL:")) throw new Error(background);
      const host = [...extension.views].find((view) =>
        view.viewType === "offscreen"
      );
      const value = host?.xulBrowser.contentTitle;
      if (value?.startsWith("FAIL:")) throw new Error(value);
      return value;
    };
    const run = async (shouldNavigate = false, withFrame = false) => {
      action.action.triggerClickOrPopup(source);
      await waitFor(
        () => title()?.includes('"phase":"recording"'),
        "actual offscreen audio stream",
      );
      const host = [...extension.views].find((view) =>
        view.viewType === "offscreen"
      );
      await extension.terminateBackground();
      if (withFrame) {
        source.linkedBrowser.messageManager.loadFrameScript(
          "data:application/javascript," +
            encodeURIComponent(
              `content.postMessage({frame:"http://127.0.0.1:${frameServer.port}/tone"},"*")`,
            ),
          false,
        );
        await waitFor(
          () =>
            source.linkedBrowser.browsingContext.children.some((frame) =>
              frame.currentWindowGlobal?.documentURI?.spec.startsWith(
                "http://127.0.0.1",
              )
            ),
          "new Fission audio frame",
        );
        const frame = source.linkedBrowser.browsingContext.children.find(
          (frame) =>
            frame.currentWindowGlobal?.documentURI?.spec.startsWith(
              "http://127.0.0.1",
            ),
        );
        assert(
          frame.currentWindowGlobal.osPid !==
            source.linkedBrowser.browsingContext.currentWindowGlobal.osPid,
          "audio source frame uses a separate native process",
        );
      }
      if (shouldNavigate) {
        source.linkedBrowser.fixupAndLoadURIString("about:blank", {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });
      }
      await waitFor(
        () => title()?.includes('"phase":"done"'),
        "recorded audio WebM",
      );
      const result = JSON.parse(title());
      await IOUtils.writeUTF8(
        PathUtils.join(
          PathUtils.tempDir,
          withFrame
            ? "floorp-tab-audio-frame-result.json"
            : shouldNavigate
            ? "floorp-tab-audio-stop.json"
            : "floorp-tab-audio-result.json",
        ),
        JSON.stringify(result),
      );
      assertEquals(result.audioTracks, 1, "native audio track");
      assertEquals(result.videoTracks, 0, "audio-only request");
      if (!shouldNavigate) {
        assert(result.rms > 0.05, "recording contains real target waveform");
        assert(
          Math.abs(result.peak - 523.25) < 25,
          "waveform belongs to the 523Hz target source",
        );
        assert(result.bytes > 1000, "real audio WebM output");
        if (withFrame) {
          assert(
            result.frameLevel > -35,
            "new cross-process iframe's 880Hz audio joins the actual recording",
          );
        }
      } else {assert(
          result.endedBeforeManualStop,
          "target navigation ends actual audio track",
        );}
      await IOUtils.writeUTF8(
        PathUtils.join(
          PathUtils.tempDir,
          shouldNavigate
            ? "floorp-tab-audio-stop.json"
            : "floorp-tab-audio-result.json",
        ),
        JSON.stringify(result),
      );
      assert(
        [...extension.views].includes(host),
        "offscreen survives background termination",
      );
      await waitFor(
        async () =>
          !(await capture.getAPI(host).tabCapture.getCapturedTabs()).length,
        "native source cleanup",
      );
    };
    await run();
    await run(false, true);
    await run(true);
    const host = [...extension.views].find((view) =>
      view.viewType === "offscreen"
    );
    await offscreenModule.getAPI(host).offscreen.closeDocument();
  } finally {
    await addon?.uninstall();
    frameServer.close();
    window.gBrowser.removeTab(source);
    await IOUtils.remove(xpi, { ignoreAbsent: true });
    await IOUtils.remove(html, { ignoreAbsent: true });
    if (autoplay === null) {
      Services.prefs.clearUserPref("media.autoplay.default");
    } else Services.prefs.setIntPref("media.autoplay.default", autoplay);
  }
}
export async function runAllTests() {
  await runTests("CaptureAudioNative.test.mts", [{
    name: "native target tab audio reaches Offscreen and stops on navigation",
    fn: audioCapture,
  }]);
}
