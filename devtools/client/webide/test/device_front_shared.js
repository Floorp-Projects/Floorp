/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var customName;
var customValue;
var customValueType;
var customBtn;
var newField;
var change;
var doc;
var iframe;
var resetBtn;
var found = false;

function setDocument(frame) {
  iframe = frame;
  doc = iframe.contentWindow.document;
}

function fieldChange(fields, id) {
  // Trigger existing field change
  for (let field of fields) {
    if (field.id == id) {
      let button = doc.getElementById("btn-" + id);
      found = true;
      ok(button.classList.contains("hide"), "Default field detected");
      field.value = "custom";
      field.click();
      ok(!button.classList.contains("hide"), "Custom field detected");
      break;
    }
  }
  ok(found, "Found " + id + " line");
}

function addNewField() {
  found = false;
  customName = doc.querySelector("#custom-value-name");
  customValue = doc.querySelector("#custom-value-text");
  customValueType = doc.querySelector("#custom-value-type");
  customBtn = doc.querySelector("#custom-value");
  change = doc.createEvent("HTMLEvents");
  change.initEvent("change", false, true);

  // Add a new custom string
  customValueType.value = "string";
  customValueType.dispatchEvent(change);
  customName.value = "new-string-field!";
  customValue.value = "test";
  customBtn.click();
  let newField = doc.querySelector("#new-string-field");
  if (newField) {
    found = true;
    is(newField.type, "text", "Custom type is a string");
    is(newField.value, "test", "Custom string new value is correct");
  }
  ok(found, "Found new string field line");
  is(customName.value, "", "Custom string name reset");
  is(customValue.value, "", "Custom string value reset");
}

function addNewFieldWithEnter() {
  // Add a new custom value with the <enter> key
  found = false;
  customName.value = "new-string-field-two";
  customValue.value = "test";
  let newAddField = doc.querySelector("#add-custom-field");
  let enter = doc.createEvent("KeyboardEvent");
  enter.initKeyEvent(
    "keyup", true, true, null, false, false, false, false, 13, 0);
  newAddField.dispatchEvent(enter);
  newField = doc.querySelector("#new-string-field-two");
  if (newField) {
    found = true;
    is(newField.type, "text", "Custom type is a string");
    is(newField.value, "test", "Custom string new value is correct");
  }
  ok(found, "Found new string field line");
  is(customName.value, "", "Custom string name reset");
  is(customValue.value, "", "Custom string value reset");
}

function editExistingField() {
  // Edit existing custom string preference
  newField.value = "test2";
  newField.click();
  is(newField.value, "test2", "Custom string existing value is correct");
}

function addNewFieldInteger() {
  // Add a new custom integer preference with a valid integer
  customValueType.value = "number";
  customValueType.dispatchEvent(change);
  customName.value = "new-integer-field";
  customValue.value = 1;
  found = false;

  customBtn.click();
  newField = doc.querySelector("#new-integer-field");
  if (newField) {
    found = true;
    is(newField.type, "number", "Custom type is a number");
    is(newField.value, "1", "Custom integer value is correct");
  }
  ok(found, "Found new integer field line");
  is(customName.value, "", "Custom integer name reset");
  is(customValue.value, "", "Custom integer value reset");
}

var editFieldInteger = Task.async(function* () {
  // Edit existing custom integer preference
  newField.value = 3;
  newField.click();
  is(newField.value, "3", "Custom integer existing value is correct");

  // Reset a custom field
  let resetBtn = doc.querySelector("#btn-new-integer-field");
  resetBtn.click();

  try {
    yield iframe.contentWindow.configView._defaultField;
  } catch (err) {
    let fieldRow = doc.querySelector("#row-new-integer-field");
    if (!fieldRow) {
      found = false;
    }
    ok(!found, "Custom field removed");
  }
});

var resetExistingField = Task.async(function* (id) {
  let existing = doc.getElementById(id);
  existing.click();
  is(existing.checked, true, "Existing boolean value is correct");
  resetBtn = doc.getElementById("btn-" + id);
  resetBtn.click();

  yield iframe.contentWindow.configView._defaultField;

  ok(resetBtn.classList.contains("hide"), true, "Reset button hidden");
  is(existing.checked, true, "Existing field reset");
});

var resetNewField = Task.async(function* (id) {
  let custom = doc.getElementById(id);
  custom.click();
  is(custom.value, "test", "New string value is correct");
  resetBtn = doc.getElementById("btn-" + id);
  resetBtn.click();

  yield iframe.contentWindow.configView._defaultField;

  ok(resetBtn.classList.contains("hide"), true, "Reset button hidden");
});

function addNewFieldBoolean() {
  customValueType.value = "boolean";
  customValueType.dispatchEvent(change);
  customName.value = "new-boolean-field";
  customValue.checked = true;
  found = false;
  customBtn.click();
  newField = doc.querySelector("#new-boolean-field");
  if (newField) {
    found = true;
    is(newField.type, "checkbox", "Custom type is a checkbox");
    is(newField.checked, true, "Custom boolean value is correctly true");
  }
  ok(found, "Found new boolean field line");

  // Mouse event trigger
  var mouseClick = new MouseEvent("click", {
    canBubble: true,
    cancelable: true,
    view: doc.parent,
  });

  found = false;
  customValueType.value = "boolean";
  customValueType.dispatchEvent(change);
  customName.value = "new-boolean-field2";
  customValue.dispatchEvent(mouseClick);
  customBtn.dispatchEvent(mouseClick);
  newField = doc.querySelector("#new-boolean-field2");
  if (newField) {
    found = true;
    is(newField.checked, true, "Custom boolean value is correctly false");
  }
  ok(found, "Found new second boolean field line");

  is(customName.value, "", "Custom boolean name reset");
  is(customValue.checked, false, "Custom boolean value reset");

  newField.click();
  is(newField.checked, false, "Custom boolean existing value is correct");
}

function searchFields(deck, keyword) {
  // Search for a non-existent field
  let searchField = doc.querySelector("#search-bar");
  searchField.value = "![o_O]!";
  searchField.click();

  let fieldsTotal = doc.querySelectorAll("tr.edit-row").length;
  let hiddenFields = doc.querySelectorAll("tr.hide");
  is(hiddenFields.length, fieldsTotal, "Search keyword not found");

  // Search for existing fields
  searchField.value = keyword;
  searchField.click();
  hiddenFields = doc.querySelectorAll("tr.hide");
  isnot(hiddenFields.length, fieldsTotal, "Search keyword found");

  doc.querySelector("#close").click();

  ok(!deck.selectedPanel, "No panel selected");
}
