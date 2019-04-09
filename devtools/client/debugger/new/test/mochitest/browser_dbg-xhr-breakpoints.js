/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

async function addXHRBreakpoint(dbg, text, method) {
  info(`Adding a XHR breakpoint for pattern ${text} and method ${method}`);

  const plusIcon = findElementWithSelector(dbg, ".xhr-breakpoints-pane .plus");
  if (plusIcon) {
    plusIcon.click();
  }
  findElementWithSelector(dbg, ".xhr-input-url").focus();
  type(dbg, text);

  if (method) {
    findElementWithSelector(dbg, ".xhr-input-method").value = method;
  }

  pressKey(dbg, "Enter");

  await waitForDispatch(dbg, "SET_XHR_BREAKPOINT");
}

async function removeXHRBreakpoint(dbg, index) {
  info("Removing a XHR breakpoint");

  const closeButtons = dbg.win.document.querySelectorAll(
    ".xhr-breakpoints-pane .close-btn"
  );
  if (closeButtons[index]) {
    closeButtons[index].click();
  }

  await waitForDispatch(dbg, "REMOVE_XHR_BREAKPOINT");
}

function getXHRBreakpointsElements(dbg) {
  return [
    ...dbg.win.document.querySelectorAll(".xhr-breakpoints-pane .xhr-container")
  ];
}

function getXHRBreakpointLabels(elements) {
  return elements.map(element => element.title);
}

function getXHRBreakpointCheckbox(dbg) {
  return findElementWithSelector(
    dbg,
    ".xhr-breakpoints-pane .breakpoints-exceptions input"
  );
}

async function clickPauseOnAny(dbg, expectedEvent) {
  getXHRBreakpointCheckbox(dbg).click();
  await waitForDispatch(dbg, expectedEvent);
}

add_task(async function() {
  const dbg = await initDebugger("doc-xhr.html", "fetch.js");

  await addXHRBreakpoint(dbg, "doc", "GET");

  invokeInTab("main", "doc-xhr.html");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  await resume(dbg);

  await dbg.actions.removeXHRBreakpoint(0);
  await invokeInTab("main", "doc-xhr.html");
  assertNotPaused(dbg);

  await addXHRBreakpoint(dbg, "doc", "POST");
  await invokeInTab("main", "doc-xhr.html");
  assertNotPaused(dbg);
});

// Tests the "pause on any URL" checkbox works properly
add_task(async function() {
  const dbg = await initDebugger("doc-xhr.html", "fetch.js");

  // Enable pause on any URL
  await clickPauseOnAny(dbg, "SET_XHR_BREAKPOINT");

  invokeInTab("main", "doc-xhr.html");
  await waitForPaused(dbg);
  await resume(dbg);

  invokeInTab("main", "fetch.js");
  await waitForPaused(dbg);
  await resume(dbg);

  invokeInTab("main", "README.md");
  await waitForPaused(dbg);
  await resume(dbg);

  // Disable pause on any URL
  await clickPauseOnAny(dbg, "DISABLE_XHR_BREAKPOINT");
  invokeInTab("main", "README.md");
  assertNotPaused(dbg);

  // Turn off the checkbox
  await dbg.actions.removeXHRBreakpoint(0);
});

// Tests removal works properly
add_task(async function() {
  const dbg = await initDebugger("doc-xhr.html");

  const pauseOnAnyCheckbox = getXHRBreakpointCheckbox(dbg);

  await clickPauseOnAny(dbg, "SET_XHR_BREAKPOINT");
  await addXHRBreakpoint(dbg, "1");
  await addXHRBreakpoint(dbg, "2");
  await addXHRBreakpoint(dbg, "3");
  await addXHRBreakpoint(dbg, "4");

  // Remove "2"
  await removeXHRBreakpoint(dbg, 1);

  const listItems = getXHRBreakpointsElements(dbg);
  is(listItems.length, 3, "3 XHR breakpoints display in list");
  is(pauseOnAnyCheckbox.checked, true, "The pause on any is still checked");
  is(
    getXHRBreakpointLabels(listItems).join(""),
    "134",
    "Only the desired breakpoint was removed"
  );
});
