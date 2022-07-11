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
function checkOneAtATime(events) {
  var cur = null;
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
      cur = null;
    }
  }

  is(cur, null, "No GC left running");
}

function checkAllCompleted(events, expectTabsCompleted) {
  var tabsCompleted = events.filter(e => e.state === "end").map(e => e.tab);

  for (var t of expectTabsCompleted) {
    ok(tabsCompleted.includes(t), `Tab ${t} did a GC`);
  }
}

async function setupTabs(num_tabs) {
  var pids = [];

  const parent_pid = getProcessID();
  info("Parent process PID is " + parent_pid);

  const tabs = await Promise.all(
    Array(num_tabs)
      .fill()
      .map(_ => {
        return BrowserTestUtils.openNewForegroundTab({
          gBrowser,
          opening: TEST_PAGE,
          forceNewProcess: true,
        });
      })
  );

  for (const [i, tab] of Object.entries(tabs)) {
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

  return tabs;
}

function doContentRunNextCollectionTimer() {
  content.windowUtils.pokeGC("PAGE_HIDE");
  content.windowUtils.runNextCollectorTimer("PAGE_HIDE");
}

function startNextCollection(
  tab,
  tab_num,
  waits,
  fn = doContentRunNextCollectionTimer
) {
  var browser = tab.linkedBrowser;

  // Finish any currently running GC.
  SpecialPowers.spawn(browser, [], () => {
    SpecialPowers.Cu.getJSTestingFunctions().finishgc();
  });

  var waitBegin = SpecialPowers.spawn(browser, [], waitForGCBegin);
  var waitEnd = SpecialPowers.spawn(browser, [], waitForGCEnd);
  waits.push({ promise: waitBegin, tab: tab_num, state: "begin" });
  waits.push({ promise: waitEnd, tab: tab_num, state: "end" });

  SpecialPowers.spawn(browser, [], fn);

  // Return these so that the abort GC test can wait for the begin.
  return { waitBegin, waitEnd };
}

add_task(async function gcOneAtATime() {
  SpecialPowers.pushPrefEnv({
    set: [["javascript.options.concurrent_multiprocess_gcs.max", 1]],
  });

  const num_tabs = 12;
  var tabs = await setupTabs(num_tabs);

  info("Tabs ready, Asking for GCs");
  var waits = [];
  for (var i = 0; i < num_tabs; i++) {
    startNextCollection(tabs[i], i, waits);
  }

  let order = await resolveInOrder(waits);
  // We need these in the order they actually occurred, so far that's how
  // they're returned, but we'll sort them to be sure.
  order.sort((e1, e2) => e1.when - e2.when);
  checkOneAtATime(order);
  checkAllCompleted(
    order,
    Array.from({ length: num_tabs }, (_, n) => n)
  );

  for (var tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  SpecialPowers.popPrefEnv();
});

add_task(async function gcAbort() {
  SpecialPowers.pushPrefEnv({
    set: [["javascript.options.concurrent_multiprocess_gcs.max", 1]],
  });

  const num_tabs = 2;
  var tabs = await setupTabs(num_tabs);

  info("Tabs ready, Asking for GCs");
  var waits = [];

  var tab0Waits = startNextCollection(tabs[0], 0, waits, () => {
    SpecialPowers.Cu.getJSTestingFunctions().gcslice(1);
  });
  await tab0Waits.waitBegin;

  // Tab 0 has started a GC. Now we schedule a GC in tab one.  It must not
  // begin yet (but we don't check that, gcOneAtATime is assumed to check
  // this.
  startNextCollection(tabs[1], 1, waits);

  // Request that tab 0 abort, this test checks that tab 1 can now begin.
  SpecialPowers.spawn(tabs[0].linkedBrowser, [], () => {
    SpecialPowers.Cu.getJSTestingFunctions().abortgc();
  });

  let order = await resolveInOrder(waits);
  // We need these in the order they actually occurred, so far that's how
  // they're returned, but we'll sort them to be sure.
  order.sort((e1, e2) => e1.when - e2.when);
  checkOneAtATime(order);
  checkAllCompleted(
    order,
    Array.from({ length: num_tabs }, (_, n) => n)
  );

  for (var tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  SpecialPowers.popPrefEnv();
});

add_task(async function gcJSInitiatedDuring() {
  SpecialPowers.pushPrefEnv({
    set: [["javascript.options.concurrent_multiprocess_gcs.max", 1]],
  });

  const num_tabs = 3;
  var tabs = await setupTabs(num_tabs);

  info("Tabs ready, Asking for GCs");
  var waits = [];

  // Start a GC on tab 0 to consume the scheduler's "token".  Zeal mode 10
  // will cause it to run in many slices.
  var tab0Waits = startNextCollection(tabs[0], 0, waits, () => {
    if (SpecialPowers.Cu.getJSTestingFunctions().gczeal) {
      SpecialPowers.Cu.getJSTestingFunctions().gczeal(10);
    }
    SpecialPowers.Cu.getJSTestingFunctions().gcslice(1);
  });
  await tab0Waits.waitBegin;
  info("GC on tab 0 has begun");

  // Request a GC in tab 1, this will be blocked by the ongoing GC in tab 0.
  var tab1Waits = startNextCollection(tabs[1], 1, waits);

  // Force a GC to start in tab 1.  This won't wait for tab 0.
  SpecialPowers.spawn(tabs[1].linkedBrowser, [], () => {
    SpecialPowers.Cu.getJSTestingFunctions().gcslice(1);
  });

  await tab1Waits.waitBegin;
  info("GC on tab 1 has begun");

  // The GC in tab 0 should still be running.
  var state = await SpecialPowers.spawn(tabs[0].linkedBrowser, [], () => {
    return SpecialPowers.Cu.getJSTestingFunctions().gcstate();
  });
  info("State of Tab 0 GC is " + state);
  isnot(state, "NotActive", "GC is active in tab 0");

  // Let the GCs complete, verify that a GC in a 3rd tab can acquire a token.
  startNextCollection(tabs[2], 2, waits);

  let order = await resolveInOrder(waits);
  info("All GCs finished");
  checkAllCompleted(
    order,
    Array.from({ length: num_tabs }, (_, n) => n)
  );

  for (var tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  SpecialPowers.popPrefEnv();
});

add_task(async function gcJSInitiatedBefore() {
  SpecialPowers.pushPrefEnv({
    set: [["javascript.options.concurrent_multiprocess_gcs.max", 1]],
  });

  const num_tabs = 8;
  var tabs = await setupTabs(num_tabs);

  info("Tabs ready");
  var waits = [];

  // Start a GC on tab 0 to consume the scheduler's first "token".  Zeal mode 10
  // will cause it to run in many slices.
  info("Force a JS-initiated GC in tab 0");
  var tab0Waits = startNextCollection(tabs[0], 0, waits, () => {
    if (SpecialPowers.Cu.getJSTestingFunctions().gczeal) {
      SpecialPowers.Cu.getJSTestingFunctions().gczeal(10);
    }
    SpecialPowers.Cu.getJSTestingFunctions().gcslice(1);
  });
  await tab0Waits.waitBegin;

  info("Request GCs in remaining tabs");
  for (var i = 1; i < num_tabs; i++) {
    startNextCollection(tabs[i], i, waits);
  }

  // The GC in tab 0 should still be running.
  var state = await SpecialPowers.spawn(tabs[0].linkedBrowser, [], () => {
    return SpecialPowers.Cu.getJSTestingFunctions().gcstate();
  });
  info("State is " + state);
  isnot(state, "NotActive", "GC is active in tab 0");

  let order = await resolveInOrder(waits);
  // We need these in the order they actually occurred, so far that's how
  // they're returned, but we'll sort them to be sure.
  order.sort((e1, e2) => e1.when - e2.when);
  checkOneAtATime(order);
  checkAllCompleted(
    order,
    Array.from({ length: num_tabs }, (_, n) => n)
  );

  for (var tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  SpecialPowers.popPrefEnv();
});
