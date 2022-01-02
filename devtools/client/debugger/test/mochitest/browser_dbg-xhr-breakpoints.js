/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  info("Test XHR requests done very early during page load");

  const dbg = await initDebugger("doc-xhr.html", "fetch.js");

  await addXHRBreakpoint(dbg, "doc", "GET");

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [EXAMPLE_REMOTE_URL + "doc-early-xhr.html"],
    remoteUrl => {
      const firstIframe = content.document.createElement("iframe");
      content.document.body.append(firstIframe);
      firstIframe.src = remoteUrl;
    }
  );

  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  const whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(whyPaused, `Paused on XMLHttpRequest`);

  await resume(dbg);

  await dbg.actions.removeXHRBreakpoint(0);

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [EXAMPLE_REMOTE_URL + "doc-early-xhr.html"],
    remoteUrl => {
      const secondIframe = content.document.createElement("iframe");
      content.document.body.append(secondIframe);
      secondIframe.src = remoteUrl;
    }
  );

  // Wait for some time, in order to wait for it to be paused
  // in case we regress
  await waitForTime(1000);

  assertNotPaused(dbg);
});

add_task(async function() {
  info("Test simple XHR breakpoints set before doing the request");

  const dbg = await initDebugger("doc-xhr.html", "fetch.js");

  await addXHRBreakpoint(dbg, "doc", "GET");

  invokeInTab("main", "doc-xhr.html");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  await resume(dbg);

  await dbg.actions.removeXHRBreakpoint(0);
  await invokeInTab("main", "doc-xhr.html");
  assertNotPaused(dbg);

  info("Test that we do not pause on different method type");
  await addXHRBreakpoint(dbg, "doc", "POST");
  await invokeInTab("main", "doc-xhr.html");
  assertNotPaused(dbg);
});

// Tests the "pause on any URL" checkbox works properly
add_task(async function() {
  info("Test 'pause on any URL'");
  const dbg = await initDebugger("doc-xhr.html", "fetch.js");

  // Enable pause on any URL
  await clickPauseOnAny(dbg, "SET_XHR_BREAKPOINT");

  invokeInTab("main", "doc-xhr.html");
  await waitForPaused(dbg);
  await resume(dbg);
  await assertDebuggerTabHighlight(dbg);

  invokeInTab("main", "fetch.js");
  await waitForPaused(dbg);
  await resume(dbg);
  await assertDebuggerTabHighlight(dbg);

  invokeInTab("main", "README.md");
  await waitForPaused(dbg);
  await resume(dbg);
  await assertDebuggerTabHighlight(dbg);

  // Disable pause on any URL
  await clickPauseOnAny(dbg, "DISABLE_XHR_BREAKPOINT");
  invokeInTab("main", "README.md");
  assertNotPaused(dbg);

  // Turn off the checkbox
  await dbg.actions.removeXHRBreakpoint(0);
});

// Tests removal works properly
add_task(async function() {
  info("Assert the frontend state when removing breakpoints");
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

  await waitForDispatch(dbg.store, "SET_XHR_BREAKPOINT");
}

async function removeXHRBreakpoint(dbg, index) {
  info("Removing a XHR breakpoint");

  const closeButtons = dbg.win.document.querySelectorAll(
    ".xhr-breakpoints-pane .close-btn"
  );
  if (closeButtons[index]) {
    closeButtons[index].click();
  }

  await waitForDispatch(dbg.store, "REMOVE_XHR_BREAKPOINT");
}

function getXHRBreakpointsElements(dbg) {
  return [
    ...dbg.win.document.querySelectorAll(
      ".xhr-breakpoints-pane .xhr-container"
    ),
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
  await waitForDispatch(dbg.store, expectedEvent);
}

async function assertDebuggerTabHighlight(dbg) {
  await waitUntil(() => !dbg.toolbox.isHighlighted("jsdebugger"));
  ok(true, "Debugger is no longer highlighted after resume");
}
