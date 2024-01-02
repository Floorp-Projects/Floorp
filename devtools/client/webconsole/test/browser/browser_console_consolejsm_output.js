/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that Console.sys.mjs outputs messages to the Browser Console.

"use strict";

add_task(async function testCategoryLogs() {
  const consoleStorage = Cc["@mozilla.org/consoleAPI-storage;1"];
  const storage = consoleStorage.getService(Ci.nsIConsoleAPIStorage);
  storage.clearEvents();

  console.log("bug861338-log-cached");

  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  await checkMessageExists(hud, "bug861338-log-cached");

  await clearOutput(hud);

  function testTrace() {
    console.trace();
  }

  console.time("foobarTimer");
  const foobar = { bug851231prop: "bug851231value" };

  console.log("bug851231-log");
  console.info("bug851231-info");
  console.warn("bug851231-warn");
  console.error("bug851231-error", foobar);
  console.debug("bug851231-debug");
  console.dir({ "bug851231-dir": 1 });
  testTrace();
  console.timeEnd("foobarTimer");

  info("wait for the Console.sys.mjs messages");

  await checkMessageExists(hud, "bug851231-log");
  await checkMessageExists(hud, "bug851231-info");
  await checkMessageExists(hud, "bug851231-warn");
  await checkMessageExists(hud, "bug851231-error");
  await checkMessageExists(hud, "bug851231-debug");
  await checkMessageExists(hud, "bug851231-dir");
  await checkMessageExists(hud, "console.trace()");
  await checkMessageExists(hud, "foobarTimer");

  await clearOutput(hud);
  await BrowserConsoleManager.toggleBrowserConsole();
});

add_task(async function testFilter() {
  const consoleStorage = Cc["@mozilla.org/consoleAPI-storage;1"];
  const storage = consoleStorage.getService(Ci.nsIConsoleAPIStorage);
  storage.clearEvents();

  const { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  const console2 = new ConsoleAPI();
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  // Enable the error category and disable the log category.
  await setFilterState(hud, {
    error: true,
    log: false,
  });

  const shouldBeVisible = "Should be visible";
  const shouldBeHidden = "Should be hidden";

  console2.log(shouldBeHidden);
  console2.error(shouldBeVisible);

  await checkMessageExists(hud, shouldBeVisible);
  // Here we can safely assert that the log message is not visible, since the
  // error message was logged after and is visible.
  await checkMessageHidden(hud, shouldBeHidden);

  await resetFilters(hud);
  await clearOutput(hud);
});

// Test that console.profile / profileEnd trigger the right events
add_task(async function testProfile() {
  const consoleStorage = Cc["@mozilla.org/consoleAPI-storage;1"];
  const storage = consoleStorage.getService(Ci.nsIConsoleAPIStorage);
  const { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  const console = new ConsoleAPI();

  storage.clearEvents();

  const profilerEvents = [];

  function observer(subject, topic) {
    is(topic, "console-api-profiler", "The topic is 'console-api-profiler'");
    const subjectObj = subject.wrappedJSObject;
    const event = { action: subjectObj.action, name: subjectObj.arguments[0] };
    info(`Profiler event: action=${event.action}, name=${event.name}`);
    profilerEvents.push(event);
  }

  Services.obs.addObserver(observer, "console-api-profiler");

  console.profile("test");
  console.profileEnd("test");

  Services.obs.removeObserver(observer, "console-api-profiler");

  // Test that no messages were logged to the storage
  const consoleEvents = storage.getEvents();
  is(consoleEvents.length, 0, "There are zero logged messages");

  // Test that two profiler events were fired
  is(profilerEvents.length, 2, "Got two profiler events");
  is(profilerEvents[0].action, "profile", "First event has the right action");
  is(profilerEvents[0].name, "test", "First event has the right name");
  is(
    profilerEvents[1].action,
    "profileEnd",
    "Second event has the right action"
  );
  is(profilerEvents[1].name, "test", "Second event has the right name");
});

async function checkMessageExists(hud, msg) {
  info(`Checking "${msg}" was logged`);
  const message = await waitFor(() => findConsoleAPIMessage(hud, msg));
  ok(message, `"${msg}" was logged`);
}

async function checkMessageHidden(hud, msg) {
  info(`Checking "${msg}" was not logged`);
  await waitFor(() => findConsoleAPIMessage(hud, msg) == null);
  ok(true, `"${msg}" was not logged`);
}
