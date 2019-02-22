/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles are shown for <video> tags in the
// content process when devtools.chrome.enabled=true.

const TEST_URL = URL_ROOT + "doc_markup_events_chrome_listeners.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "video",
    expected: [
      createEvent("canplay"),
      createEvent("canplaythrough"),
      createEvent("emptied"),
      createEvent("ended"),
      createEvent("error"),
      createEvent("keypress"),
      createEvent("loadeddata"),
      createEvent("loadedmetadata"),
      createEvent("loadstart"),
      createEvent("mozvideoonlyseekbegin"),
      createEvent("mozvideoonlyseekcompleted"),
      createEvent("pause"),
      createEvent("play"),
      createEvent("playing"),
      createEvent("progress"),
      createEvent("seeked"),
      createEvent("seeking"),
      createEvent("stalled"),
      createEvent("suspend"),
      createEvent("timeupdate"),
      createEvent("volumechange"),
      createEvent("waiting"),
    ],
  },
];

function createEvent(type) {
  return {
    type: type,
    filename: "chrome://global/content/elements/videocontrols.js:437",
    attributes: [
      "Capturing",
      "DOM2",
    ],
    handler: `
      ${type === "play" ? "function" : "handleEvent"}(aEvent) {
        if (!aEvent.isTrusted) {
          this.log("Drop untrusted event ----> " + aEvent.type);
          return;
        }

        this.log("Got event ----> " + aEvent.type);

        if (this.videoEvents.includes(aEvent.type)) {
          this.handleVideoEvent(aEvent);
        } else {
          this.handleControlEvent(aEvent);
        }
      }`,
  };
}

add_task(async function() {
  await pushPref("devtools.chrome.enabled", true);
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
