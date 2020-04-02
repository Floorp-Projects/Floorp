/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

async function testTreeOnHide(browser, accDoc, containerID, id, before, after) {
  let acc = findAccessibleChildByID(accDoc, containerID);
  testAccessibleTree(acc, before);

  let onReorder = waitForEvent(EVENT_REORDER, containerID);
  await invokeSetStyle(browser, id, "visibility", "hidden");
  await onReorder;

  testAccessibleTree(acc, after);
}

async function test3(browser, accessible) {
  let tree = {
    SECTION: [
      // container
      {
        SECTION: [
          // parent
          {
            SECTION: [
              // child
              { TEXT_LEAF: [] },
            ],
          },
        ],
      },
      {
        SECTION: [
          // parent2
          {
            SECTION: [
              // child2
              { TEXT_LEAF: [] },
            ],
          },
        ],
      },
    ],
  };
  testAccessibleTree(accessible, tree);

  let onReorder = waitForEvent(EVENT_REORDER, "t3_container");
  await invokeContentTask(browser, [], () => {
    let doc = content.document;
    doc.getElementById("t3_container").style.color = "red";
    doc.getElementById("t3_parent").style.visibility = "hidden";
    doc.getElementById("t3_parent2").style.visibility = "hidden";
  });
  await onReorder;

  tree = {
    SECTION: [
      // container
      {
        SECTION: [
          // child
          { TEXT_LEAF: [] },
        ],
      },
      {
        SECTION: [
          // child2
          { TEXT_LEAF: [] },
        ],
      },
    ],
  };
  testAccessibleTree(accessible, tree);
}

async function test4(browser, accessible) {
  let tree = {
    SECTION: [{ TABLE: [{ ROW: [{ CELL: [] }] }] }],
  };
  testAccessibleTree(accessible, tree);

  let onReorder = waitForEvent(EVENT_REORDER, "t4_parent");
  await invokeContentTask(browser, [], () => {
    let doc = content.document;
    doc.getElementById("t4_container").style.color = "red";
    doc.getElementById("t4_child").style.visibility = "visible";
  });
  await onReorder;

  tree = {
    SECTION: [
      {
        TABLE: [
          {
            ROW: [
              {
                CELL: [
                  {
                    SECTION: [
                      {
                        TEXT_LEAF: [],
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
  testAccessibleTree(accessible, tree);
}

addAccessibleTask(
  "e10s/doc_treeupdate_visibility.html",
  async function(browser, accDoc) {
    let t3Container = findAccessibleChildByID(accDoc, "t3_container");
    let t4Container = findAccessibleChildByID(accDoc, "t4_container");

    await testTreeOnHide(
      browser,
      accDoc,
      "t1_container",
      "t1_parent",
      {
        SECTION: [
          {
            SECTION: [
              {
                SECTION: [{ TEXT_LEAF: [] }],
              },
            ],
          },
        ],
      },
      {
        SECTION: [
          {
            SECTION: [{ TEXT_LEAF: [] }],
          },
        ],
      }
    );

    await testTreeOnHide(
      browser,
      accDoc,
      "t2_container",
      "t2_grandparent",
      {
        SECTION: [
          {
            // container
            SECTION: [
              {
                // grand parent
                SECTION: [
                  {
                    SECTION: [
                      {
                        // child
                        TEXT_LEAF: [],
                      },
                    ],
                  },
                  {
                    SECTION: [
                      {
                        // child2
                        TEXT_LEAF: [],
                      },
                    ],
                  },
                ],
              },
            ],
          },
        ],
      },
      {
        SECTION: [
          {
            // container
            SECTION: [
              {
                // child
                TEXT_LEAF: [],
              },
            ],
          },
          {
            SECTION: [
              {
                // child2
                TEXT_LEAF: [],
              },
            ],
          },
        ],
      }
    );

    await test3(browser, t3Container);
    await test4(browser, t4Container);

    await testTreeOnHide(
      browser,
      accDoc,
      "t5_container",
      "t5_subcontainer",
      {
        SECTION: [
          {
            // container
            SECTION: [
              {
                // subcontainer
                TABLE: [
                  {
                    ROW: [
                      {
                        CELL: [
                          {
                            SECTION: [
                              {
                                // child
                                TEXT_LEAF: [],
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
      },
      {
        SECTION: [
          {
            // container
            SECTION: [
              {
                // child
                TEXT_LEAF: [],
              },
            ],
          },
        ],
      }
    );

    await testTreeOnHide(
      browser,
      accDoc,
      "t6_container",
      "t6_subcontainer",
      {
        SECTION: [
          {
            // container
            SECTION: [
              {
                // subcontainer
                TABLE: [
                  {
                    ROW: [
                      {
                        CELL: [
                          {
                            TABLE: [
                              {
                                // nested table
                                ROW: [
                                  {
                                    CELL: [
                                      {
                                        SECTION: [
                                          {
                                            // child
                                            TEXT_LEAF: [],
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
                  },
                ],
              },
              {
                SECTION: [
                  {
                    // child2
                    TEXT_LEAF: [],
                  },
                ],
              },
            ],
          },
        ],
      },
      {
        SECTION: [
          {
            // container
            SECTION: [
              {
                // child
                TEXT_LEAF: [],
              },
            ],
          },
          {
            SECTION: [
              {
                // child2
                TEXT_LEAF: [],
              },
            ],
          },
        ],
      }
    );
  },
  { iframe: true, remoteIframe: true }
);
