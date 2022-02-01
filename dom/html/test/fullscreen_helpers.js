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

function waitRemoteFullscreenExitEvents(aBrowsingContexts) {
  let promises = [];
  aBrowsingContexts.forEach(([aBrowsingContext, aName]) => {
    promises.push(
      SpecialPowers.spawn(aBrowsingContext, [aName], async name => {
        return new Promise(resolve => {
          let document = content.document;
          document.addEventListener(
            "fullscreenchange",
            function changeHandler() {
              if (document.fullscreenElement) {
                return;
              }

              ok(true, `check remote fullscreen event (${name})`);
              document.removeEventListener("fullscreenchange", changeHandler);
              resolve();
            }
          );
        });
      })
    );
  });
  return Promise.all(promises);
}

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

// Wait for fullscreenchange event for fullscreen exit. And wait for
// fullscreen-painted observed conditionally.
async function waitForFullscreenExit(aDocument) {
  info(`waitForFullscreenExit`);
  let promiseFsObserver = null;
  let observer = function() {
    if (aDocument.documentElement.hasAttribute("inDOMFullscreen")) {
      info(`waitForFullscreenExit, fullscreen-painted, inDOMFullscreen`);
      Services.obs.removeObserver(observer, "fullscreen-painted");
      promiseFsObserver = waitForFullScreenObserver(aDocument, false);
    }
  };
  Services.obs.addObserver(observer, "fullscreen-painted");

  await waitFullscreenEvent(aDocument, false, true);
  // If there is a fullscreen-painted observer notified for inDOMFullscreen set,
  // we expect to have a subsequent fullscreen-painted observer notified with
  // inDOMFullscreen unset.
  if (promiseFsObserver) {
    info(`waitForFullscreenExit, promiseFsObserver`);
    return promiseFsObserver;
  }

  Services.obs.removeObserver(observer, "fullscreen-painted");
  // If inDOMFullscreen is set we expect to have a subsequent fullscreen-painted
  // observer notified with inDOMFullscreen unset.
  if (aDocument.documentElement.hasAttribute("inDOMFullscreen")) {
    info(`waitForFullscreenExit, inDOMFullscreen`);
    return waitForFullScreenObserver(aDocument, false, true);
  }
}
