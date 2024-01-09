/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the Javascript Tracing feature.

"use strict";

add_task(async function () {
  // This is preffed off for now, so ensure turning it on
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const dbg = await initDebugger("doc-scripts.html");

  info("Enable the tracing");
  await clickElement(dbg, "trace");

  const topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, state => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  ok(
    dbg.toolbox.splitConsole,
    "Split console is automatically opened when tracing to the console"
  );

  await hasConsoleMessage(dbg, "Started tracing to Web Console");

  invokeInTab("main");

  info("Wait for console messages for the whole trace");
  // `main` calls `foo` which calls `bar`
  await hasConsoleMessage(dbg, "λ main");
  await hasConsoleMessage(dbg, "λ foo");
  await hasConsoleMessage(dbg, "λ bar");

  const traceMessages = await findConsoleMessages(dbg.toolbox, "λ main");
  is(traceMessages.length, 1, "We got a unique trace for 'main' function call");
  const sourceLink = traceMessages[0].querySelector(".frame-link-source");
  sourceLink.click();
  info("Wait for the main function to be highlighted in the debugger");
  await waitForSelectedSource(dbg, "simple1.js");
  await waitForSelectedLocation(dbg, 1, 16);

  // Trigger a click to verify we do trace DOM events
  BrowserTestUtils.synthesizeMouseAtCenter(
    "button",
    {},
    gBrowser.selectedBrowser
  );

  await hasConsoleMessage(dbg, "DOM(click)");
  await hasConsoleMessage(dbg, "λ simple");

  // Test Blackboxing
  info("Clear the console from previous traces");
  const { hud } = await dbg.toolbox.getPanel("webconsole");
  hud.ui.clearOutput();
  await waitFor(
    async () => !(await findConsoleMessages(dbg.toolbox, "λ main")).length,
    "Wait for console to be cleared"
  );

  info(
    "Now blackbox only the source where main function is (simple1.js), but foo and bar are in another module"
  );
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX_WHOLE_SOURCES");

  info("Trigger some code from simple1 and simple2");
  invokeInTab("main");

  info("Only methods from simple2 are logged");
  await hasConsoleMessage(dbg, "λ foo");
  await hasConsoleMessage(dbg, "λ bar");
  is(
    (await findConsoleMessages(dbg.toolbox, "λ main")).length,
    0,
    "Traces from simple1.js, related to main function are not logged"
  );

  info("Revert blackboxing");
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "UNBLACKBOX_WHOLE_SOURCES");

  // Test Disabling tracing
  info("Disable the tracing");
  await clickElement(dbg, "trace");
  info("Wait for tracing to be disabled");
  await waitForState(dbg, state => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });
  await hasConsoleMessage(dbg, "Stopped tracing");

  invokeInTab("inline_script2");

  // Let some time for the tracer to appear if we failed disabling the tracing
  await wait(1000);

  const messages = await findConsoleMessages(dbg.toolbox, "inline_script2");
  is(
    messages.length,
    0,
    "We stopped recording traces, an the function call isn't logged in the console"
  );

  // Test Navigations
  await navigate(dbg, "doc-sourcemaps2.html", "main.js", "main.min.js");

  info("Re-enable the tracing after navigation");
  await clickElement(dbg, "trace");

  const newTopLevelThread =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be re-enabled");
  await waitForState(dbg, state => {
    return dbg.selectors.getIsThreadCurrentlyTracing(newTopLevelThread);
  });

  invokeInTab("logMessage");

  await hasConsoleMessage(dbg, "λ logMessage");

  // Test clicking on the function to open the precise related location
  const traceMessages2 = await findConsoleMessages(dbg.toolbox, "λ logMessage");
  is(
    traceMessages2.length,
    1,
    "We got a unique trace for 'logMessage' function call"
  );
  const sourceLink2 = traceMessages2[0].querySelector(".frame-link-source");
  sourceLink2.click();

  info("Wait for the 'logMessage' function to be highlighted in the debugger");
  await waitForSelectedSource(dbg, "main.js");
  await waitForSelectedLocation(dbg, 4, 2);
  ok(true, "The selected source and location is on the original file");

  await dbg.toolbox.closeToolbox();
});

add_task(async function testTracingWorker() {
  // We have to enable worker targets and disable this pref to have functional tracing for workers
  await pushPref("dom.worker.console.dispatch_events_to_main_thread", false);

  const dbg = await initDebugger("doc-scripts.html");

  info("Instantiate a worker");
  const { targetCommand } = dbg.toolbox.commands;
  let onAvailable;
  const onNewTarget = new Promise(resolve => {
    onAvailable = ({ targetFront }) => {
      resolve(targetFront);
    };
  });
  await targetCommand.watchTargets({
    types: [targetCommand.TYPES.FRAME],
    onAvailable,
  });
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.worker = new content.Worker("simple-worker.js");
  });
  info("Wait for the worker target");
  const workerTarget = await onNewTarget;

  await waitFor(
    () => findAllElements(dbg, "threadsPaneItems").length == 2,
    "Wait for the two threads to be displayed in the thread pane"
  );
  const threadsEl = findAllElements(dbg, "threadsPaneItems");
  is(threadsEl.length, 2, "There are two threads in the thread panel");

  info("Enable tracing on all threads");
  await clickElement(dbg, "trace");
  info("Wait for tracing to be enabled for the worker");
  await waitForState(dbg, state => {
    return dbg.selectors.getIsThreadCurrentlyTracing(
      workerTarget.threadFront.actorID
    );
  });

  // `timer` is called within the worker via a setInterval of 1 second
  await hasConsoleMessage(dbg, "setIntervalCallback");
  await hasConsoleMessage(dbg, "λ timer");

  // Also verify that postMessage are traced
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.worker.postMessage("foo");
  });

  await hasConsoleMessage(dbg, "DOM(message)");
  await hasConsoleMessage(dbg, "λ onmessage");

  await dbg.toolbox.closeToolbox();
});

add_task(async function testPersitentLogMethod() {
  let dbg = await initDebugger("doc-scripts.html");
  is(
    dbg.selectors.getJavascriptTracingLogMethod(),
    "console",
    "By default traces are logged to the console"
  );

  info("Change the log method to stdout");
  dbg.actions.setJavascriptTracingLogMethod("stdout");

  await dbg.toolbox.closeToolbox();

  dbg = await initDebugger("doc-scripts.html");
  is(
    dbg.selectors.getJavascriptTracingLogMethod(),
    "stdout",
    "The new setting has been persisted"
  );

  info("Reset back to the default value");
  dbg.actions.setJavascriptTracingLogMethod("console");
});

add_task(async function testPageKeyShortcut() {
  // Ensures that the key shortcut emitted in the content process bubbles up to the parent process
  await pushPref("test.events.async.enabled", true);

  // Fake DevTools being opened by a real user interaction.
  // Tests are bypassing DevToolsStartup to open the tools by calling gDevTools directly.
  // By doing so DevToolsStartup considers itself as uninitialized,
  // whereas we want it to handle the key shortcut we trigger in this test.
  const DevToolsStartup = Cc["@mozilla.org/devtools/startup-clh;1"].getService(
    Ci.nsISupports
  ).wrappedJSObject;
  DevToolsStartup.initialized = true;
  registerCleanupFunction(() => {
    DevToolsStartup.initialized = false;
  });

  const dbg = await initDebuggerWithAbsoluteURL("data:text/html,key-shortcut");

  const topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  ok(
    !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID),
    "Tracing is disabled on debugger opening"
  );

  info(
    "Focus the page in order to assert that the page keeps the focus when enabling the tracer"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.focus();
  });
  await waitFor(
    () => Services.focus.focusedElement == gBrowser.selectedBrowser
  );
  is(
    Services.focus.focusedElement,
    gBrowser.selectedBrowser,
    "The tab is still focused before enabling tracing"
  );

  info("Toggle ON the tracing via the key shortcut from the web page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    EventUtils.synthesizeKey(
      "VK_5",
      { ctrlKey: true, shiftKey: true },
      content
    );
  });

  info("Wait for tracing to be enabled");
  await waitForState(dbg, state => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  is(
    Services.focus.focusedElement,
    gBrowser.selectedBrowser,
    "The tab is still focused after enabling tracing"
  );

  info("Toggle it back off, with the same shortcut");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    EventUtils.synthesizeKey(
      "VK_5",
      { ctrlKey: true, shiftKey: true },
      content
    );
  });

  info("Wait for tracing to be disabled");
  await waitForState(dbg, state => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });
});

add_task(async function testPageKeyShortcutWithoutDebugger() {
  // Ensures that the key shortcut emitted in the content process bubbles up to the parent process
  await pushPref("test.events.async.enabled", true);

  // Fake DevTools being opened by a real user interaction.
  // Tests are bypassing DevToolsStartup to open the tools by calling gDevTools directly.
  // By doing so DevToolsStartup considers itself as uninitialized,
  // whereas we want it to handle the key shortcut we trigger in this test.
  const DevToolsStartup = Cc["@mozilla.org/devtools/startup-clh;1"].getService(
    Ci.nsISupports
  ).wrappedJSObject;
  DevToolsStartup.initialized = true;
  registerCleanupFunction(() => {
    DevToolsStartup.initialized = false;
  });

  const toolbox = await openNewTabAndToolbox(
    "data:text/html,tracer",
    "webconsole"
  );

  info(
    "Focus the page in order to assert that the page keeps the focus when enabling the tracer"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.focus();
  });
  await waitFor(
    () => Services.focus.focusedElement == gBrowser.selectedBrowser
  );
  is(
    Services.focus.focusedElement,
    gBrowser.selectedBrowser,
    "The tab is still focused before enabling tracing"
  );

  info("Toggle ON the tracing via the key shortcut from the web page");
  const { resourceCommand } = toolbox.commands;
  const { onResource: onTracingStateEnabled } =
    await resourceCommand.waitForNextResource(
      resourceCommand.TYPES.JSTRACER_STATE,
      {
        ignoreExistingResources: true,
        predicate(resource) {
          return resource.enabled;
        },
      }
    );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    EventUtils.synthesizeKey(
      "VK_5",
      { ctrlKey: true, shiftKey: true },
      content
    );
  });
  info("Wait for tracing to be enabled");
  await onTracingStateEnabled;

  info("Toggle it back off, with the same shortcut");
  const { onResource: onTracingStateDisabled } =
    await resourceCommand.waitForNextResource(
      resourceCommand.TYPES.JSTRACER_STATE,
      {
        ignoreExistingResources: true,
        predicate(resource) {
          return !resource.enabled;
        },
      }
    );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    EventUtils.synthesizeKey(
      "VK_5",
      { ctrlKey: true, shiftKey: true },
      content
    );
  });

  info("Wait for tracing to be disabled");
  await onTracingStateDisabled;
});

add_task(async function testTracingValues() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);
  // Cover tracing function argument values
  const jsCode = `function foo() { bar(1, ["array"], { attribute: 3 }, BigInt(4), Infinity, Symbol("6"), "7"); }; function bar(a, b, c) {}`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," + encodeURIComponent(`<script>${jsCode}</script>`)
  );

  await openContextMenuInDebugger(dbg, "trace");
  const toggled = waitForDispatch(
    dbg.store,
    "TOGGLE_JAVASCRIPT_TRACING_VALUES"
  );
  selectContextMenuItem(dbg, `#debugger-trace-menu-item-log-values`);
  await toggled;
  ok(true, "Toggled the log values setting");

  await clickElement(dbg, "trace");

  const topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, state => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  invokeInTab("foo");

  await hasConsoleMessage(dbg, "λ foo()");
  await hasConsoleMessage(dbg, "λ bar");
  const { value } = await findConsoleMessage(dbg, "λ bar");
  is(
    value,
    `interpreter⟶λ bar(1, \nArray [ "array" ]\n, \nObject { attribute: 3 }\n, 4n, Infinity, Symbol("6"), "7")`,
    "The argument were printed for bar()"
  );

  // Reset the log values setting
  Services.prefs.clearUserPref("devtools.debugger.javascript-tracing-values");
});

add_task(async function testTracingOnNextInteraction() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);
  // Cover tracing only on next user interaction
  const jsCode = `function foo() {}; window.addEventListener("mousedown", function onmousedown(){}, { capture: true }); window.onclick = function onclick() {};`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," +
      encodeURIComponent(`<script>${jsCode}</script><body></body>`)
  );

  await openContextMenuInDebugger(dbg, "trace");
  const toggled = waitForDispatch(
    dbg.store,
    "TOGGLE_JAVASCRIPT_TRACING_ON_NEXT_INTERACTION"
  );
  selectContextMenuItem(dbg, `#debugger-trace-menu-item-next-interaction`);
  await toggled;
  ok(true, "Toggled the trace on next interaction");

  await clickElement(dbg, "trace");

  const topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, state => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  invokeInTab("foo");

  // Let a change to have the tracer to regress and log foo call
  await wait(500);

  is(
    (await findConsoleMessages(dbg.toolbox, "λ foo")).length,
    0,
    "The tracer did not log the function call before trigerring the click event"
  );

  // We intentionally turn off this a11y check, because the following click
  // is send on an empty <body> to to test the click event tracer performance,
  // and not to activate any control, therefore this check can be ignored.
  AccessibilityUtils.setEnv({
    mustHaveAccessibleRule: false,
  });
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    {},
    gBrowser.selectedBrowser
  );
  AccessibilityUtils.resetEnv();

  await hasConsoleMessage(dbg, "λ onmousedown");
  await hasConsoleMessage(dbg, "λ onclick");

  is(
    (await findConsoleMessages(dbg.toolbox, "λ foo")).length,
    0,
    "Even after the click, the code called before the click is still not logged"
  );

  // But if we call this code again, now it should be logged
  invokeInTab("foo");
  await hasConsoleMessage(dbg, "λ foo");
  ok(true, "foo was traced as expected");

  // Reset the trace on next interaction setting
  Services.prefs.clearUserPref(
    "devtools.debugger.javascript-tracing-on-next-interaction"
  );
});
