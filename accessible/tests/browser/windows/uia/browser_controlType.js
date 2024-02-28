/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable camelcase */
const UIA_ButtonControlTypeId = 50000;
const UIA_DocumentControlTypeId = 50030;
/* eslint-enable camelcase */

addAccessibleTask(
  `
<button id="button">button</button>
  `,
  async function (browser, docAcc) {
    let controlType = await runPython(`
      global doc
      doc = getDocUia()
      return doc.CurrentControlType
    `);
    is(controlType, UIA_DocumentControlTypeId, "doc has correct control type");
    controlType = await runPython(`
      button = findUiaByDomId(doc, "button")
      return button.CurrentControlType
    `);
    is(controlType, UIA_ButtonControlTypeId, "button has correct control type");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);
