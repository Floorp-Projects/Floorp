/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const snippet = `
<select id="select">
  <option>o1</option>
  <optgroup label="g1">
    <option>g1o1</option>
    <option>g1o2</option>
  </optgroup>
  <optgroup label="g2">
    <option>g2o1</option>
    <option>g2o2</option>
  </optgroup>
  <option>o2</option>
</select>
`;

addAccessibleTask(
  snippet,
  async function (browser, accDoc) {
    await invokeFocus(browser, "select");
    // Expand the select. A dropdown item should get focus.
    // Note that the dropdown is rendered in the parent process.
    let focused = waitForEvent(
      EVENT_FOCUS,
      event => event.accessible.role == ROLE_COMBOBOX_OPTION,
      "Dropdown item focused after select expanded"
    );
    await invokeContentTask(browser, [], () => {
      const { ContentTaskUtils } = ChromeUtils.importESModule(
        "resource://testing-common/ContentTaskUtils.sys.mjs"
      );
      const EventUtils = ContentTaskUtils.getEventUtils(content);
      EventUtils.synthesizeKey("VK_DOWN", { altKey: true }, content);
    });
    info("Waiting for parent focus");
    let event = await focused;
    let dropdown = event.accessible.parent;

    let selectedOptionChildren = [];
    if (MAC) {
      // Checkmark is part of the Mac menu styling.
      selectedOptionChildren = [{ STATICTEXT: [] }];
    }
    let tree = {
      COMBOBOX_LIST: [
        { COMBOBOX_OPTION: selectedOptionChildren },
        { GROUPING: [{ COMBOBOX_OPTION: [] }, { COMBOBOX_OPTION: [] }] },
        { GROUPING: [{ COMBOBOX_OPTION: [] }, { COMBOBOX_OPTION: [] }] },
        { COMBOBOX_OPTION: [] },
      ],
    };
    testAccessibleTree(dropdown, tree);

    // Collapse the select. Focus should return to the select.
    focused = waitForEvent(
      EVENT_FOCUS,
      "select",
      "select focused after collapsed"
    );
    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
    info("Waiting for child focus");
    await focused;
  },
  { iframe: true, remoteIframe: true }
);
