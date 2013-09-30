/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let wrapper = {};
Cu.import("resource:///modules/sessionstore/TabStateCache.jsm", wrapper);
let {TabStateCache} = wrapper;

// The number of tabs that are present in the browser but that we're not dealing
// with. This should be one (for an empty about:blank), but let's not make this
// a magic number.
let numberOfUntrackedTabs;

// Arbitrary URL prefix, used to generate the URL of pages we visit
const URL_PREFIX = "http://example.org:80/";

/**
 * Check tab state cache telemetry statistics before and after an operation.
 *
 * @param {function} f The operation being measured. If it returns a promise,
 * we wait until the promise is resolved before proceeding.
 * @return {promise}
 */
function getTelemetryDelta(f) {
  return Task.spawn(function() {
    let KEYS = ["hits", "misses", "clears"];
    let old = {};
    for (let key of KEYS) {
      old[key] = TabStateCache[key];
    }
    yield f();
    let result = {};
    for (let key of KEYS) {
      result[key] = TabStateCache[key] - old[key];
    }
    ok(result.hits >= 0, "Sanity check: hits have not decreased");
    ok(result.misses >= 0, "Sanity check: misses have not decreased");
    ok(result.clears >= 0, "Sanity check: clears have not decreased");
    throw new Task.Result(result);
  });
}

add_task(function init() {
  // Start with an empty cache
  closeAllButPrimaryWindow();
  TabStateCache.clear();
  numberOfUntrackedTabs = gBrowser.tabs.length;
  info("Starting with " + numberOfUntrackedTabs + " tabs");
});

add_task(function add_remove() {
  info("Adding the first tab");
  // Initialize one tab, save to initialize cache
  let tab1 = gBrowser.addTab(URL_PREFIX + "?tab1");
  yield promiseBrowserLoaded(tab1.linkedBrowser);
  yield getTelemetryDelta(forceSaveState);

  // Save/collect again a few times, ensure that we always hit
  info("Save/collect a few times with one tab");
  for (let collector of [forceSaveState, ss.getBrowserState]) {
    for (let i = 0; i < 5; ++i) {
      let PREFIX = "Trivial test " + i + " using " + collector.name + ": ";
      let delta = yield getTelemetryDelta(collector);
      is(delta.hits, numberOfUntrackedTabs + 1, PREFIX + " has at least one hit " + delta.hits);
      is(delta.misses, 0, PREFIX + " has no miss");
      is(delta.clears, 0, PREFIX + " has no clear");
    }
  }

  // Add a second tab, ensure that we have both hits and misses
  info("Adding the second tab");
  let tab2 = gBrowser.addTab(URL_PREFIX + "?tab2");
  yield promiseBrowserLoaded(tab2.linkedBrowser);

  let PREFIX = "Adding second tab: ";
  ok(!TabStateCache.has(tab2), PREFIX + " starts out of the cache");
  let delta = yield getTelemetryDelta(forceSaveState);
  is(delta.hits, numberOfUntrackedTabs + 2, PREFIX + " we hit all tabs, thanks to prefetching");
  is(delta.misses, 0, PREFIX + " we missed no tabs, thanks to prefetching");
  is(delta.clears, 0, PREFIX + " has no clear");

  // Save/collect again a few times, ensure that we always hit
  info("Save/collect a few times with two tabs");
  for (let collector of [forceSaveState, ss.getBrowserState]) {
    for (let i = 0; i < 5; ++i) {
      let PREFIX = "With two tabs " + i + " using " + collector.name + ": ";
      let delta = yield getTelemetryDelta(collector);
      is(delta.hits, numberOfUntrackedTabs + 2, PREFIX + " both tabs hit");
      is(delta.misses, 0, PREFIX + " has no miss");
      is(delta.clears, 0, PREFIX + " has no clear");
    }
  }

  info("Removing second tab");
  gBrowser.removeTab(tab2);
  PREFIX = "Removing second tab: ";
  delta = yield getTelemetryDelta(forceSaveState);
  is(delta.hits, numberOfUntrackedTabs + 1, PREFIX + " we hit for one tab");
  is(delta.misses, 0, PREFIX + " has no miss");
  is(delta.clears, 0, PREFIX + " has no clear");

  info("Removing first tab");
  gBrowser.removeTab(tab1);
});

add_task(function browsing() {
  info("Opening first browsing tab");
  let tab1 = gBrowser.addTab(URL_PREFIX + "?do_not_move_from_here");
  let browser1 = tab1.linkedBrowser;
  yield promiseBrowserLoaded(browser1);
  yield forceSaveState();

  info("Opening second browsing tab");
  let tab2 = gBrowser.addTab(URL_PREFIX + "?start_browsing_from_here");
  let browser2 = tab2.linkedBrowser;
  yield promiseBrowserLoaded(browser2);

  for (let i = 0; i < 4; ++i) {
    let url = URL_PREFIX + "?browsing" + i; // Arbitrary url, easy to recognize
    let PREFIX = "Browsing to " + url;
    info(PREFIX);
    let delta = yield getTelemetryDelta(function() {
      return Task.spawn(function() {
        // Move to new URI then save session
        let promise = promiseBrowserLoaded(browser2);
        browser2.loadURI(url);
        yield promise;
        ok(!TabStateCache.has(browser2), PREFIX + " starts out of the cache");
        yield forceSaveState();
      });
    });
    is(delta.hits, numberOfUntrackedTabs + 2, PREFIX + " has all hits, thanks to prefetching");
    is(delta.misses, 0, PREFIX + " has no miss, thanks to prefetching");
    is(delta.clears, 0, PREFIX + " has no clear");
  }
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab1);
});


