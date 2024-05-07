/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported testCachedRelation, testRelated */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js and relations.js.
/* import-globals-from ../../mochitest/relations.js */
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR },
  { name: "relations.js", dir: MOCHITESTS_DIR }
);

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
    await untilCacheOk(function () {
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
  await untilCacheOk(function () {
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

  await untilCacheOk(function () {
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

  await untilCacheOk(function () {
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
      desc: "Change attribute to multiple targets",
      attrs: [{ key: attr, value: "dependant1 dependant2" }],
      expected: [host, host, [dependant1, dependant2]],
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
