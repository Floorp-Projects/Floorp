/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported HeadersTraversalRule, ObjectTraversalRule, testPivotSequence, testFailsWithNotInTree */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

/* import-globals-from ../../mochitest/layout.js */
/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR },
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "layout.js", dir: MOCHITESTS_DIR }
);

const FILTER_MATCH = nsIAccessibleTraversalRule.FILTER_MATCH;
const FILTER_IGNORE = nsIAccessibleTraversalRule.FILTER_IGNORE;
const FILTER_IGNORE_SUBTREE = nsIAccessibleTraversalRule.FILTER_IGNORE_SUBTREE;

const NS_ERROR_NOT_IN_TREE = 0x80780026;

// //////////////////////////////////////////////////////////////////////////////
// Traversal rules

/**
 * Rule object to traverse all focusable nodes and text nodes.
 */
const HeadersTraversalRule = {
  match(acc) {
    return acc.role == ROLE_HEADING ? FILTER_MATCH : FILTER_IGNORE;
  },

  QueryInterface: ChromeUtils.generateQI([nsIAccessibleTraversalRule]),
};

/**
 * Traversal rule for all focusable nodes or leafs.
 */
const ObjectTraversalRule = {
  match(acc) {
    let [state, extstate] = getStates(acc);
    if (state & STATE_INVISIBLE) {
      return FILTER_IGNORE;
    }

    if ((extstate & EXT_STATE_OPAQUE) == 0) {
      return FILTER_IGNORE | FILTER_IGNORE_SUBTREE;
    }

    let rv = FILTER_IGNORE;
    let role = acc.role;
    if (
      hasState(acc, STATE_FOCUSABLE) &&
      role != ROLE_DOCUMENT &&
      role != ROLE_INTERNAL_FRAME
    ) {
      rv = FILTER_IGNORE_SUBTREE | FILTER_MATCH;
    } else if (
      acc.childCount == 0 &&
      role != ROLE_LISTITEM_MARKER &&
      acc.name.trim()
    ) {
      rv = FILTER_MATCH;
    }

    return rv;
  },

  QueryInterface: ChromeUtils.generateQI([nsIAccessibleTraversalRule]),
};

function getIdOrName(acc) {
  let id = getAccessibleDOMNodeID(acc);
  if (id) {
    return id;
  }
  return acc.name;
}

function* pivotNextGenerator(pivot, rule) {
  for (let acc = pivot.first(rule); acc; acc = pivot.next(acc, rule)) {
    yield acc;
  }
}

function* pivotPreviousGenerator(pivot, rule) {
  for (let acc = pivot.last(rule); acc; acc = pivot.prev(acc, rule)) {
    yield acc;
  }
}

function testPivotSequence(pivot, rule, expectedSequence) {
  is(
    JSON.stringify([...pivotNextGenerator(pivot, rule)].map(getIdOrName)),
    JSON.stringify(expectedSequence),
    "Forward pivot sequence is correct"
  );
  is(
    JSON.stringify([...pivotPreviousGenerator(pivot, rule)].map(getIdOrName)),
    JSON.stringify([...expectedSequence].reverse()),
    "Reverse pivot sequence is correct"
  );
}

function testFailsWithNotInTree(func, msg) {
  try {
    func();
    ok(false, msg);
  } catch (x) {
    is(x.result, NS_ERROR_NOT_IN_TREE, `Expecting NOT_IN_TREE: ${msg}`);
  }
}
