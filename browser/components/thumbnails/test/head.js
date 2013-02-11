/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tmp = {};
Cu.import("resource:///modules/PageThumbs.jsm", tmp);
let PageThumbs = tmp.PageThumbs;
let PageThumbsStorage = tmp.PageThumbsStorage;

Cu.import("resource://gre/modules/PlacesUtils.jsm");

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

    if (this._iter)
      this.next();
    else
      finish();
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
 * @param aCallback The function to call when the tab has loaded.
 */
function addTab(aURI, aCallback) {
  let tab = gBrowser.selectedTab = gBrowser.addTab(aURI);
  whenLoaded(tab.linkedBrowser, aCallback);
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

  let htmlns = "http://www.w3.org/1999/xhtml";
  let img = document.createElementNS(htmlns, "img");
  img.setAttribute("src", thumb);

  whenLoaded(img, function () {
    let canvas = document.createElementNS(htmlns, "canvas");
    canvas.setAttribute("width", width);
    canvas.setAttribute("height", height);

    // Draw the image to a canvas and compare the pixel color values.
    let ctx = canvas.getContext("2d");
    ctx.drawImage(img, 0, 0, width, height);
    checkCanvasColor(ctx, aRed, aGreen, aBlue, aMessage);

    next();
  });
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

/**
 * Checks if a thumbnail for the given URL exists.
 * @param aURL The url associated to the thumbnail.
 */
function thumbnailExists(aURL) {
  let file = PageThumbsStorage.getFileForURL(aURL);
  return file.exists() && file.fileSize;
}

/**
 * Asynchronously adds visits to a page, invoking a callback function when done.
 *
 * @param aPlaceInfo
 *        Can be an nsIURI, in such a case a single LINK visit will be added.
 *        Otherwise can be an object describing the visit to add, or an array
 *        of these objects:
 *          { uri: nsIURI of the page,
 *            transition: one of the TRANSITION_* from nsINavHistoryService,
 *            [optional] title: title of the page,
 *            [optional] visitDate: visit date in microseconds from the epoch
 *            [optional] referrer: nsIURI of the referrer for this visit
 *          }
 * @param [optional] aCallback
 *        Function to be invoked on completion.
 */
function addVisits(aPlaceInfo, aCallback) {
  let places = [];
  if (aPlaceInfo instanceof Ci.nsIURI) {
    places.push({ uri: aPlaceInfo });
  }
  else if (Array.isArray(aPlaceInfo)) {
    places = places.concat(aPlaceInfo);
  } else {
    places.push(aPlaceInfo)
  }

  // Create mozIVisitInfo for each entry.
  let now = Date.now();
  for (let i = 0; i < places.length; i++) {
    if (!places[i].title) {
      places[i].title = "test visit for " + places[i].uri.spec;
    }
    places[i].visits = [{
      transitionType: places[i].transition === undefined ? PlacesUtils.history.TRANSITION_LINK
                                                         : places[i].transition,
      visitDate: places[i].visitDate || (now++) * 1000,
      referrerURI: places[i].referrer
    }];
  }

  PlacesUtils.asyncHistory.updatePlaces(
    places,
    {
      handleError: function AAV_handleError() {
        throw("Unexpected error in adding visit.");
      },
      handleResult: function () {},
      handleCompletion: function UP_handleCompletion() {
        if (aCallback)
          aCallback();
      }
    }
  );
}

/**
 * Calls a given callback when the thumbnail for a given URL has been found
 * on disk. Keeps trying until the thumbnail has been created.
 *
 * @param aURL The URL of the thumbnail's page.
 * @param [optional] aCallback
 *        Function to be invoked on completion.
 */
function whenFileExists(aURL, aCallback) {
  let callback = aCallback;
  if (!thumbnailExists(aURL)) {
    callback = function () whenFileExists(aURL, aCallback);
  }

  executeSoon(callback || next);
}

/**
 * Calls a given callback when the given file has been removed.
 * Keeps trying until the file is removed.
 *
 * @param aFile The file that is being removed
 * @param [optional] aCallback
 *        Function to be invoked on completion.
 */
function whenFileRemoved(aFile, aCallback) {
  let callback = aCallback;
  if (aFile.exists()) {
    callback = function () whenFileRemoved(aFile, aCallback);
  }

  executeSoon(callback || next);
}
