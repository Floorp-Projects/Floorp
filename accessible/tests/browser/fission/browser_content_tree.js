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
  async function (browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    (gIsRemoteIframe ? isnot : is)(
      browser.browsingContext.currentWindowGlobal.osPid,
      browser.browsingContext.children[0].currentWindowGlobal.osPid,
      `Content and IFRAME documents are in ${
        gIsRemoteIframe ? "separate processes" : "same process"
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
                      LISTITEM: [{ LISTITEM_MARKER: [] }, { TEXT_LEAF: [] }],
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
      iframeDocAcc,
      "Document for the IFRAME matches IFRAME's first child."
    );

    is(
      iframeDocAcc.parent,
      iframeAcc,
      "IFRAME document's parent matches the IFRAME."
    );
  },
  { topLevel: false, iframe: true, remoteIframe: true }
);
