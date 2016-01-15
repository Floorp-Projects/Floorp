/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that attribute items work in the context menu

const TEST_URL = URL_ROOT + "doc_inspector_menu.html";

add_task(function* () {
  let { inspector, toolbox, testActor } = yield openInspectorForURL(TEST_URL);
  yield selectNode("#attributes", inspector);

  yield testAddAttribute();
  yield testEditAttribute();
  yield testRemoveAttribute();

  function* testAddAttribute() {
    info("Testing 'Add Attribute' menu item");
    let addAttribute = getMenuItem("node-menu-add-attribute");

    info("Triggering 'Add Attribute' and waiting for mutation to occur");
    dispatchCommandEvent(addAttribute);
    EventUtils.synthesizeKey('class="u-hidden"', {});
    let onMutation = inspector.once("markupmutation");
    EventUtils.synthesizeKey('VK_RETURN', {});
    yield onMutation;

    let hasAttribute = testActor.hasNode("#attributes.u-hidden");
    ok(hasAttribute, "attribute was successfully added");
  }

  function* testEditAttribute() {
    info("Testing 'Edit Attribute' menu item");
    let editAttribute = getMenuItem("node-menu-edit-attribute");

    info("Triggering 'Edit Attribute' and waiting for mutation to occur");
    inspector.nodeMenuTriggerInfo = {
      type: "attribute",
      name: "data-edit"
    };
    dispatchCommandEvent(editAttribute);
    EventUtils.synthesizeKey("data-edit='edited'", {});
    let onMutation = inspector.once("markupmutation");
    EventUtils.synthesizeKey('VK_RETURN', {});
    yield onMutation;

    let isAttributeChanged =
      yield testActor.hasNode("#attributes[data-edit='edited']");
    ok(isAttributeChanged, "attribute was successfully edited");
  }

  function* testRemoveAttribute() {
    info("Testing 'Remove Attribute' menu item");
    let removeAttribute = getMenuItem("node-menu-remove-attribute");

    info("Triggering 'Remove Attribute' and waiting for mutation to occur");
    inspector.nodeMenuTriggerInfo = {
      type: "attribute",
      name: "data-remove"
    };
    let onMutation = inspector.once("markupmutation");
    dispatchCommandEvent(removeAttribute);
    yield onMutation;

    let hasAttribute = yield testActor.hasNode("#attributes[data-remove]")
    ok(!hasAttribute, "attribute was successfully removed");
  }

  function getMenuItem(id) {
    let attribute = inspector.panelDoc.getElementById(id);
    ok(attribute, "Menu item '" + id + "' found");
    return attribute;
  }
});
