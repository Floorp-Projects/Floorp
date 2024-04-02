"use strict";

/* import-globals-from ../eme_standalone.js */

// Opens a tab containing a blank page, returns a promise that will resolve
// to that tab.
function openTab() {
  const emptyPageUri =
    "https://example.com/browser/dom/media/test/browser/file_empty_page.html";
  return BrowserTestUtils.openNewForegroundTab(window.gBrowser, emptyPageUri);
}

// Creates and configures a video element for non-MSE playback in `tab`. Does not
// start playback for the element. Returns a promise that will resolve once
// the element is setup and ready for playback.
function loadVideo(tab, extraEvent = undefined) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [extraEvent],
    async _extraEvent => {
      let video = content.document.createElement("video");
      video.id = "media";
      content.document.body.appendChild(video);

      video.src = "gizmo.mp4";
      video.load();

      info(`waiting 'loadeddata' event to ensure playback is ready`);
      let promises = [];
      promises.push(new Promise(r => (video.onloadeddata = r)));
      if (_extraEvent != undefined) {
        info(
          `waiting '${_extraEvent}' event to ensure the probe has been recorded`
        );
        promises.push(
          new Promise(r =>
            video.addEventListener(_extraEvent, r, { once: true })
          )
        );
      }
      await Promise.allSettled(promises);
    }
  );
}

// Creates and configures a video element for MSE playback in `tab`. Does not
// start playback for the element. Returns a promise that will resolve once
// the element is setup and ready for playback.
function loadMseVideo(tab, extraEvent = undefined) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [extraEvent],
    async _extraEvent => {
      async function once(target, name) {
        return new Promise(r =>
          target.addEventListener(name, r, { once: true })
        );
      }

      let video = content.document.createElement("video");
      video.id = "media";
      content.document.body.appendChild(video);

      info(`starting setup MSE`);
      const ms = new content.wrappedJSObject.MediaSource();
      video.src = content.wrappedJSObject.URL.createObjectURL(ms);
      await once(ms, "sourceopen");
      const sb = ms.addSourceBuffer("video/mp4");
      const videoFile = "bipbop2s.mp4";
      let fetchResponse = await content.fetch(videoFile);
      sb.appendBuffer(await fetchResponse.arrayBuffer());
      await once(sb, "updateend");
      ms.endOfStream();
      await once(ms, "sourceended");

      info(`waiting 'loadeddata' event to ensure playback is ready`);
      let promises = [];
      promises.push(once(video, "loadeddata"));
      if (_extraEvent != undefined) {
        info(
          `waiting '${_extraEvent}' event to ensure the probe has been recorded`
        );
        promises.push(
          new Promise(r =>
            video.addEventListener(_extraEvent, r, { once: true })
          )
        );
      }
      await Promise.allSettled(promises);
    }
  );
}

// Creates and configures a video element for EME playback in `tab`. Does not
// start playback for the element. Returns a promise that will resolve once
// the element is setup and ready for playback.
function loadEmeVideo(tab, extraEvent = undefined) {
  const emeHelperUri =
    gTestPath.substr(0, gTestPath.lastIndexOf("/")) + "/eme_standalone.js";
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [emeHelperUri, extraEvent],
    async (_emeHelperUri, _extraEvent) => {
      async function once(target, name) {
        return new Promise(r =>
          target.addEventListener(name, r, { once: true })
        );
      }

      // Helper to clone data into content so the EME helper can use the data.
      function cloneIntoContent(data) {
        return Cu.cloneInto(data, content.wrappedJSObject);
      }

      info(`starting setup EME`);
      Services.scriptloader.loadSubScript(_emeHelperUri, content);
      let video = content.document.createElement("video");
      video.id = "media";
      content.document.body.appendChild(video);
      let emeHelper = new content.wrappedJSObject.EmeHelper();
      emeHelper.SetKeySystem(
        content.wrappedJSObject.EmeHelper.GetClearkeyKeySystemString()
      );
      emeHelper.SetInitDataTypes(cloneIntoContent(["webm"]));
      emeHelper.SetVideoCapabilities(
        cloneIntoContent([{ contentType: 'video/webm; codecs="vp9"' }])
      );
      emeHelper.AddKeyIdAndKey(
        "2cdb0ed6119853e7850671c3e9906c3c",
        "808b9adac384de1e4f56140f4ad76194"
      );
      emeHelper.onerror = error => {
        is(false, `Got unexpected error from EME helper: ${error}`);
      };
      await emeHelper.ConfigureEme(video);

      info(`starting setup MSE`);
      const ms = new content.wrappedJSObject.MediaSource();
      video.src = content.wrappedJSObject.URL.createObjectURL(ms);
      await once(ms, "sourceopen");
      const sb = ms.addSourceBuffer("video/webm");
      const videoFile = "sintel-short-clearkey-subsample-encrypted-video.webm";
      let fetchResponse = await content.fetch(videoFile);
      sb.appendBuffer(await fetchResponse.arrayBuffer());
      await once(sb, "updateend");
      ms.endOfStream();
      await once(ms, "sourceended");

      info(`waiting 'loadeddata' event to ensure playback is ready`);
      let promises = [];
      promises.push(once(video, "loadeddata"));
      if (_extraEvent != undefined) {
        info(
          `waiting '${_extraEvent}' event to ensure the probe has been recorded`
        );
        promises.push(
          new Promise(r =>
            video.addEventListener(_extraEvent, r, { once: true })
          )
        );
      }
      await Promise.allSettled(promises);
    }
  );
}
