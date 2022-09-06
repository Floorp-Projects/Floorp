/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/relations.js */
loadScripts({ name: "relations.js", dir: MOCHITESTS_DIR });

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
];

/**
 * Test the accessible relation.
 *
 * @param identifier          [in] identifier to get an accessible, may be ID
 *                             attribute or DOM element or accessible object
 * @param relType             [in] relation type (see constants above)
 * @param relatedIdentifiers  [in] identifier or array of identifiers of
 *                             expected related accessibles
 */
async function testCachedRelation(identifier, relType, relatedIdentifiers) {
  const relDescr = getRelationErrorMsg(identifier, relType);
  const relDescrStart = getRelationErrorMsg(identifier, relType, true);
  info(`Testing ${relDescr}`);

  if (!relatedIdentifiers) {
    await untilCacheOk(function() {
      let r = getRelationByType(identifier, relType);
      if (r) {
        info(`Fetched ${r.targetsCount} relations from cache`);
      } else {
        info("Could not fetch relations");
      }
      return r && !r.targetsCount;
    }, relDescrStart + " has no targets, as expected");
    return;
  }

  const relatedIds =
    relatedIdentifiers instanceof Array
      ? relatedIdentifiers
      : [relatedIdentifiers];
  await untilCacheOk(function() {
    let r = getRelationByType(identifier, relType);
    if (r) {
      info(
        `Fetched ${r.targetsCount} relations from cache, looking for ${relatedIds.length}`
      );
    } else {
      info("Could not fetch relations");
    }

    return r && r.targetsCount == relatedIds.length;
  }, "Found correct number of expected relations");

  let targets = [];
  for (let idx = 0; idx < relatedIds.length; idx++) {
    targets.push(getAccessible(relatedIds[idx]));
  }

  if (targets.length != relatedIds.length) {
    return;
  }

  await untilCacheOk(function() {
    const relation = getRelationByType(identifier, relType);
    const actualTargets = relation ? relation.getTargets() : null;
    if (!actualTargets) {
      info("Could not fetch relations");
      return false;
    }

    // Check if all given related accessibles are targets of obtained relation.
    for (let idx = 0; idx < targets.length; idx++) {
      let isFound = false;
      for (let relatedAcc of actualTargets.enumerate(Ci.nsIAccessible)) {
        if (targets[idx] == relatedAcc) {
          isFound = true;
          break;
        }
      }

      if (!isFound) {
        info(
          prettyName(relatedIds[idx]) +
            " could not be found in relation: " +
            relDescr
        );
        return false;
      }
    }

    return true;
  }, "All given related accessibles are targets of fetched relation.");

  await untilCacheOk(function() {
    const relation = getRelationByType(identifier, relType);
    const actualTargets = relation ? relation.getTargets() : null;
    if (!actualTargets) {
      info("Could not fetch relations");
      return false;
    }

    // Check if all obtained targets are given related accessibles.
    for (let relatedAcc of actualTargets.enumerate(Ci.nsIAccessible)) {
      let wasFound = false;
      for (let idx = 0; idx < targets.length; idx++) {
        if (relatedAcc == targets[idx]) {
          wasFound = true;
        }
      }
      if (!wasFound) {
        info(
          prettyName(relatedAcc) +
            " was found, but shouldn't be in relation: " +
            relDescr
        );
        return false;
      }
    }
    return true;
  }, "No unexpected targets found.");
}

async function testRelated(
  browser,
  accDoc,
  attr,
  hostRelation,
  dependantRelation
) {
  let host = findAccessibleChildByID(accDoc, "host");
  let dependant1 = findAccessibleChildByID(accDoc, "dependant1");
  let dependant2 = findAccessibleChildByID(accDoc, "dependant2");

  /**
   * Test data has the format of:
   * {
   *   desc      {String}   description for better logging
   *   attrs     {?Array}   an optional list of attributes to update
   *   expected  {Array}    expected relation values for dependant1, dependant2
   *                        and host respectively.
   * }
   */
  const tests = [
    {
      desc: "No attribute",
      expected: [null, null, null],
    },
    {
      desc: "Set attribute",
      attrs: [{ key: attr, value: "dependant1" }],
      expected: [host, null, dependant1],
    },
    {
      desc: "Change attribute",
      attrs: [{ key: attr, value: "dependant2" }],
      expected: [null, host, dependant2],
    },
    {
      desc: "Remove attribute",
      attrs: [{ key: attr }],
      expected: [null, null, null],
    },
  ];

  for (let { desc, attrs, expected } of tests) {
    info(desc);

    if (attrs) {
      for (let { key, value } of attrs) {
        await invokeSetAttribute(browser, "host", key, value);
      }
    }

    await testCachedRelation(dependant1, dependantRelation, expected[0]);
    await testCachedRelation(dependant2, dependantRelation, expected[1]);
    await testCachedRelation(host, hostRelation, expected[2]);
  }
}

/**
 * Test caching of relations between accessible objects.
 */
addAccessibleTask(
  `
  <div id="dependant1">label</div>
  <div id="dependant2">label2</div>
  <div role="checkbox" id="host"></div>`,
  async function(browser, accDoc) {
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
  async function(browser, accDoc) {
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
  async function(browser, accDoc) {
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
  async function(browser, accDoc) {
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
  async function(browser, accDoc) {
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
  async function(browser, accDoc) {
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
  async function(browser, primaryDocAcc, secondaryDocAcc) {
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
  async function(browser, primaryDocAcc, secondaryDocAcc) {
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
    topLevel: isCacheEnabled,
    iframe: isCacheEnabled,
    remoteIframe: isCacheEnabled,
  }
);

/*
 * Test relation caching on link
 */
addAccessibleTask(
  `
  <a id="link" href="#item">
  <div id="item">hello</div>
  <div id="item2">world</div>`,
  async function(browser, accDoc) {
    const link = findAccessibleChildByID(accDoc, "link");
    const item = findAccessibleChildByID(accDoc, "item");
    const item2 = findAccessibleChildByID(accDoc, "item2");

    await testCachedRelation(link, RELATION_LINKS_TO, item);

    await invokeContentTask(browser, [], () => {
      content.document.getElementById("link").href = "";
    });

    await testCachedRelation(link, RELATION_LINKS_TO, null);

    await invokeContentTask(browser, [], () => {
      content.document.getElementById("link").href = "#item2";
    });

    await testCachedRelation(link, RELATION_LINKS_TO, item2);
  },
  { chrome: true, iframe: true, remoteIframe: true }
);
