/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR });

addAccessibleTask('doc_treeupdate_removal.xhtml', function*(browser, accDoc) {
  ok(isAccessible(findAccessibleChildByID(accDoc, 'the_table')),
    'table should be accessible');

  // Move the_table element into hidden subtree.
  let onReorder = waitForEvent(EVENT_REORDER, 'body');
  yield ContentTask.spawn(browser, {}, () => content.document.getElementById(
    'the_displaynone').appendChild(content.document.getElementById(
      'the_table')));
  yield onReorder;

  ok(!isAccessible(findAccessibleChildByID(accDoc, 'the_table')),
    'table in display none tree shouldn\'t be accessible');
  ok(!isAccessible(findAccessibleChildByID(accDoc, 'the_row')),
    'row shouldn\'t be accessible');

  // Remove the_row element (since it did not have accessible, no event needed).
  yield ContentTask.spawn(browser, {}, () =>
    content.document.body.removeChild(
      content.document.getElementById('the_row')));

  // make sure no accessibles have stuck around.
  ok(!isAccessible(findAccessibleChildByID(accDoc, 'the_row')),
    'row shouldn\'t be accessible');
  ok(!isAccessible(findAccessibleChildByID(accDoc, 'the_table')),
    'table shouldn\'t be accessible');
  ok(!isAccessible(findAccessibleChildByID(accDoc, 'the_displayNone')),
    'display none things shouldn\'t be accessible');
});
