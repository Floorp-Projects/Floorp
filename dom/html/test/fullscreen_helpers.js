/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URLS = [
  // all frames are in different process.
  `data:text/html,
    <div name="div" id="div" style="width: 100px; height: 100px; background: red;">
    <iframe id="iframe" allowfullscreen="yes"
     src="http://mochi.test:8888/browser/dom/html/test/file_fullscreen-iframe-middle.html"></iframe>
    </div>`,
  // toplevel and inner most iframe are in same process, and middle iframe is
  // in a different process.
  `http://example.org/browser/dom/html/test/file_fullscreen-iframe-top.html`,
  // toplevel and middle iframe are in same process, and inner most iframe is
  // in a different process.
  `http://mochi.test:8888/browser/dom/html/test/file_fullscreen-iframe-top.html`,
];

function waitFullscreenEvent(aDocument, aIsInFullscreen, aWaitUntil = false) {
  return new Promise(resolve => {
    function errorHandler() {
      ok(false, "should not get fullscreenerror event");
      aDocument.removeEventListener("fullscreenchange", changeHandler);
      aDocument.removeEventListener("fullscreenerror", errorHandler);
      resolve();
    }

    function changeHandler() {
      if (aWaitUntil && aIsInFullscreen != !!aDocument.fullscreenElement) {
        return;
      }

      is(
        aIsInFullscreen,
        !!aDocument.fullscreenElement,
        "check fullscreen (event)"
      );
      aDocument.removeEventListener("fullscreenchange", changeHandler);
      aDocument.removeEventListener("fullscreenerror", errorHandler);
      resolve();
    }

    aDocument.addEventListener("fullscreenchange", changeHandler);
    aDocument.addEventListener("fullscreenerror", errorHandler);
  });
}

function waitForFullScreenObserver(
  aDocument,
  aIsInFullscreen,
  aWaitUntil = false
) {
  return TestUtils.topicObserved("fullscreen-painted", (subject, data) => {
    if (
      aWaitUntil &&
      aIsInFullscreen !=
        aDocument.documentElement.hasAttribute("inDOMFullscreen")
    ) {
      return false;
    }

    is(
      aIsInFullscreen,
      aDocument.documentElement.hasAttribute("inDOMFullscreen"),
      "check fullscreen (observer)"
    );
    return true;
  });
}

function waitForFullscreenState(
  aDocument,
  aIsInFullscreen,
  aWaitUntil = false
) {
  return Promise.all([
    waitFullscreenEvent(aDocument, aIsInFullscreen, aWaitUntil),
    waitForFullScreenObserver(aDocument, aIsInFullscreen, aWaitUntil),
  ]);
}
