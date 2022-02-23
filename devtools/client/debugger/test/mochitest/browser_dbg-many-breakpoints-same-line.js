/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test settings multiple types of breakpoints on the same line
// Only the last should be used

// Line where we set a breakpoint in simple2.js
const BREAKPOINT_LINE = 5;

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2");

  await selectSource(dbg, "simple2");
  await waitForSelectedSource(dbg, "simple2");

  await testSimpleAndLog(dbg);

  await testLogUpdates(dbg);
})

async function testSimpleAndLog(dbg) {
  info("Add a simple breakpoint");
  await addBreakpoint(dbg, "simple2", BREAKPOINT_LINE);

  info("Add a log breakpoint, replacing the breakpoint into a logpoint");
  await setLogPoint(dbg, BREAKPOINT_LINE, "`log point ${x}`");
  await waitForLog(dbg, "`log point ${x}`");
  await assertLogBreakpoint(dbg, BREAKPOINT_LINE);

  const bp = findBreakpoint(dbg, "simple2", BREAKPOINT_LINE);
  is(bp.options.logValue, "`log point ${x}`", "log breakpoint value is correct");

  info("Eval foo() and trigger the breakpoints. If this freeze here, it means that the log point has been ignored.");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.foo(42);
  });

  info("Wait for the log-point message. Only log-point breakpoint should work.");
  await waitForMessage(dbg, "log point 42");

  const source = findSource(dbg, "simple2.js");
  await removeBreakpoint(dbg, source.id, BREAKPOINT_LINE);
}

async function testLogUpdates(dbg) {
  info("Add a log breakpoint");
  await setLogPoint(dbg, BREAKPOINT_LINE, "`log point`");
  await waitForLog(dbg, "`log point`");
  await assertLogBreakpoint(dbg, BREAKPOINT_LINE);

  const bp = findBreakpoint(dbg, "simple2", BREAKPOINT_LINE);
  is(bp.options.logValue, "`log point`", "log breakpoint value is correct");

  info("Edit the log breakpoint");
  await setLogPoint(dbg, BREAKPOINT_LINE, " + ` edited`");
  await waitForLog(dbg, "`log point` + ` edited`");
  await assertLogBreakpoint(dbg, BREAKPOINT_LINE);

  const bp2 = findBreakpoint(dbg, "simple2", BREAKPOINT_LINE);
  is(bp2.options.logValue, "`log point` + ` edited`", "log breakpoint value is correct");

  info("Eval foo() and trigger the breakpoints");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.foo();
  });

  info("Wait for the log-point message. Only the edited one should appear");
  await waitForMessage(dbg, "log point edited");
}

function findMessages(win, query) {
  return Array.prototype.filter.call(
    win.document.querySelectorAll(".message"),
    e => e.innerText.includes(query)
  );
}

async function waitForMessage(dbg, msg) {
  const webConsolePanel = await getDebuggerSplitConsole(dbg);
  return waitFor(
    async () => findMessages(webConsolePanel._frameWindow, msg).length > 0
  );
}
