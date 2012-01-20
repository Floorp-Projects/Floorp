/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource:///modules/PageThumbs.jsm");

registerCleanupFunction(function () {
  while (gBrowser.tabs.length > 1)
    gBrowser.removeTab(gBrowser.tabs[1]);
});

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
  whenBrowserLoaded(tab.linkedBrowser);
}

/**
 * Loads a new URI into the currently selected tab.
 * @param aURI The URI to load.
 */
function navigateTo(aURI) {
  let browser = gBrowser.selectedTab.linkedBrowser;
  whenBrowserLoaded(browser);
  browser.loadURI(aURI);
}

/**
 * Continues the current test execution when a load event for the given browser
 * has been received
 * @param aBrowser The browser to listen on.
 */
function whenBrowserLoaded(aBrowser) {
  aBrowser.addEventListener("load", function onLoad() {
    aBrowser.removeEventListener("load", onLoad, true);
    executeSoon(next);
  }, true);
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
