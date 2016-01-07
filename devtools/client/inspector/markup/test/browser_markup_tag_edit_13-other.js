/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that doesn't fit into any specific category.

const TEST_URL = "data:text/html;charset=utf8,<div a b id='order' c class></div>";

add_task(function*() {
  let {inspector, testActor} = yield addTab(TEST_URL).then(openInspector);

  yield testOriginalAttributesOrder(inspector);
  yield testOrderAfterAttributeChange(inspector, testActor);
});

function* testOriginalAttributesOrder(inspector) {
  info("Testing order of attributes on initial node render");

  let attributes = yield getAttributesFromEditor("#order", inspector);
  ok(isEqual(attributes, ["id", "class", "a", "b", "c"]), "ordered correctly");
}

function* testOrderAfterAttributeChange(inspector, testActor) {
  info("Testing order of attributes after attribute is change by setAttribute");

  let container = getContainerForSelector("#order", inspector);
  yield testActor.setAttribute("#order", "a", "changed");

  let attributes = yield getAttributesFromEditor("#order", inspector);
  ok(isEqual(attributes, ["id", "class", "a", "b", "c"]),
    "order isn't changed");
}

function isEqual(a, b) {
  return a.toString() === b.toString();
}
