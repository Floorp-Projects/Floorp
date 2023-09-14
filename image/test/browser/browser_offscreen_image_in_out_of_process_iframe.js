/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);
const parentPATH = DIRPATH + "empty.html";
const iframePATH = DIRPATH + "empty.html";

const parentURL = `http://example.com/${parentPATH}`;
const iframeURL = `http://example.org/${iframePATH}`;

add_task(async function setup_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // To avoid throttling requestAnimationFrame callbacks in invisible
      // iframes
      ["layout.throttled_frame_rate", 60],
    ],
  });
});

add_task(async function () {
  const win = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });

  try {
    const browser = win.gBrowser.selectedTab.linkedBrowser;

    BrowserTestUtils.startLoadingURIString(browser, parentURL);
    await BrowserTestUtils.browserLoaded(browser, false, parentURL);

    async function setup(url) {
      const scroller = content.document.createElement("div");
      scroller.style = "width: 300px; height: 300px; overflow: scroll;";
      scroller.setAttribute("id", "scroller");
      content.document.body.appendChild(scroller);

      // Make a space bigger than display port.
      const spacer = content.document.createElement("div");
      spacer.style = "width: 100%; height: 10000px;";
      scroller.appendChild(spacer);

      const iframe = content.document.createElement("iframe");
      scroller.appendChild(iframe);

      iframe.contentWindow.location = url;
      await new Promise(resolve => {
        iframe.addEventListener("load", resolve, { once: true });
      });

      return iframe.browsingContext;
    }

    async function setupImage() {
      const img = content.document.createElement("img");
      // This GIF is a 100ms interval animation.
      img.setAttribute("src", "animated.gif");
      img.setAttribute("id", "img");
      content.document.body.appendChild(img);

      const spacer = content.document.createElement("div");
      spacer.style = "width: 100%; height: 10000px;";
      content.document.body.appendChild(spacer);
      await new Promise(resolve => {
        img.addEventListener("load", resolve, { once: true });
      });
    }

    // Returns the count of frameUpdate during |time| (in ms) period.
    async function observeFrameUpdate(time) {
      function ImageDecoderObserverStub() {
        this.sizeAvailable = function sizeAvailable(aRequest) {};
        this.frameComplete = function frameComplete(aRequest) {};
        this.decodeComplete = function decodeComplete(aRequest) {};
        this.loadComplete = function loadComplete(aRequest) {};
        this.frameUpdate = function frameUpdate(aRequest) {};
        this.discard = function discard(aRequest) {};
        this.isAnimated = function isAnimated(aRequest) {};
        this.hasTransparency = function hasTransparency(aRequest) {};
      }

      // Start from the callback of setTimeout.
      await new Promise(resolve => content.window.setTimeout(resolve, 0));

      let frameCount = 0;
      const observer = new ImageDecoderObserverStub();
      observer.frameUpdate = () => {
        frameCount++;
      };
      observer.loadComplete = () => {
        // Ignore the frameUpdate callback along with loadComplete. It seems
        // a frameUpdate sometimes happens with a loadComplete when attatching
        // observer in fission world.
        frameCount--;
      };

      const gObserver = SpecialPowers.Cc["@mozilla.org/image/tools;1"]
        .getService(SpecialPowers.Ci.imgITools)
        .createScriptedObserver(observer);
      const img = content.document.getElementById("img");

      SpecialPowers.wrap(img).addObserver(gObserver);
      await new Promise(resolve => content.window.setTimeout(resolve, time));
      SpecialPowers.wrap(img).removeObserver(gObserver);

      return frameCount;
    }

    // Setup an iframe which is initially scrolled out.
    const iframe = await SpecialPowers.spawn(browser, [iframeURL], setup);

    // Setup a 100ms interval animated GIF image in the iframe.
    await SpecialPowers.spawn(iframe, [], setupImage);

    let frameCount = await SpecialPowers.spawn(
      iframe,
      [1000],
      observeFrameUpdate
    );
    // Bug 1577084.
    if (frameCount == 0) {
      is(frameCount, 0, "no frameupdates");
    } else {
      todo_is(frameCount, 0, "no frameupdates");
    }

    // Scroll the iframe into view.
    await SpecialPowers.spawn(browser, [], async () => {
      const scroller = content.document.getElementById("scroller");
      scroller.scrollTo({ left: 0, top: 9800, behavior: "smooth" });
      await new Promise(resolve => content.window.setTimeout(resolve, 1000));
    });

    await new Promise(resolve => requestAnimationFrame(resolve));

    frameCount = await SpecialPowers.spawn(iframe, [1000], observeFrameUpdate);
    ok(frameCount > 0, "There should be frameUpdate(s)");

    await new Promise(resolve => requestAnimationFrame(resolve));

    await SpecialPowers.spawn(iframe, [], async () => {
      const img = content.document.getElementById("img");
      // Move the image outside of the scroll port.  'position: absolute' causes
      // a relow on the image element.
      img.style = "position: absolute; top: 9000px;";
      await new Promise(resolve =>
        content.window.requestAnimationFrame(resolve)
      );
    });

    await new Promise(resolve => requestAnimationFrame(resolve));

    frameCount = await SpecialPowers.spawn(iframe, [1000], observeFrameUpdate);
    is(frameCount, 0, "No frameUpdate should happen");
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
