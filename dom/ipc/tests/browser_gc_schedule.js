/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PAGE =
  "http://mochi.test:8888/browser/dom/ipc/tests/file_dummy.html";

async function waitForGCBegin() {
  var waitTopic = "garbage-collector-begin";
  var observer = {};

  info("Waiting for " + waitTopic);
  // This fixes a ReferenceError for Date, it's weird.
  ok(Date.now(), "Date.now()");
  var when = await new Promise(resolve => {
    observer.observe = function(subject, topic, data) {
      resolve(Date.now());
    };

    Services.obs.addObserver(observer, waitTopic);
  });

  Services.obs.removeObserver(observer, waitTopic);

  // This delay attempts to make the time stamps unique.
  do {
    var now = Date.now();
  } while (when + 5 > now);

  return when;
}

async function waitForGCEnd() {
  var waitTopic = "garbage-collector-end";
  var observer = {};

  info("Waiting for " + waitTopic);
  // This fixes a ReferenceError for Date, it's weird.
  ok(Date.now(), "Date.now()");
  let when = await new Promise(resolve => {
    observer.observe = function(subject, topic, data) {
      resolve(Date.now());
    };

    Services.obs.addObserver(observer, waitTopic);
  });

  Services.obs.removeObserver(observer, waitTopic);

  do {
    var now = Date.now();
  } while (when + 5 > now);

  return when;
}

function getProcessID() {
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );

  return Services.appinfo.processID;
}

async function resolveInOrder(promisesAndStates) {
  var order = [];
  var promises = [];

  for (let p of promisesAndStates) {
    promises.push(
      p.promise.then(when => {
        info(`Tab: ${p.tab} did ${p.state}`);
        order.push({ tab: p.tab, state: p.state, when });
      })
    );
  }

  await Promise.all(promises);

  return order;
}

// Check that the list of events returned by resolveInOrder are in a
// sensible order.
function checkOneAtATime(events, expectTabsCompleted) {
  var cur = null;
  var tabsCompleted = [];
  var lastWhen = null;

  info("Checking order of events");
  for (const e of events) {
    ok(e.state === "begin" || e.state === "end", "event.state is good");
    ok(e.tab !== undefined, "event.tab exists");

    if (lastWhen) {
      // We need these in sorted order so that the other checks here make
      // sense.
      ok(
        lastWhen <= e.when,
        `Unsorted events, last: ${lastWhen}, this: ${e.when}`
      );
    }
    lastWhen = e.when;

    if (e.state === "begin") {
      is(cur, null, `GC can begin on tab ${e.tab}`);
      cur = e.tab;
    } else {
      is(e.tab, cur, `GC can end on tab ${e.tab}`);
      tabsCompleted.push(e.tab);
      cur = null;
    }
  }

  is(cur, null, "No GC left running");
  for (var t of expectTabsCompleted) {
    ok(tabsCompleted.includes(t), `Tab ${t} did a GC`);
  }
}

add_task(async function gcOneAtATime() {
  SpecialPowers.pushPrefEnv({
    set: [["javascript.options.concurrent_multiprocess_gcs.max", 1]],
  });

  const num_tabs = 12;
  var tabs = [];
  var pids = [];

  const parent_pid = getProcessID();
  info("Parent process PID is " + parent_pid);

  for (var i = 0; i < num_tabs; i++) {
    var newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
    const tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: TEST_PAGE,
      forceNewProcess: true,
    });
    // Make sure the tab is ready
    await newTabPromise;

    tabs[i] = tab;

    const tab_pid = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      getProcessID
    );

    info(`Tab ${i} pid is ${tab_pid}`);
    isnot(parent_pid, tab_pid, `Tab ${i} is in content process`);
    ok(!pids.includes(tab_pid), `Tab ${i} is in a distinct process`);

    pids.push(tab_pid);
  }

  info("Tabs ready, Asking for GCs");
  var waits = [];
  for (i = 0; i < num_tabs; i++) {
    var browser = tabs[i].linkedBrowser;

    // Finish any currently running GC.
    SpecialPowers.spawn(browser, [], () => {
      SpecialPowers.Cu.getJSTestingFunctions().finishgc();
    });

    var waitBegin = SpecialPowers.spawn(browser, [], waitForGCBegin);
    var waitEnd = SpecialPowers.spawn(browser, [], waitForGCEnd);
    waits.push({ promise: waitBegin, tab: i, state: "begin" });
    waits.push({ promise: waitEnd, tab: i, state: "end" });

    SpecialPowers.spawn(browser, [], () => {
      content.windowUtils.pokeGC();
      content.windowUtils.runNextCollectorTimer();
    });
  }

  let order = await resolveInOrder(waits);
  // We need these in the order they actually occurred, so far that's how
  // they're returned, but we'll sort them to be sure.
  order.sort((e1, e2) => e1.when - e2.when);
  checkOneAtATime(
    order,
    Array.from({ length: num_tabs }, (_, n) => n)
  );

  for (var tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  SpecialPowers.popPrefEnv();
});
