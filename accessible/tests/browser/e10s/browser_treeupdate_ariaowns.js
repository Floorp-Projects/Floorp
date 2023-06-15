/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

async function testContainer1(browser, accDoc) {
  const id = "t1_container";
  const docID = getAccessibleDOMNodeID(accDoc);
  const acc = findAccessibleChildByID(accDoc, id);

  /* ================= Initial tree test ==================================== */
  // children are swapped by ARIA owns
  let tree = {
    SECTION: [{ CHECKBUTTON: [{ SECTION: [] }] }, { PUSHBUTTON: [] }],
  };
  testAccessibleTree(acc, tree);

  /* ================ Change ARIA owns ====================================== */
  let onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeSetAttribute(browser, id, "aria-owns", "t1_button t1_subdiv");
  await onReorder;

  // children are swapped again, button and subdiv are appended to
  // the children.
  tree = {
    SECTION: [
      { CHECKBUTTON: [] }, // checkbox, native order
      { PUSHBUTTON: [] }, // button, rearranged by ARIA own
      { SECTION: [] }, // subdiv from the subtree, ARIA owned
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================ Remove ARIA owns ====================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeSetAttribute(browser, id, "aria-owns");
  await onReorder;

  // children follow the DOM order
  tree = {
    SECTION: [{ PUSHBUTTON: [] }, { CHECKBUTTON: [{ SECTION: [] }] }],
  };
  testAccessibleTree(acc, tree);

  /* ================ Set ARIA owns ========================================= */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeSetAttribute(browser, id, "aria-owns", "t1_button t1_subdiv");
  await onReorder;

  // children are swapped again, button and subdiv are appended to
  // the children.
  tree = {
    SECTION: [
      { CHECKBUTTON: [] }, // checkbox
      { PUSHBUTTON: [] }, // button, rearranged by ARIA own
      { SECTION: [] }, // subdiv from the subtree, ARIA owned
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================ Add ID to ARIA owns =================================== */
  onReorder = waitForEvent(EVENT_REORDER, docID);
  await invokeSetAttribute(
    browser,
    id,
    "aria-owns",
    "t1_button t1_subdiv t1_group"
  );
  await onReorder;

  // children are swapped again, button and subdiv are appended to
  // the children.
  tree = {
    SECTION: [
      { CHECKBUTTON: [] }, // t1_checkbox
      { PUSHBUTTON: [] }, // button, t1_button
      { SECTION: [] }, // subdiv from the subtree, t1_subdiv
      { GROUPING: [] }, // group from outside, t1_group
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================ Append element ======================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeContentTask(browser, [id], contentId => {
    let div = content.document.createElement("div");
    div.setAttribute("id", "t1_child3");
    div.setAttribute("role", "radio");
    content.document.getElementById(contentId).appendChild(div);
  });
  await onReorder;

  // children are invalidated, they includes aria-owns swapped kids and
  // newly inserted child.
  tree = {
    SECTION: [
      { CHECKBUTTON: [] }, // existing explicit, t1_checkbox
      { RADIOBUTTON: [] }, // new explicit, t1_child3
      { PUSHBUTTON: [] }, // ARIA owned, t1_button
      { SECTION: [] }, // ARIA owned, t1_subdiv
      { GROUPING: [] }, // ARIA owned, t1_group
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================ Remove element ======================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeContentTask(browser, [], () => {
    content.document.getElementById("t1_span").remove();
  });
  await onReorder;

  // subdiv should go away
  tree = {
    SECTION: [
      { CHECKBUTTON: [] }, // explicit, t1_checkbox
      { RADIOBUTTON: [] }, // explicit, t1_child3
      { PUSHBUTTON: [] }, // ARIA owned, t1_button
      { GROUPING: [] }, // ARIA owned, t1_group
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================ Remove ID ============================================= */
  onReorder = waitForEvent(EVENT_REORDER, docID);
  await invokeSetAttribute(browser, "t1_group", "id");
  await onReorder;

  tree = {
    SECTION: [
      { CHECKBUTTON: [] },
      { RADIOBUTTON: [] },
      { PUSHBUTTON: [] }, // ARIA owned, t1_button
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================ Set ID ================================================ */
  onReorder = waitForEvent(EVENT_REORDER, docID);
  await invokeSetAttribute(browser, "t1_grouptmp", "id", "t1_group");
  await onReorder;

  tree = {
    SECTION: [
      { CHECKBUTTON: [] },
      { RADIOBUTTON: [] },
      { PUSHBUTTON: [] }, // ARIA owned, t1_button
      { GROUPING: [] }, // ARIA owned, t1_group, previously t1_grouptmp
    ],
  };
  testAccessibleTree(acc, tree);
}

async function removeContainer(browser, accDoc) {
  const id = "t2_container1";
  const acc = findAccessibleChildByID(accDoc, id);

  let tree = {
    SECTION: [
      { CHECKBUTTON: [] }, // ARIA owned, 't2_owned'
    ],
  };
  testAccessibleTree(acc, tree);

  let onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeContentTask(browser, [], () => {
    content.document
      .getElementById("t2_container2")
      .removeChild(content.document.getElementById("t2_container3"));
  });
  await onReorder;

  tree = {
    SECTION: [],
  };
  testAccessibleTree(acc, tree);
}

async function stealAndRecacheChildren(browser, accDoc) {
  const id1 = "t3_container1";
  const id2 = "t3_container2";
  const acc1 = findAccessibleChildByID(accDoc, id1);
  const acc2 = findAccessibleChildByID(accDoc, id2);

  /* ================ Attempt to steal from other ARIA owns ================= */
  let onReorder = waitForEvent(EVENT_REORDER, id2);
  await invokeSetAttribute(browser, id2, "aria-owns", "t3_child");
  await invokeContentTask(browser, [id2], id => {
    let div = content.document.createElement("div");
    div.setAttribute("role", "radio");
    content.document.getElementById(id).appendChild(div);
  });
  await onReorder;

  let tree = {
    SECTION: [
      { CHECKBUTTON: [] }, // ARIA owned
    ],
  };
  testAccessibleTree(acc1, tree);

  tree = {
    SECTION: [{ RADIOBUTTON: [] }],
  };
  testAccessibleTree(acc2, tree);
}

async function showHiddenElement(browser, accDoc) {
  const id = "t4_container1";
  const acc = findAccessibleChildByID(accDoc, id);

  let tree = {
    SECTION: [{ RADIOBUTTON: [] }],
  };
  testAccessibleTree(acc, tree);

  let onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeSetStyle(browser, "t4_child1", "display", "block");
  await onReorder;

  tree = {
    SECTION: [{ CHECKBUTTON: [] }, { RADIOBUTTON: [] }],
  };
  testAccessibleTree(acc, tree);
}

async function rearrangeARIAOwns(browser, accDoc) {
  const id = "t5_container";
  const acc = findAccessibleChildByID(accDoc, id);
  const tests = [
    {
      val: "t5_checkbox t5_radio t5_button",
      roleList: ["CHECKBUTTON", "RADIOBUTTON", "PUSHBUTTON"],
    },
    {
      val: "t5_radio t5_button t5_checkbox",
      roleList: ["RADIOBUTTON", "PUSHBUTTON", "CHECKBUTTON"],
    },
  ];

  for (let { val, roleList } of tests) {
    let onReorder = waitForEvent(EVENT_REORDER, id);
    await invokeSetAttribute(browser, id, "aria-owns", val);
    await onReorder;

    let tree = { SECTION: [] };
    for (let role of roleList) {
      let ch = {};
      ch[role] = [];
      tree.SECTION.push(ch);
    }
    testAccessibleTree(acc, tree);
  }
}

async function removeNotARIAOwnedEl(browser, accDoc) {
  const id = "t6_container";
  const acc = findAccessibleChildByID(accDoc, id);

  let tree = {
    SECTION: [{ TEXT_LEAF: [] }, { GROUPING: [] }],
  };
  testAccessibleTree(acc, tree);

  let onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeContentTask(browser, [id], contentId => {
    content.document
      .getElementById(contentId)
      .removeChild(content.document.getElementById("t6_span"));
  });
  await onReorder;

  tree = {
    SECTION: [{ GROUPING: [] }],
  };
  testAccessibleTree(acc, tree);
}

addAccessibleTask(
  "e10s/doc_treeupdate_ariaowns.html",
  async function (browser, accDoc) {
    await testContainer1(browser, accDoc);
    await removeContainer(browser, accDoc);
    await stealAndRecacheChildren(browser, accDoc);
    await showHiddenElement(browser, accDoc);
    await rearrangeARIAOwns(browser, accDoc);
    await removeNotARIAOwnedEl(browser, accDoc);
  },
  { iframe: true, remoteIframe: true }
);

// Test owning an ancestor which isn't created yet with an iframe in the
// subtree.
addAccessibleTask(
  `
  <span id="a">
    <div id="b" aria-owns="c"></div>
  </span>
  <div id="c">
    <iframe></iframe>
  </div>
  <script>
    document.getElementById("c").setAttribute("aria-owns", "a");
  </script>
  `,
  async function (browser, accDoc) {
    testAccessibleTree(accDoc, {
      DOCUMENT: [
        {
          // b
          SECTION: [
            {
              // c
              SECTION: [{ INTERNAL_FRAME: [{ DOCUMENT: [] }] }],
            },
          ],
        },
      ],
    });
  }
);

// Verify that removing the parent of a DOM-sibling aria-owned child keeps the
// formerly-owned child in the tree.
addAccessibleTask(
  `<input id='x'></input><div aria-owns='x'></div>`,
  async function (browser, accDoc) {
    testAccessibleTree(accDoc, {
      DOCUMENT: [{ SECTION: [{ ENTRY: [] }] }],
    });

    info("Removing the div that aria-owns a DOM sibling");
    let onReorder = waitForEvent(EVENT_REORDER, accDoc);
    await invokeContentTask(browser, [], () => {
      content.document.querySelector("div").remove();
    });
    await onReorder;

    info("Verifying that the formerly-owned child is still present");
    testAccessibleTree(accDoc, {
      DOCUMENT: [{ ENTRY: [] }],
    });
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Verify that removing the parent of multiple DOM-sibling aria-owned children
// keeps all formerly-owned children in the tree.
addAccessibleTask(
  `<input id='x'></input><input id='y'><div aria-owns='x y'></div>`,
  async function (browser, accDoc) {
    testAccessibleTree(accDoc, {
      DOCUMENT: [
        {
          SECTION: [{ ENTRY: [] }, { ENTRY: [] }],
        },
      ],
    });

    info("Removing the div that aria-owns DOM siblings");
    let onReorder = waitForEvent(EVENT_REORDER, accDoc);
    await invokeContentTask(browser, [], () => {
      content.document.querySelector("div").remove();
    });
    await onReorder;

    info("Verifying that the formerly-owned children are still present");
    testAccessibleTree(accDoc, {
      DOCUMENT: [{ ENTRY: [] }, { ENTRY: [] }],
    });
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Verify that reordering owned elements by changing the aria-owns attribute
// properly reorders owned elements.
addAccessibleTask(
  `
<div id="container" aria-owns="b d c a">
  <div id="a" role="button"></div>
  <div id="b" role="checkbox"></div>
</div>
<div id="c" role="radio"></div>
<div id="d"></div>`,
  async function (browser, accDoc) {
    testAccessibleTree(accDoc, {
      DOCUMENT: [
        {
          SECTION: [
            { CHECKBUTTON: [] }, // b
            { SECTION: [] }, // d
            { RADIOBUTTON: [] }, // c
            { PUSHBUTTON: [] }, // a
          ],
        },
      ],
    });

    info("Removing the div that aria-owns other elements");
    let onReorder = waitForEvent(EVENT_REORDER, accDoc);
    await invokeContentTask(browser, [], () => {
      content.document.querySelector("#container").remove();
    });
    await onReorder;

    info(
      "Verify DOM children are removed, order of remaining elements is correct"
    );
    testAccessibleTree(accDoc, {
      DOCUMENT: [
        { RADIOBUTTON: [] }, // c
        { SECTION: [] }, // d
      ],
    });
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Verify that we avoid sending unwanted hide events when doing multiple
// aria-owns relocations in a single tick. Note that we're avoiding testing
// chrome here since parent process locals don't track moves in the same way,
// meaning our mechanism for avoiding duplicate hide events doesn't work.
addAccessibleTask(
  `
<div id='b' aria-owns='a'></div>
<div id='d'></div>
<dd id='f'>
  <div id='a' aria-owns='d'></div>
</dd>
  `,
  async function (browser, accDoc) {
    const b = findAccessibleChildByID(accDoc, "b");
    const waitFor = {
      expected: [
        [EVENT_HIDE, b],
        [EVENT_SHOW, "d"],
        [EVENT_REORDER, accDoc],
      ],
      unexpected: [
        [EVENT_HIDE, "d"],
        [EVENT_REORDER, "a"],
      ],
    };
    info(
      "Verifying that events are fired properly after doing two aria-owns relocations"
    );
    await contentSpawnMutation(browser, waitFor, function () {
      content.document.querySelector("#b").remove();
      content.document.querySelector("#f").remove();
    });
  },
  { chrome: false, iframe: true, remoteIframe: true }
);
