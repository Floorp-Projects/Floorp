/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var HiddenFrame = ChromeUtils.import(
  "resource://gre/modules/HiddenFrame.jsm",
  {}
).HiddenFrame;

const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * Create a frame in the |hiddenDOMWindow| to host a |browser|, then load the URL in the
 * latter.
 *
 * @param aURL
 *        The URL to open in the browser.
 **/
function createHiddenBrowser(aURL) {
  let frame = new HiddenFrame();
  return new Promise(resolve =>
    frame.get().then(aFrame => {
      let doc = aFrame.document;
      let browser = doc.createElementNS(XUL_NS, "browser");
      browser.setAttribute("type", "content");
      browser.setAttribute("disableglobalhistory", "true");
      browser.setAttribute("src", aURL);

      doc.documentElement.appendChild(browser);
      resolve({ frame, browser });
    })
  );
}

/**
 * Remove the browser and the HiddenFrame.
 *
 * @param aFrame
 *        The HiddenFrame to dismiss.
 * @param aBrowser
 *        The browser to dismiss.
 */
function destroyHiddenBrowser(aFrame, aBrowser) {
  // Dispose of the hidden browser.
  aBrowser.remove();

  // Take care of the frame holding our invisible browser.
  aFrame.destroy();
}

/**
 * Test that UITour works when called when no tabs are available (e.g., when using windowless
 * browsers).
 */
add_task(async function test_windowless_UITour() {
  // Get the URL for the test page.
  let pageURL = getRootDirectory(gTestPath) + "uitour.html";

  // Allow the URL to use the UITour.
  info("Adding UITour permission to the test page.");
  PermissionTestUtils.add(pageURL, "uitour", Services.perms.ALLOW_ACTION);

  // UITour's ping will resolve this promise.
  await new Promise(resolve => {
    // Create a windowless browser and test that UITour works in it.
    let browserPromise = createHiddenBrowser(pageURL);
    browserPromise.then(frameInfo => {
      isnot(frameInfo.browser, null, "The browser must exist and not be null.");

      // Load UITour frame script.
      frameInfo.browser.messageManager.loadFrameScript(
        "chrome://browser/content/content-UITour.js",
        false
      );

      // When the page loads, try to use UITour API.
      frameInfo.browser.addEventListener(
        "load",
        function loadListener() {
          info("The test page was correctly loaded.");

          frameInfo.browser.removeEventListener("load", loadListener, true);

          // Get a reference to the UITour API.
          info("Testing access to the UITour API.");
          let contentWindow = Cu.waiveXrays(
            frameInfo.browser.contentDocument.defaultView
          );
          isnot(
            contentWindow,
            null,
            "The content window must exist and not be null."
          );

          let uitourAPI = contentWindow.Mozilla.UITour;

          // Test the UITour API with a ping.
          uitourAPI.ping(function() {
            info("Ping response received from the UITour API.");

            // Make sure to clean up.
            destroyHiddenBrowser(frameInfo.frame, frameInfo.browser);

            // Resolve our promise.
            resolve();
          });
        },
        true
      );
    });

    // Wait for the UITour ping to complete.
  });
});
