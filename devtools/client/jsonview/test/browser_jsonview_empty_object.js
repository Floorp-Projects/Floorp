/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function testRootObject(objExpr, summary = objExpr) {
  return async function() {
    info("Test JSON with root empty object " + objExpr + " started");

    const TEST_JSON_URL = "data:application/json," + objExpr;
    await addJsonViewTab(TEST_JSON_URL);

    const objectText = await getElementText(
      ".jsonPanelBox .panelContent");
    is(objectText, summary, "The root object " + objExpr + " is visible");
  };
}

function testNestedObject(objExpr, summary = objExpr) {
  return async function() {
    info("Test JSON with nested empty object " + objExpr + " started");

    const TEST_JSON_URL = "data:application/json,[" + objExpr + "]";
    await addJsonViewTab(TEST_JSON_URL);

    const objectCellCount = await getElementCount(
      ".jsonPanelBox .treeTable .objectCell");
    is(objectCellCount, 1, "There must be one object cell");

    const objectCellText = await getElementText(
      ".jsonPanelBox .treeTable .objectCell");
    is(objectCellText, summary, objExpr + " has a visible summary");

    // Collapse auto-expanded node.
    await clickJsonNode(".jsonPanelBox .treeTable .treeLabel");

    const textAfter = await getElementText(
      ".jsonPanelBox .treeTable .objectCell");
    is(textAfter, summary, objExpr + " still has a visible summary");
  };
}

add_task(testRootObject("null"));
add_task(testNestedObject("null"));
add_task(testNestedObject("[]"));
add_task(testNestedObject("{}"));
