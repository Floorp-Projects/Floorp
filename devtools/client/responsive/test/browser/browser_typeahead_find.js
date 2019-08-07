/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This test attempts to exercise automatic triggering of typeaheadfind
 * within RDM content. It does this by simulating keystrokes while
 * various elements in the RDM content are focused.

 * The test currently does not work due to two reasons:
 * 1) The simulated key events do not behave the same as real key events.
 *    Specifically, when this test is run on an un-patched build, it
 *    fails to trigger typeaheadfind when the input element is focused.
 * 2) In order to test that typeahead find has or has not occurred, this
 *    test checks the values held by the findBar associated with the
 *    tab that contains the RDM pane. That findBar has no values, though
 *    they appear correctly when the steps are run interactively. This
 *    indicates that some other findBar is receiving the typeaheadfind
 *    characters.
 */

const TEST_URL =
  "data:text/html;charset=utf-8," +
  '<body id="body"><input id="input" type="text"/><p>text</body>';

addRDMTask(TEST_URL, async function({ ui, manager }) {
  // Turn on the pref that allows meta viewport support.
  await pushPref("accessibility.typeaheadfind", true);

  const store = ui.toolWindow.store;

  // Wait until the viewport has been added.
  await waitUntilState(store, state => state.viewports.length == 1);

  const browser = ui.getViewportBrowser();

  info("--- Starting test output ---");

  const expected = [
    {
      id: "body",
      findTriggered: true,
    },
    {
      id: "input",
      findTriggered: false,
    },
  ];

  for (const e of expected) {
    await ContentTask.spawn(browser, { e }, async function(args) {
      const { e: values } = args;
      const element = content.document.getElementById(values.id);

      // Set focus on the desired element.
      element.focus();
    });

    // Press the 'T' key and see if find is triggered.
    await BrowserTestUtils.synthesizeKey("t", {}, browser);

    const findBar = await gBrowser.getFindBar();

    const findIsTriggered = findBar._findField.value == "t";
    is(
      findIsTriggered,
      e.findTriggered,
      "Text input with focused element " +
        e.id +
        " should " +
        (e.findTriggered ? "" : "not ") +
        "trigger find."
    );
    findBar._findField.value = "";

    await ContentTask.spawn(browser, {}, async function() {
      // Clear focus.
      content.document.activeElement.blur();
    });
  }
});
