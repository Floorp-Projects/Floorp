/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  `<table id="table">
    <tr>
      <td>cell1</td>
      <td>cell2</td>
    </tr>
  </table>
  <ul id="ul">
    <li id="li">item1</li>
  </ul>`,
  async function(browser, fissionDocAcc, contentDocAcc) {
    ok(fissionDocAcc, "Fission document accessible is present");
    (gFissionBrowser ? isnot : is)(
      browser.browsingContext.currentWindowGlobal.osPid,
      browser.browsingContext.getChildren()[0].currentWindowGlobal.osPid,
      `Content and fission documents are in ${
        gFissionBrowser ? "separate processes" : "same process"
      }.`
    );

    const tree = {
      DOCUMENT: [
        {
          INTERNAL_FRAME: [
            {
              DOCUMENT: [
                {
                  TABLE: [
                    {
                      ROW: [
                        { CELL: [{ TEXT_LEAF: [] }] },
                        { CELL: [{ TEXT_LEAF: [] }] },
                      ],
                    },
                  ],
                },
                {
                  LIST: [
                    {
                      LISTITEM: [{ STATICTEXT: [] }, { TEXT_LEAF: [] }],
                    },
                  ],
                },
              ],
            },
          ],
        },
      ],
    };
    testAccessibleTree(contentDocAcc, tree);

    const iframeAcc = contentDocAcc.getChildAt(0);
    is(
      iframeAcc.getChildAt(0),
      fissionDocAcc,
      "Fission document for the IFRAME matches."
    );

    is(
      fissionDocAcc.parent,
      iframeAcc,
      "Fission document's parent matches the IFRAME."
    );
  },
  { topLevel: false, iframe: true }
);
