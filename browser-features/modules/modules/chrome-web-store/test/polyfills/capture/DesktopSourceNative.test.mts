// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import type {
  DesktopCaptureAddon,
  DesktopCaptureAddonManager,
  DesktopCaptureAssertEquals,
  DesktopCaptureExtensionParent,
  DesktopCaptureWindow,
  DesktopSourceModule,
  DesktopSourcePage,
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
  const limit = Date.now() + 20000;
  while (Date.now() < limit) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error("Timed out waiting for " + label);
}
async function desktopSource() {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: DesktopCaptureAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: DesktopCaptureExtensionParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { DesktopSourcePage } = ChromeUtils.importESModule(
    "resource://gre/modules/ChromeExtensionDesktopSource.sys.mjs",
  ) as unknown as DesktopSourceModule;
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as DesktopCaptureWindow;
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const target = window.gBrowser.addTab("about:blank", {
    triggeringPrincipal: principal,
  });
  window.gBrowser.selectedTab = target;
  const originalId = "gggggggggggggggggggggggggggggggg",
    id = originalId + "@chromium-extension";
  const xpi = PathUtils.join(
    PathUtils.tempDir,
    "floorp-desktop-source-" + Services.uuid.generateUUID() + ".xpi",
  );
  const html = PathUtils.join(
    PathUtils.tempDir,
    "floorp-desktop-tone-" + Services.uuid.generateUUID() + ".html",
  );
  const autoplay = Services.prefs.prefHasUserValue("media.autoplay.default")
    ? Services.prefs.getIntPref("media.autoplay.default")
    : null;
  let addon: DesktopCaptureAddon | undefined,
    page: DesktopSourcePage | undefined,
    peer: RTCPeerConnection | undefined;
  try {
    Services.prefs.setIntPref("media.autoplay.default", 0);
    await IOUtils.writeUTF8(
      html,
      '<!doctype html><title>desktop-source-target</title><body style="background:rgb(0,255,0)"><script>let value=254;setInterval(()=>document.body.style.background="rgb(0,"+(value=509-value)+",0)",80);window.context=new AudioContext();window.osc=context.createOscillator();osc.frequency.value=659.25;osc.connect(context.destination);osc.start();context.resume();</script>',
    );
    target.linkedBrowser.fixupAndLoadURIString(
      Services.io.newFileURI(new FileUtils.File(html)).spec,
      { triggeringPrincipal: principal },
    );
    await until(
      () => target.linkedBrowser.contentTitle === "desktop-source-target",
      "target document",
    );
    const files = {
      "manifest.json": JSON.stringify({
        manifest_version: 3,
        name: "Native desktop source fixture",
        version: "1.0",
        permissions: ["tabCapture", "desktopCapture"],
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
        "browser.action.onClicked.addListener(()=>{document.title='invoked';});",
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
      "extension",
    );
    await until(() => extension.backgroundContext, "background");
    if (window.navigator.mediaDevices.mozFloorpCapturePickerVersion !== 1) {
      page = new DesktopSourcePage(extension);
      await page.open();
      let rejected = false;
      try {
        await page.select({
          kind: "screen",
          pickerToken: "unregistered-test-picker",
        });
      } catch (error) {
        rejected = /Runtime with the native picker bridge/.test(
          (error as Error).message,
        );
      }
      assert(
        rejected,
        "script-only artifact rejects before unprompted native desktop capture",
      );
      assert(page.unloaded, "failed picker releases private source host");
    }
    page = new DesktopSourcePage(extension);
    await page.open();
    assertEquals(
      page.windowGlobal!.documentPrincipal.originNoSuffix,
      extension.baseURI.prePath,
      "private source document belongs to extension",
    );
    const sourcePrincipal = page.windowGlobal!.documentPrincipal;
    const expiry = Date.now() + 10000;
    assertEquals(
      Services.perms.testExactPermissionFromPrincipal(
        sourcePrincipal,
        "MediaManagerVideo",
      ),
      Services.perms.UNKNOWN_ACTION,
      "no pre-existing backend grant",
    );
    Services.perms.addFromPrincipal(
      sourcePrincipal,
      "MediaManagerVideo",
      Services.perms.ALLOW_ACTION!,
      Services.perms.EXPIRE_TIME!,
      expiry,
    );
    const selected = await page.select({
      kind: "tab",
      browserId: target.linkedBrowser.browsingContext.browserId,
      audioTab: target,
    });
    assert(
      selected.video && selected.audio,
      "real tab selection includes native video and target audio",
    );
    const sessionId = String(Services.uuid.generateUUID());
    const offer = await page.offer(sessionId);
    peer = new window.RTCPeerConnection({ iceServers: [] });
    const stream = new window.MediaStream();
    peer.addEventListener("track", (event) => stream.addTrack(event.track));
    await peer.setRemoteDescription(offer);
    await peer.setLocalDescription(await peer.createAnswer());
    await until(() => peer!.iceGatheringState === "complete", "receiver ICE");
    await page.answer(sessionId, peer.localDescription!.toJSON());
    await until(
      () => peer!.connectionState === "connected",
      "real source RTP transport",
    );
    (assertEquals as DesktopCaptureAssertEquals)(
      stream.getVideoTracks().length,
      1,
    );
    (assertEquals as DesktopCaptureAssertEquals)(
      stream.getAudioTracks().length,
      1,
    );
    await extension.terminateBackground();
    assert(!page.unloaded, "source host survives background suspension");
    const video = window.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "video",
    ) as HTMLVideoElement;
    video.muted = true;
    video.srcObject = stream;
    await video.play();
    const audio = new window.AudioContext();
    const analyser = audio.createAnalyser();
    analyser.fftSize = 8192;
    audio.createMediaStreamSource(stream).connect(analyser);
    await audio.resume();
    await new Promise((resolve) => setTimeout(resolve, 600));
    const samples = new Float32Array(analyser.fftSize);
    analyser.getFloatTimeDomainData(samples);
    const rms = Math.sqrt(
      samples.reduce((sum, value) => sum + value * value, 0) / samples.length,
    );
    const frequencies = new Float32Array(analyser.frequencyBinCount);
    analyser.getFloatFrequencyData(frequencies);
    let peak = 0;
    for (let i = 1; i < frequencies.length; i++) {
      if (frequencies[i] > frequencies[peak]) peak = i;
    }
    const frequency = peak * audio.sampleRate / analyser.fftSize;
    const canvas = window.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "canvas",
    ) as HTMLCanvasElement;
    canvas.width = 2;
    canvas.height = 2;
    const context = canvas.getContext("2d");
    context!.drawImage(video, 0, 0, 2, 2);
    const pixel = Array.from(context!.getImageData(1, 1, 1, 1).data);
    let bytes = 0;
    const recorder = new window.MediaRecorder(stream, {
      mimeType: "video/webm;codecs=vp8,opus",
    });
    const done = new Promise<Event>((resolve) => {
      recorder.ondataavailable = (event) => bytes += event.data.size;
      recorder.onstop = resolve;
    });
    recorder.start();
    await new Promise((resolve) => setTimeout(resolve, 400));
    recorder.stop();
    await done;
    assert(
      pixel[1] > 200 && pixel[0] < 20,
      "source bridge carries real selected tab pixels",
    );
    assert(
      rms > 0.05 && Math.abs(frequency - 659.25) < 25,
      "source bridge carries selected tab's 659Hz audio",
    );
    assert(bytes > 1000, "source output is real video+audio WebM");
    await IOUtils.writeUTF8(
      PathUtils.join(
        PathUtils.tempDir,
        "floorp-desktop-source-native-result.json",
      ),
      JSON.stringify({ bytes, pixel, rms, frequency }),
    );
    let closes = 0;
    page.onClose = () => closes++;
    page.close();
    page.close();
    assertEquals(closes, 1, "source closes once");
    stream.getTracks().forEach((track) => track.stop());
    await audio.close();
  } finally {
    peer?.close();
    page?.close();
    await addon?.uninstall();
    window.gBrowser.removeTab(target);
    await IOUtils.remove(html, { ignoreAbsent: true });
    await IOUtils.remove(xpi, { ignoreAbsent: true });
    if (autoplay === null) {
      Services.prefs.clearUserPref("media.autoplay.default");
    } else Services.prefs.setIntPref("media.autoplay.default", autoplay);
  }
}
export async function runAllTests() {
  await runTests("DesktopSourceNative.test.mts", [{
    name:
      "private desktop source transports actual consented tab audio and video",
    fn: desktopSource,
  }]);
}
