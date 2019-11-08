/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const NESTED_FISSION_DOC_BODY_ID = "nested-fission-body";
const NESTED_FISSION_IFRAME_ID = "nested-fission-iframe";
const nestedURL = new URL(
  `http://example.net${CURRENT_FILE_DIR}fission_document_builder.sjs`
);
nestedURL.searchParams.append(
  "html",
  `<html>
      <head>
        <meta charset="utf-8"/>
        <title>Accessibility Nested Fission Frame Test</title>
      </head>
      <body id="${NESTED_FISSION_DOC_BODY_ID}">
        <table id="table">
          <tr>
            <td>cell1</td>
            <td>cell2</td>
          </tr>
        </table>
        <ul id="ul">
          <li id="li">item1</li>
        </ul>
      </body>
    </html>`
);

function getOsPid(browsingContext) {
  return browsingContext.currentWindowGlobal.osPid;
}

addAccessibleTask(
  `<iframe id="${NESTED_FISSION_IFRAME_ID}" src="${nestedURL.href}"/>`,
  async function(browser, fissionDocAcc, contentDocAcc) {
    let nestedDocAcc = findAccessibleChildByID(
      fissionDocAcc,
      NESTED_FISSION_DOC_BODY_ID
    );

    ok(fissionDocAcc, "Fission document accessible is present");
    ok(nestedDocAcc, "Nested fission document accessible is present");

    const state = {};
    nestedDocAcc.getState(state, {});
    if (state.value & STATE_BUSY) {
      nestedDocAcc = (await waitForEvent(
        EVENT_DOCUMENT_LOAD_COMPLETE,
        NESTED_FISSION_DOC_BODY_ID
      )).accessible;
    }

    if (gFissionBrowser) {
      isnot(
        getOsPid(browser.browsingContext),
        getOsPid(browser.browsingContext.getChildren()[0]),
        `Content and fission documents are in separate processes.`
      );
      isnot(
        getOsPid(browser.browsingContext),
        getOsPid(browser.browsingContext.getChildren()[0].getChildren()[0]),
        `Content and nested fission documents are in separate processes.`
      );
      isnot(
        getOsPid(browser.browsingContext.getChildren()[0]),
        getOsPid(browser.browsingContext.getChildren()[0].getChildren()[0]),
        `Fission and nested fission documents are in separate processes.`
      );
    } else {
      is(
        getOsPid(browser.browsingContext),
        getOsPid(browser.browsingContext.getChildren()[0]),
        `Content and fission documents are in same processes.`
      );
      is(
        getOsPid(browser.browsingContext),
        getOsPid(browser.browsingContext.getChildren()[0].getChildren()[0]),
        `Content and nested fission documents are in same processes.`
      );
    }

    const tree = {
      DOCUMENT: [
        {
          INTERNAL_FRAME: [
            {
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
            },
          ],
        },
      ],
    };
    testAccessibleTree(contentDocAcc, tree);

    const nestedIframeAcc = fissionDocAcc.getChildAt(0);
    is(
      nestedIframeAcc.getChildAt(0),
      nestedDocAcc,
      "Nested fission document for nested IFRAME matches."
    );

    is(
      nestedDocAcc.parent,
      nestedIframeAcc,
      "Nested fission document's parent matches the nested IFRAME."
    );
  },
  { topLevel: false, iframe: true }
);
