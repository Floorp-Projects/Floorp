/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for importing HAR data.
 */
add_task(async () => {
  let { tab, monitor } = await initNetMonitor(
    HAR_EXAMPLE_URL + "html_har_import-test-page.html");

  info("Starting test... ");

  let { actions, connector, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { HarMenuUtils } = windowRequire(
    "devtools/client/netmonitor/src/har/har-menu-utils");
  let { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index");
  let { HarImporter } = windowRequire(
    "devtools/client/netmonitor/src/har/har-importer");

  store.dispatch(Actions.batchEnable(false));

  // Execute one POST request on the page and wait till its done.
  let wait = waitForNetworkEvents(monitor, 3);
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.executeTest();
  });
  await wait;

  // Copy HAR into the clipboard
  let json1 = await HarMenuUtils.copyAllAsHar(
    getSortedRequests(store.getState()), connector);

  // Import HAR string
  let importer = new HarImporter(actions);
  importer.import(json1);

  // Copy HAR into the clipboard again
  let json2 = await HarMenuUtils.copyAllAsHar(
    getSortedRequests(store.getState()), connector);

  // Compare exported HAR data
  let har1 = JSON.parse(json1);
  let har2 = JSON.parse(json2);

  dump("---------------\n");
  dump(json1 + "\n");
  dump("---------------\n");
  dump("---------------\n");
  dump(json2 + "\n");
  dump("---------------\n");

  // Explicit tests
  is(har2.log.entries.length, 3,
    "There must be expected number of requests");
  ok(har2.log.entries[0].request.headers.length > 0,
    "There must be some request headers");
  ok(har2.log.entries[0].response.headers.length > 0,
    "There must be some response headers");
  is(har2.log.entries[1].response.cookies.length, 3,
    "There must be expected number of cookies");
  is(har2.log.entries[1]._securityState, "insecure",
    "There must be expected security state");
  is(har2.log.entries[2].response.status, 304,
    "There must be expected status");

  // Complex test comparing exported & imported HARs.
  ok(compare(har1.log, har2.log, ["log"]), "Exported HAR must be the same");

  // Clean up
  return teardown(monitor);
});

/**
 * Check equality of HAR files.
 */
function compare(obj1, obj2, path) {
  let keys1 = Object.getOwnPropertyNames(obj1).sort();
  let keys2 = Object.getOwnPropertyNames(obj2).sort();

  let name = path.join("/");

  is(keys1.length, keys2.length, "There must be the same number of keys for: " + name);
  if (keys1.length != keys2.length) {
    return false;
  }

  is(keys1.join(), keys2.join(), "There must be the same keys for: " + name);
  if (keys1.join() != keys2.join()) {
    return false;
  }

  // Page IDs are generated and don't have to be the same after import.
  let ignore = [
    "log/entries/0/pageref",
    "log/entries/1/pageref",
    "log/entries/2/pageref",
    "log/pages/0/id",
    "log/pages/1/id",
    "log/pages/2/id",
  ];

  let result = true;
  for (let i = 0; i < keys1.length; i++) {
    let key = keys1[i];
    let prop1 = obj1[key];
    let prop2 = obj2[key];

    if (prop1 instanceof Array) {
      if (!(prop2 instanceof Array)) {
        ok(false, "Arrays are not the same " + name);
        result = false;
        break;
      }
      if (!compare(prop1, prop2, path.concat(key))) {
        result = false;
        break;
      }
    } else if (prop1 instanceof Object) {
      if (!(prop2 instanceof Object)) {
        ok(false, "Objects are not the same in: " + name);
        result = false;
        break;
      }
      if (!compare(prop1, prop2, path.concat(key))) {
        result = false;
        break;
      }
    } else if (prop1 !== prop2) {
      let propName = name + "/" + key;
      if (!ignore.includes(propName)) {
        is(prop1, prop2, "Values are not the same: " + propName);
        result = false;
        break;
      }
    }
  }

  return result;
}
