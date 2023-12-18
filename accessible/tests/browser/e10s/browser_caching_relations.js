/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
requestLongerTimeout(2);

/**
 * A test specification that has the following format:
 * [
 *   attr                 relevant aria attribute
 *   hostRelation         corresponding host relation type
 *   dependantRelation    corresponding dependant relation type
 * ]
 */
const attrRelationsSpec = [
  ["aria-labelledby", RELATION_LABELLED_BY, RELATION_LABEL_FOR],
  ["aria-describedby", RELATION_DESCRIBED_BY, RELATION_DESCRIPTION_FOR],
  ["aria-controls", RELATION_CONTROLLER_FOR, RELATION_CONTROLLED_BY],
  ["aria-flowto", RELATION_FLOWS_TO, RELATION_FLOWS_FROM],
  ["aria-details", RELATION_DETAILS, RELATION_DETAILS_FOR],
  ["aria-errormessage", RELATION_ERRORMSG, RELATION_ERRORMSG_FOR],
];

/**
 * Test caching of relations between accessible objects.
 */
addAccessibleTask(
  `
  <div id="dependant1">label</div>
  <div id="dependant2">label2</div>
  <div role="checkbox" id="host"></div>`,
  async function (browser, accDoc) {
    for (let spec of attrRelationsSpec) {
      await testRelated(browser, accDoc, ...spec);
    }
  },
  { iframe: true, remoteIframe: true }
);

/**
 * Test caching of relations with respect to label objects and their "for" attr.
 */
addAccessibleTask(
  `
  <input type="checkbox" id="dependant1">
  <input type="checkbox" id="dependant2">
  <label id="host">label</label>`,
  async function (browser, accDoc) {
    await testRelated(
      browser,
      accDoc,
      "for",
      RELATION_LABEL_FOR,
      RELATION_LABELLED_BY
    );
  },
  { iframe: true, remoteIframe: true }
);

/**
 * Test rel caching for element with existing relation attribute.
 */
addAccessibleTask(
  `<div id="label">label</div><button id="button" aria-labelledby="label">`,
  async function (browser, accDoc) {
    const button = findAccessibleChildByID(accDoc, "button");
    const label = findAccessibleChildByID(accDoc, "label");

    await testCachedRelation(button, RELATION_LABELLED_BY, label);
    await testCachedRelation(label, RELATION_LABEL_FOR, button);
  },
  { iframe: true, remoteIframe: true }
);

/**
 * Test caching of relations with respect to output objects and their "for" attr.
 */
addAccessibleTask(
  `
  <form oninput="host.value=parseInt(dependant1.value)+parseInt(dependant2.value)">
    <input type="number" id="dependant1" value="50"> +
    <input type="number" id="dependant2" value="25"> =
    <output name="host" id="host"></output>
  </form>`,
  async function (browser, accDoc) {
    await testRelated(
      browser,
      accDoc,
      "for",
      RELATION_CONTROLLED_BY,
      RELATION_CONTROLLER_FOR
    );
  },
  { iframe: true, remoteIframe: true }
);

/**
 * Test rel caching for <label> element with existing "for" attribute.
 */
addAccessibleTask(
  `data:text/html,<label id="label" for="input">label</label><input id="input">`,
  async function (browser, accDoc) {
    const input = findAccessibleChildByID(accDoc, "input");
    const label = findAccessibleChildByID(accDoc, "label");
    await testCachedRelation(input, RELATION_LABELLED_BY, label);
    await testCachedRelation(label, RELATION_LABEL_FOR, input);
  },
  { iframe: true, remoteIframe: true }
);

/*
 * Test caching of relations with respect to label objects that are ancestors of
 * their target.
 */
addAccessibleTask(
  `
  <label id="host">
    <input type="checkbox" id="dependant1">
  </label>`,
  async function (browser, accDoc) {
    const input = findAccessibleChildByID(accDoc, "dependant1");
    const label = findAccessibleChildByID(accDoc, "host");

    await testCachedRelation(input, RELATION_LABELLED_BY, label);
    await testCachedRelation(label, RELATION_LABEL_FOR, input);
  },
  { iframe: true, remoteIframe: true }
);

/*
 * Test EMBEDS on root accessible.
 */
addAccessibleTask(
  `hello world`,
  async function (browser, primaryDocAcc, secondaryDocAcc) {
    // The root accessible should EMBED the top level
    // content document. If this test runs in an iframe,
    // the test harness will pass in doc accs for both the
    // iframe (primaryDocAcc) and the top level remote
    // browser (secondaryDocAcc). We should use the second
    // one.
    // If this is not in an iframe, we'll only get
    // a single docAcc (primaryDocAcc) which refers to
    // the top level content doc.
    const topLevelDoc = secondaryDocAcc ? secondaryDocAcc : primaryDocAcc;
    await testRelation(
      getRootAccessible(document),
      RELATION_EMBEDS,
      topLevelDoc
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

/**
 * Test CONTAINING_TAB_PANE
 */
addAccessibleTask(
  `<p id="p">hello world</p>`,
  async function (browser, primaryDocAcc, secondaryDocAcc) {
    // The CONTAINING_TAB_PANE of any acc should be the top level
    // content document. If this test runs in an iframe,
    // the test harness will pass in doc accs for both the
    // iframe (primaryDocAcc) and the top level remote
    // browser (secondaryDocAcc). We should use the second
    // one.
    // If this is not in an iframe, we'll only get
    // a single docAcc (primaryDocAcc) which refers to
    // the top level content doc.
    const topLevelDoc = secondaryDocAcc ? secondaryDocAcc : primaryDocAcc;
    await testCachedRelation(
      findAccessibleChildByID(primaryDocAcc, "p"),
      RELATION_CONTAINING_TAB_PANE,
      topLevelDoc
    );
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/*
 * Test relation caching on link
 */
addAccessibleTask(
  `
  <a id="link" href="#item">a</a>
  <div id="item">hello</div>
  <div id="item2">world</div>
  <a id="link2" href="#anchor">b</a>
  <a id="namedLink" name="anchor">c</a>`,
  async function (browser, accDoc) {
    const link = findAccessibleChildByID(accDoc, "link");
    const link2 = findAccessibleChildByID(accDoc, "link2");
    const namedLink = findAccessibleChildByID(accDoc, "namedLink");
    const item = findAccessibleChildByID(accDoc, "item");
    const item2 = findAccessibleChildByID(accDoc, "item2");

    await testCachedRelation(link, RELATION_LINKS_TO, item);
    await testCachedRelation(link2, RELATION_LINKS_TO, namedLink);

    await invokeContentTask(browser, [], () => {
      content.document.getElementById("link").href = "";
      content.document.getElementById("namedLink").name = "newName";
    });

    await testCachedRelation(link, RELATION_LINKS_TO, null);
    await testCachedRelation(link2, RELATION_LINKS_TO, null);

    await invokeContentTask(browser, [], () => {
      content.document.getElementById("link").href = "#item2";
    });

    await testCachedRelation(link, RELATION_LINKS_TO, item2);
  },
  {
    chrome: true,
    // IA2 doesn't have a LINKS_TO relation and Windows non-cached
    // RemoteAccessible uses IA2, so we can't run these tests in this case.
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/*
 * Test relation caching for NODE_CHILD_OF and NODE_PARENT_OF with aria trees.
 */
addAccessibleTask(
  `
  <div role="tree" id="tree">
    <div role="treeitem" id="treeitem">test</div>
    <div role="treeitem" id="treeitem2">test</div>
  </div>`,
  async function (browser, accDoc) {
    const tree = findAccessibleChildByID(accDoc, "tree");
    const treeItem = findAccessibleChildByID(accDoc, "treeitem");
    const treeItem2 = findAccessibleChildByID(accDoc, "treeitem2");

    await testCachedRelation(tree, RELATION_NODE_PARENT_OF, [
      treeItem,
      treeItem2,
    ]);
    await testCachedRelation(treeItem, RELATION_NODE_CHILD_OF, tree);
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

/*
 * Test relation caching for NODE_CHILD_OF and NODE_PARENT_OF with aria lists.
 */
addAccessibleTask(
  `
  <div id="l1" role="list">
    <div id="l1i1" role="listitem" aria-level="1">a</div>
    <div id="l1i2" role="listitem" aria-level="2">b</div>
    <div id="l1i3" role="listitem" aria-level="1">c</div>
  </div>`,
  async function (browser, accDoc) {
    const list = findAccessibleChildByID(accDoc, "l1");
    const listItem1 = findAccessibleChildByID(accDoc, "l1i1");
    const listItem2 = findAccessibleChildByID(accDoc, "l1i2");
    const listItem3 = findAccessibleChildByID(accDoc, "l1i3");

    await testCachedRelation(list, RELATION_NODE_PARENT_OF, [
      listItem1,
      listItem3,
    ]);
    await testCachedRelation(listItem1, RELATION_NODE_CHILD_OF, list);
    await testCachedRelation(listItem3, RELATION_NODE_CHILD_OF, list);

    await testCachedRelation(listItem1, RELATION_NODE_PARENT_OF, listItem2);
    await testCachedRelation(listItem2, RELATION_NODE_CHILD_OF, listItem1);
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

/*
 * Test NODE_CHILD_OF relation caching for JAWS window emulation special case.
 */
addAccessibleTask(
  ``,
  async function (browser, accDoc) {
    await testCachedRelation(accDoc, RELATION_NODE_CHILD_OF, accDoc.parent);
  },
  { topLevel: true, chrome: true }
);
