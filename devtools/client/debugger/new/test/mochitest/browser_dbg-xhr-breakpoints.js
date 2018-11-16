/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

 async function addXHRBreakpoint(dbg, text) {
  info("Adding a XHR breakpoint");

  const plusIcon = findElementWithSelector(dbg, ".xhr-breakpoints-pane .plus");
  if (plusIcon) {
    plusIcon.click();
  }
  findElementWithSelector(dbg, ".xhr-input").focus();
  type(dbg, text);
  pressKey(dbg, "Enter");

  await waitForDispatch(dbg, "SET_XHR_BREAKPOINT");
}

async function removeXHRBreakpoint(dbg, index) {
  info("Removing a XHR breakpoint");

  const closeButtons = dbg.win.document.querySelectorAll(".xhr-breakpoints-pane .close-btn");
  if (closeButtons[index]) {
    closeButtons[index].click();
  }

  await waitForDispatch(dbg, "REMOVE_XHR_BREAKPOINT");
}

function getXHRBreakpointsElements(dbg) {
  return [...dbg.win.document.querySelectorAll(".xhr-breakpoints-pane .xhr-container")];
}

function getXHRBreakpointLabels(elements) {
  return elements.map(element => element.title);
}

function getXHRBreakpointCheckbox(dbg) {
  return findElementWithSelector(dbg, ".xhr-breakpoints-pane .breakpoints-exceptions input");
}

async function clickPauseOnAny(dbg, expectedEvent) {
  getXHRBreakpointCheckbox(dbg).click();
  await waitForDispatch(dbg, expectedEvent);
}

// Tests that a basic XHR breakpoint works for get and POST is ignored
add_task(async function() {
  const dbg = await initDebugger("doc-xhr.html");
  await waitForSources(dbg, "fetch.js");
  await dbg.actions.setXHRBreakpoint("doc", "GET");
  invokeInTab("main", "doc-xhr.html");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  resume(dbg);

  await dbg.actions.removeXHRBreakpoint(0);
  invokeInTab("main", "doc-xhr.html");
  assertNotPaused(dbg);

  await dbg.actions.setXHRBreakpoint("doc-xhr.html", "POST");
  invokeInTab("main", "doc");
  assertNotPaused(dbg);
});

// Tests the "pause on any URL" checkbox works properly
add_task(async function() {
  const dbg = await initDebugger("doc-xhr.html");
  await waitForSources(dbg, "fetch.js");

  info("HERE 1");

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
  info("HERE 4");
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
  is(
    pauseOnAnyCheckbox.checked, true, 
    "The pause on any is still checked"
  );
  is(
    getXHRBreakpointLabels(listItems).join(""),
    "134",
    "Only the desired breakpoint was removed"
  );

});