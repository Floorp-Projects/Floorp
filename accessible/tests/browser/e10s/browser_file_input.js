/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/name.js */
loadScripts({ name: "name.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  `
<input type="file" id="noName">
<input type="file" id="ariaLabel" aria-label="ariaLabel">
<label>wrappingLabel <input type="file" id="wrappingLabel"></label>
<label for="labelFor">labelFor</label> <input type="file" id="labelFor">
<input type="file" id="title" title="title">
  `,
  async function (browser, docAcc) {
    const browseButton = "Browse…";
    const noFileSuffix = `${browseButton} No file selected.`;
    const noName = findAccessibleChildByID(docAcc, "noName");
    testName(noName, noFileSuffix);
    const ariaLabel = findAccessibleChildByID(docAcc, "ariaLabel");
    testName(ariaLabel, `ariaLabel ${noFileSuffix}`);
    const wrappingLabel = findAccessibleChildByID(docAcc, "wrappingLabel");
    testName(wrappingLabel, `wrappingLabel ${noFileSuffix}`);
    const labelFor = findAccessibleChildByID(docAcc, "labelFor");
    testName(labelFor, `labelFor ${noFileSuffix}`);
    const title = findAccessibleChildByID(docAcc, "title");
    testName(title, noFileSuffix);
    testDescr(title, "title");

    // Test that the name of the button changes correctly when a file is chosen.
    function chooseFile(id) {
      return invokeContentTask(browser, [id], contentId => {
        const MockFilePicker = content.SpecialPowers.MockFilePicker;
        MockFilePicker.init(content.browsingContext);
        MockFilePicker.useBlobFile();
        MockFilePicker.returnValue = MockFilePicker.returnOK;
        const input = content.document.getElementById(contentId);
        const inputReceived = new Promise(resolve =>
          input.addEventListener(
            "input",
            event => {
              MockFilePicker.cleanup();
              resolve(event.target.files[0].name);
            },
            { once: true }
          )
        );

        // Activate the page to allow opening the file picker.
        content.SpecialPowers.wrap(
          content.document
        ).notifyUserGestureActivation();

        input.click();

        return inputReceived;
      });
    }

    info("noName: Choosing file");
    let nameChanged = waitForEvent(EVENT_NAME_CHANGE, "noName");
    const fn = await chooseFile("noName");
    // e.g. "Browse…helloworld.txt"
    const withFileSuffix = `${browseButton} ${fn}`;
    await nameChanged;
    testName(noName, withFileSuffix);

    info("ariaLabel: Choosing file");
    nameChanged = waitForEvent(EVENT_NAME_CHANGE, "ariaLabel");
    await chooseFile("ariaLabel");
    await nameChanged;
    testName(ariaLabel, `ariaLabel ${withFileSuffix}`);

    info("wrappingLabel: Choosing file");
    nameChanged = waitForEvent(EVENT_NAME_CHANGE, "wrappingLabel");
    await chooseFile("wrappingLabel");
    await nameChanged;
    testName(wrappingLabel, `wrappingLabel ${withFileSuffix}`);
  },
  { topLevel: true, chrome: true }
);
