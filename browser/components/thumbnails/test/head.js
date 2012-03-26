/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tmp = {};
Cu.import("resource:///modules/PageThumbs.jsm", tmp);
let PageThumbs = tmp.PageThumbs;
let PageThumbsCache = tmp.PageThumbsCache;

registerCleanupFunction(function () {
  while (gBrowser.tabs.length > 1)
    gBrowser.removeTab(gBrowser.tabs[1]);
});

let cachedXULDocument;

/**
 * Provide the default test function to start our test runner.
 */
function test() {
  TestRunner.run();
}

/**
 * The test runner that controls the execution flow of our tests.
 */
let TestRunner = {
  /**
   * Starts the test runner.
   */
  run: function () {
    waitForExplicitFinish();

    this._iter = runTests();
    this.next();
  },

  /**
   * Runs the next available test or finishes if there's no test left.
   */
  next: function () {
    try {
      TestRunner._iter.next();
    } catch (e if e instanceof StopIteration) {
      finish();
    }
  }
};

/**
 * Continues the current test execution.
 */
function next() {
  TestRunner.next();
}

/**
 * Creates a new tab with the given URI.
 * @param aURI The URI that's loaded in the tab.
 */
function addTab(aURI) {
  let tab = gBrowser.selectedTab = gBrowser.addTab(aURI);
  whenLoaded(tab.linkedBrowser);
}

/**
 * Loads a new URI into the currently selected tab.
 * @param aURI The URI to load.
 */
function navigateTo(aURI) {
  let browser = gBrowser.selectedTab.linkedBrowser;
  whenLoaded(browser);
  browser.loadURI(aURI);
}

/**
 * Continues the current test execution when a load event for the given element
 * has been received.
 * @param aElement The DOM element to listen on.
 * @param aCallback The function to call when the load event was dispatched.
 */
function whenLoaded(aElement, aCallback) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(aCallback || next);
  }, true);
}

/**
 * Captures a screenshot for the currently selected tab, stores it in the cache,
 * retrieves it from the cache and compares pixel color values.
 * @param aRed The red component's intensity.
 * @param aGreen The green component's intensity.
 * @param aBlue The blue component's intensity.
 * @param aMessage The info message to print when comparing the pixel color.
 */
function captureAndCheckColor(aRed, aGreen, aBlue, aMessage) {
  let browser = gBrowser.selectedBrowser;

  // Capture the screenshot.
  PageThumbs.captureAndStore(browser, function () {
    checkThumbnailColor(browser.currentURI.spec, aRed, aGreen, aBlue, aMessage);
  });
}

/**
 * Retrieve a thumbnail from the cache and compare its pixel color values.
 * @param aURL The URL of the thumbnail's page.
 * @param aRed The red component's intensity.
 * @param aGreen The green component's intensity.
 * @param aBlue The blue component's intensity.
 * @param aMessage The info message to print when comparing the pixel color.
 */
function checkThumbnailColor(aURL, aRed, aGreen, aBlue, aMessage) {
  let width = 100, height = 100;
  let thumb = PageThumbs.getThumbnailURL(aURL, width, height);

  getXULDocument(function (aDocument) {
    let htmlns = "http://www.w3.org/1999/xhtml";
    let img = aDocument.createElementNS(htmlns, "img");
    img.setAttribute("src", thumb);

    whenLoaded(img, function () {
      let canvas = aDocument.createElementNS(htmlns, "canvas");
      canvas.setAttribute("width", width);
      canvas.setAttribute("height", height);

      // Draw the image to a canvas and compare the pixel color values.
      let ctx = canvas.getContext("2d");
      ctx.drawImage(img, 0, 0, width, height);
      checkCanvasColor(ctx, aRed, aGreen, aBlue, aMessage);

      next();
    });
  });
}

/**
 * Passes a XUL document (created if necessary) to the given callback.
 * @param aCallback The function to be called when the XUL document has been
 *                  created. The first argument will be the document.
 */
function getXULDocument(aCallback) {
  let hiddenWindow = Services.appShell.hiddenDOMWindow;
  let doc = cachedXULDocument || hiddenWindow.document;

  if (doc instanceof XULDocument) {
    aCallback(cachedXULDocument = doc);
    return;
  }

  let iframe = doc.createElement("iframe");
  iframe.setAttribute("src", "chrome://global/content/mozilla.xhtml");

  iframe.addEventListener("DOMContentLoaded", function onLoad() {
    iframe.removeEventListener("DOMContentLoaded", onLoad, false);
    aCallback(cachedXULDocument = iframe.contentDocument);
  }, false);

  doc.body.appendChild(iframe);
  registerCleanupFunction(function () { doc.body.removeChild(iframe); });
}

/**
 * Checks the top-left pixel of a given canvas' 2d context for a given color.
 * @param aContext The 2D context of a canvas.
 * @param aRed The red component's intensity.
 * @param aGreen The green component's intensity.
 * @param aBlue The blue component's intensity.
 * @param aMessage The info message to print when comparing the pixel color.
 */
function checkCanvasColor(aContext, aRed, aGreen, aBlue, aMessage) {
  let [r, g, b] = aContext.getImageData(0, 0, 1, 1).data;
  ok(r == aRed && g == aGreen && b == aBlue, aMessage);
}
