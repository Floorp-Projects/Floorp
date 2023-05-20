/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This test attempts to exercise automatic triggering of typeaheadfind
 * within RDM content. It does this by simulating keystrokes while
 * various elements in the RDM content are focused.

 * The test currently does not work due to hitting the assert in
 * Bug 516128.
 */

const TEST_URL =
  "data:text/html;charset=utf-8," +
  '<body id="body"><input id="input" type="text"/><p>text</body>';

addRDMTask(TEST_URL, async function ({ ui, manager }) {
  // Turn on the pref that allows meta viewport support.
  await pushPref("accessibility.typeaheadfind", true);

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
    await SpecialPowers.spawn(browser, [{ e }], async function (args) {
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

    await SpecialPowers.spawn(browser, [], async function () {
      // Clear focus.
      content.document.activeElement.blur();
    });
  }
});
