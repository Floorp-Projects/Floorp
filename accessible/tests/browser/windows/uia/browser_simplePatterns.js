/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../../mochitest/role.js */
/* import-globals-from ../../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

/* eslint-disable camelcase */
const ExpandCollapseState_Collapsed = 0;
const ExpandCollapseState_Expanded = 1;
const ToggleState_Off = 0;
const ToggleState_On = 1;
const ToggleState_Indeterminate = 2;
/* eslint-enable camelcase */

/**
 * Test the Invoke pattern.
 */
addUiaTask(
  `
<button id="button">button</button>
<p id="p">p</p>
<input id="checkbox" type="checkbox">
  `,
  async function testInvoke() {
    await definePyVar("doc", `getDocUia()`);
    await assignPyVarToUiaWithId("button");
    await definePyVar("pattern", `getUiaPattern(button, "Invoke")`);
    ok(await runPython(`bool(pattern)`), "button has Invoke pattern");
    info("Calling Invoke on button");
    // The button will get focus when it is clicked.
    let focused = waitForEvent(EVENT_FOCUS, "button");
    // The UIA -> IA2 proxy doesn't fire the Invoked event.
    if (gIsUiaEnabled) {
      await setUpWaitForUiaEvent("Invoke_Invoked", "button");
    }
    await runPython(`pattern.Invoke()`);
    await focused;
    ok(true, "button got focus");
    if (gIsUiaEnabled) {
      await waitForUiaEvent();
      ok(true, "button got Invoked event");
    }

    await testPatternAbsent("p", "Invoke");
    // The Microsoft IA2 -> UIA proxy doesn't follow Microsoft's own rules.
    if (gIsUiaEnabled) {
      // Check boxes expose the Toggle pattern, so they should not expose the
      // Invoke pattern.
      await testPatternAbsent("checkbox", "Invoke");
    }
  }
);

/**
 * Test the Toggle pattern.
 */
addUiaTask(
  `
<input id="checkbox" type="checkbox" checked>
<button id="toggleButton" aria-pressed="false">toggle</button>
<button id="button">button</button>
<p id="p">p</p>

<script>
  // When checkbox is clicked and it is not checked, make it indeterminate.
  document.getElementById("checkbox").addEventListener("click", evt => {
    // Within the event listener, .checked is reversed and you can't set
    // .indeterminate. Work around this by deferring and handling the changes
    // ourselves.
    evt.preventDefault();
    const target = evt.target;
    setTimeout(() => {
      if (target.checked) {
        target.checked = false;
      } else {
        target.indeterminate = true;
      }
    }, 0);
  });

  // When toggleButton is clicked, set aria-pressed to true.
  document.getElementById("toggleButton").addEventListener("click", evt => {
    evt.target.ariaPressed = "true";
  });
</script>
  `,
  async function testToggle() {
    await definePyVar("doc", `getDocUia()`);
    await assignPyVarToUiaWithId("checkbox");
    await definePyVar("pattern", `getUiaPattern(checkbox, "Toggle")`);
    ok(await runPython(`bool(pattern)`), "checkbox has Toggle pattern");
    is(
      await runPython(`pattern.CurrentToggleState`),
      ToggleState_On,
      "checkbox has ToggleState_On"
    );
    // The IA2 -> UIA proxy doesn't fire ToggleState prop change events.
    if (gIsUiaEnabled) {
      info("Calling Toggle on checkbox");
      await setUpWaitForUiaPropEvent("ToggleToggleState", "checkbox");
      await runPython(`pattern.Toggle()`);
      await waitForUiaEvent();
      ok(true, "Got ToggleState prop change event on checkbox");
      is(
        await runPython(`pattern.CurrentToggleState`),
        ToggleState_Off,
        "checkbox has ToggleState_Off"
      );
      info("Calling Toggle on checkbox");
      await setUpWaitForUiaPropEvent("ToggleToggleState", "checkbox");
      await runPython(`pattern.Toggle()`);
      await waitForUiaEvent();
      ok(true, "Got ToggleState prop change event on checkbox");
      is(
        await runPython(`pattern.CurrentToggleState`),
        ToggleState_Indeterminate,
        "checkbox has ToggleState_Indeterminate"
      );
    }

    await assignPyVarToUiaWithId("toggleButton");
    await definePyVar("pattern", `getUiaPattern(toggleButton, "Toggle")`);
    ok(await runPython(`bool(pattern)`), "toggleButton has Toggle pattern");
    is(
      await runPython(`pattern.CurrentToggleState`),
      ToggleState_Off,
      "toggleButton has ToggleState_Off"
    );
    if (gIsUiaEnabled) {
      info("Calling Toggle on toggleButton");
      await setUpWaitForUiaPropEvent("ToggleToggleState", "toggleButton");
      await runPython(`pattern.Toggle()`);
      await waitForUiaEvent();
      ok(true, "Got ToggleState prop change event on toggleButton");
      is(
        await runPython(`pattern.CurrentToggleState`),
        ToggleState_On,
        "toggleButton has ToggleState_Off"
      );
    }

    await testPatternAbsent("button", "Toggle");
    await testPatternAbsent("p", "Toggle");
  }
);

/**
 * Test the ExpandCollapse pattern.
 */
addUiaTask(
  `
<details>
  <summary id="summary">summary</summary>
  details
</details>
<button id="popup" aria-haspopup="true">popup</button>
<button id="button">button</button>
<script>
  // When popup is clicked, set aria-expanded to true.
  document.getElementById("popup").addEventListener("click", evt => {
    evt.target.ariaExpanded = "true";
  });
</script>
  `,
  async function testExpandCollapse() {
    await definePyVar("doc", `getDocUia()`);
    await assignPyVarToUiaWithId("summary");
    await definePyVar("pattern", `getUiaPattern(summary, "ExpandCollapse")`);
    ok(await runPython(`bool(pattern)`), "summary has ExpandCollapse pattern");
    is(
      await runPython(`pattern.CurrentExpandCollapseState`),
      ExpandCollapseState_Collapsed,
      "summary has ExpandCollapseState_Collapsed"
    );
    // The IA2 -> UIA proxy doesn't fire ToggleState prop change events, nor
    // does it fail when Expand/Collapse is called on a control which is
    // already in the desired state.
    if (gIsUiaEnabled) {
      info("Calling Expand on summary");
      await setUpWaitForUiaPropEvent(
        "ExpandCollapseExpandCollapseState",
        "summary"
      );
      await runPython(`pattern.Expand()`);
      await waitForUiaEvent();
      ok(
        true,
        "Got ExpandCollapseExpandCollapseState prop change event on summary"
      );
      is(
        await runPython(`pattern.CurrentExpandCollapseState`),
        ExpandCollapseState_Expanded,
        "summary has ExpandCollapseState_Expanded"
      );
      info("Calling Expand on summary");
      let failed = false;
      try {
        await runPython(`pattern.Expand()`);
      } catch {
        failed = true;
      }
      ok(failed, "Expand on summary failed");
      info("Calling Collapse on summary");
      await setUpWaitForUiaPropEvent(
        "ExpandCollapseExpandCollapseState",
        "summary"
      );
      await runPython(`pattern.Collapse()`);
      await waitForUiaEvent();
      ok(
        true,
        "Got ExpandCollapseExpandCollapseState prop change event on summary"
      );
      is(
        await runPython(`pattern.CurrentExpandCollapseState`),
        ExpandCollapseState_Collapsed,
        "summary has ExpandCollapseState_Collapsed"
      );
      info("Calling Collapse on summary");
      failed = false;
      try {
        await runPython(`pattern.Collapse()`);
      } catch {
        failed = true;
      }
      ok(failed, "Collapse on summary failed");
    }

    await assignPyVarToUiaWithId("popup");
    // Initially, popup has aria-haspopup but not aria-expanded. That should
    // be exposed as collapsed.
    await definePyVar("pattern", `getUiaPattern(popup, "ExpandCollapse")`);
    ok(await runPython(`bool(pattern)`), "popup has ExpandCollapse pattern");
    // The IA2 -> UIA proxy doesn't expose ExpandCollapseState_Collapsed for
    // aria-haspopup without aria-expanded.
    if (gIsUiaEnabled) {
      is(
        await runPython(`pattern.CurrentExpandCollapseState`),
        ExpandCollapseState_Collapsed,
        "popup has ExpandCollapseState_Collapsed"
      );
      info("Calling Expand on popup");
      await setUpWaitForUiaPropEvent(
        "ExpandCollapseExpandCollapseState",
        "popup"
      );
      await runPython(`pattern.Expand()`);
      await waitForUiaEvent();
      ok(
        true,
        "Got ExpandCollapseExpandCollapseState prop change event on popup"
      );
      is(
        await runPython(`pattern.CurrentExpandCollapseState`),
        ExpandCollapseState_Expanded,
        "popup has ExpandCollapseState_Expanded"
      );
    }

    await testPatternAbsent("button", "ExpandCollapse");
  }
);

/**
 * Test the ScrollItem pattern.
 */
addUiaTask(
  `
<hr style="height: 100vh;">
<button id="button">button</button>
  `,
  async function testScrollItem(browser, docAcc) {
    await definePyVar("doc", `getDocUia()`);
    await assignPyVarToUiaWithId("button");
    await definePyVar("pattern", `getUiaPattern(button, "ScrollItem")`);
    ok(await runPython(`bool(pattern)`), "button has ScrollItem pattern");
    const button = findAccessibleChildByID(docAcc, "button");
    testStates(button, STATE_OFFSCREEN);
    info("Calling ScrollIntoView on button");
    // UIA doesn't have an event for this.
    let scrolled = waitForEvent(EVENT_SCROLLING_END, docAcc);
    await runPython(`pattern.ScrollIntoView()`);
    await scrolled;
    ok(true, "Document scrolled");
    testStates(button, 0, 0, STATE_OFFSCREEN);
  }
);
