/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

function testValueChangedEventData(
  macIface,
  data,
  expectedId,
  expectedChangeValue,
  expectedEditType,
  expectedWordAtLeft
) {
  is(
    data.AXTextChangeElement.getAttributeValue("AXDOMIdentifier"),
    expectedId,
    "Correct AXTextChangeElement"
  );
  is(
    data.AXTextStateChangeType,
    AXTextStateChangeTypeEdit,
    "Correct AXTextStateChangeType"
  );

  let changeValues = data.AXTextChangeValues;
  is(changeValues.length, 1, "One element in AXTextChangeValues");
  is(
    changeValues[0].AXTextChangeValue,
    expectedChangeValue,
    "Correct AXTextChangeValue"
  );
  is(
    changeValues[0].AXTextEditType,
    expectedEditType,
    "Correct AXTextEditType"
  );

  let textMarker = changeValues[0].AXTextChangeValueStartMarker;
  ok(textMarker, "There is a AXTextChangeValueStartMarker");
  let range = macIface.getParameterizedAttributeValue(
    "AXLeftWordTextMarkerRangeForTextMarker",
    textMarker
  );
  let str = macIface.getParameterizedAttributeValue(
    "AXStringForTextMarkerRange",
    range,
    "correct word before caret"
  );
  is(str, expectedWordAtLeft);
}

// Return true if the first given object a subset of the second
function isSubset(subset, superset) {
  if (typeof subset != "object" || typeof superset != "object") {
    return superset == subset;
  }

  for (let [prop, val] of Object.entries(subset)) {
    if (!isSubset(val, superset[prop])) {
      return false;
    }
  }

  return true;
}

function matchWebArea(expectedId, expectedInfo) {
  return (iface, data) => {
    if (!data) {
      return false;
    }

    let textChangeElemID = data.AXTextChangeElement.getAttributeValue(
      "AXDOMIdentifier"
    );

    return (
      iface.getAttributeValue("AXRole") == "AXWebArea" &&
      textChangeElemID == expectedId &&
      isSubset(expectedInfo, data)
    );
  };
}

function matchInput(expectedId, expectedInfo) {
  return (iface, data) => {
    if (!data) {
      return false;
    }

    return (
      iface.getAttributeValue("AXDOMIdentifier") == expectedId &&
      isSubset(expectedInfo, data)
    );
  };
}

async function synthKeyAndTestSelectionChanged(
  synthKey,
  synthEvent,
  expectedId,
  expectedSelectionString,
  expectedSelectionInfo
) {
  let selectionChangedEvents = Promise.all([
    waitForMacEventWithInfo(
      "AXSelectedTextChanged",
      matchWebArea(expectedId, expectedSelectionInfo)
    ),
    waitForMacEventWithInfo(
      "AXSelectedTextChanged",
      matchInput(expectedId, expectedSelectionInfo)
    ),
  ]);

  EventUtils.synthesizeKey(synthKey, synthEvent);
  let [webareaEvent, inputEvent] = await selectionChangedEvents;
  is(
    inputEvent.data.AXTextChangeElement.getAttributeValue("AXDOMIdentifier"),
    expectedId,
    "Correct AXTextChangeElement"
  );

  let rangeString = inputEvent.macIface.getParameterizedAttributeValue(
    "AXStringForTextMarkerRange",
    inputEvent.data.AXSelectedTextMarkerRange
  );
  is(
    rangeString,
    expectedSelectionString,
    `selection has correct value (${expectedSelectionString})`
  );

  is(
    webareaEvent.macIface.getAttributeValue("AXDOMIdentifier"),
    "body",
    "Input event target is top-level WebArea"
  );
  rangeString = webareaEvent.macIface.getParameterizedAttributeValue(
    "AXStringForTextMarkerRange",
    inputEvent.data.AXSelectedTextMarkerRange
  );
  is(
    rangeString,
    expectedSelectionString,
    `selection has correct value (${expectedSelectionString}) via top document`
  );
}

async function synthKeyAndTestValueChanged(
  synthKey,
  synthEvent,
  expectedId,
  expectedTextSelectionId,
  expectedChangeValue,
  expectedEditType,
  expectedWordAtLeft
) {
  let valueChangedEvents = Promise.all([
    waitForMacEvent(
      "AXSelectedTextChanged",
      matchWebArea(expectedTextSelectionId, {
        AXTextStateChangeType: AXTextStateChangeTypeSelectionMove,
      })
    ),
    waitForMacEvent(
      "AXSelectedTextChanged",
      matchInput(expectedTextSelectionId, {
        AXTextStateChangeType: AXTextStateChangeTypeSelectionMove,
      })
    ),
    waitForMacEventWithInfo(
      "AXValueChanged",
      matchWebArea(expectedId, {
        AXTextStateChangeType: AXTextStateChangeTypeEdit,
        AXTextChangeValues: [
          {
            AXTextChangeValue: expectedChangeValue,
            AXTextEditType: expectedEditType,
          },
        ],
      })
    ),
    waitForMacEventWithInfo(
      "AXValueChanged",
      matchInput(expectedId, {
        AXTextStateChangeType: AXTextStateChangeTypeEdit,
        AXTextChangeValues: [
          {
            AXTextChangeValue: expectedChangeValue,
            AXTextEditType: expectedEditType,
          },
        ],
      })
    ),
  ]);

  EventUtils.synthesizeKey(synthKey, synthEvent);
  let [, , webareaEvent, inputEvent] = await valueChangedEvents;

  testValueChangedEventData(
    webareaEvent.macIface,
    webareaEvent.data,
    expectedId,
    expectedChangeValue,
    expectedEditType,
    expectedWordAtLeft
  );
  testValueChangedEventData(
    inputEvent.macIface,
    inputEvent.data,
    expectedId,
    expectedChangeValue,
    expectedEditType,
    expectedWordAtLeft
  );
}

async function focusIntoInputAndType(accDoc, inputId, innerContainerId) {
  let selectionId = innerContainerId ? innerContainerId : inputId;
  let input = getNativeInterface(accDoc, inputId);
  ok(!input.getAttributeValue("AXFocused"), "input is not focused");
  ok(input.isAttributeSettable("AXFocused"), "input is focusable");
  let events = Promise.all([
    waitForMacEvent(
      "AXFocusedUIElementChanged",
      iface => iface.getAttributeValue("AXDOMIdentifier") == inputId
    ),
    waitForMacEventWithInfo(
      "AXSelectedTextChanged",
      matchWebArea(selectionId, {
        AXTextStateChangeType: AXTextStateChangeTypeSelectionMove,
      })
    ),
    waitForMacEventWithInfo(
      "AXSelectedTextChanged",
      matchInput(selectionId, {
        AXTextStateChangeType: AXTextStateChangeTypeSelectionMove,
      })
    ),
  ]);
  input.setAttributeValue("AXFocused", true);
  await events;

  async function testTextInput(
    synthKey,
    expectedChangeValue,
    expectedWordAtLeft
  ) {
    await synthKeyAndTestValueChanged(
      synthKey,
      null,
      inputId,
      selectionId,
      expectedChangeValue,
      AXTextEditTypeTyping,
      expectedWordAtLeft
    );
  }

  await testTextInput("h", "h", "h");
  await testTextInput("e", "e", "he");
  await testTextInput("l", "l", "hel");
  await testTextInput("l", "l", "hell");
  await testTextInput("o", "o", "hello");
  await testTextInput(" ", " ", "hello");
  // You would expect this to be useless but this is what VO
  // consumes. I guess it concats the inserted text data to the
  // word to the left of the marker.
  await testTextInput("w", "w", " ");
  await testTextInput("o", "o", "wo");
  await testTextInput("r", "r", "wor");
  await testTextInput("l", "l", "worl");
  await testTextInput("d", "d", "world");

  async function testTextDelete(expectedChangeValue, expectedWordAtLeft) {
    await synthKeyAndTestValueChanged(
      "KEY_Backspace",
      null,
      inputId,
      selectionId,
      expectedChangeValue,
      AXTextEditTypeDelete,
      expectedWordAtLeft
    );
  }

  await testTextDelete("d", "worl");
  await testTextDelete("l", "wor");

  await synthKeyAndTestSelectionChanged(
    "KEY_ArrowLeft",
    null,
    selectionId,
    "",
    {
      AXTextStateChangeType: AXTextStateChangeTypeSelectionMove,
      AXTextSelectionDirection: AXTextSelectionDirectionPrevious,
      AXTextSelectionGranularity: AXTextSelectionGranularityCharacter,
    }
  );
  await synthKeyAndTestSelectionChanged(
    "KEY_ArrowLeft",
    { shiftKey: true },
    selectionId,
    "o",
    {
      AXTextStateChangeType: AXTextStateChangeTypeSelectionExtend,
      AXTextSelectionDirection: AXTextSelectionDirectionPrevious,
      AXTextSelectionGranularity: AXTextSelectionGranularityCharacter,
    }
  );
  await synthKeyAndTestSelectionChanged(
    "KEY_ArrowLeft",
    { shiftKey: true },
    selectionId,
    "wo",
    {
      AXTextStateChangeType: AXTextStateChangeTypeSelectionExtend,
      AXTextSelectionDirection: AXTextSelectionDirectionPrevious,
      AXTextSelectionGranularity: AXTextSelectionGranularityCharacter,
    }
  );
  await synthKeyAndTestSelectionChanged(
    "KEY_ArrowLeft",
    null,
    selectionId,
    "",
    { AXTextStateChangeType: AXTextStateChangeTypeSelectionMove }
  );
  await synthKeyAndTestSelectionChanged(
    "KEY_Home",
    { shiftKey: true },
    selectionId,
    "hello ",
    {
      AXTextStateChangeType: AXTextStateChangeTypeSelectionExtend,
      AXTextSelectionDirection: AXTextSelectionDirectionPrevious,
      AXTextSelectionGranularity: AXTextSelectionGranularityWord,
    }
  );
  await synthKeyAndTestSelectionChanged(
    "KEY_ArrowLeft",
    null,
    selectionId,
    "",
    { AXTextStateChangeType: AXTextStateChangeTypeSelectionMove }
  );
  await synthKeyAndTestSelectionChanged(
    "KEY_ArrowRight",
    { shiftKey: true, altKey: true },
    selectionId,
    "hello",
    {
      AXTextStateChangeType: AXTextStateChangeTypeSelectionExtend,
      AXTextSelectionDirection: AXTextSelectionDirectionNext,
      AXTextSelectionGranularity: AXTextSelectionGranularityWord,
    }
  );
}

// Test text input
addAccessibleTask(
  `<a href="#">link</a> <input id="input">`,
  async (browser, accDoc) => {
    await focusIntoInputAndType(accDoc, "input");
  }
);

// Test content editable
addAccessibleTask(
  `<div id="input" contentEditable="true" tabindex="0" role="textbox" aria-multiline="true"><div id="inner"><br /></div></div>`,
  async (browser, accDoc) => {
    const inner = getNativeInterface(accDoc, "inner");
    const editableAncestor = inner.getAttributeValue("AXEditableAncestor");
    is(
      editableAncestor.getAttributeValue("AXDOMIdentifier"),
      "input",
      "Editable ancestor is input"
    );
    await focusIntoInputAndType(accDoc, "input");
  }
);

// Test text input in iframe
addAccessibleTask(
  `<a href="#">link</a> <input id="input">`,
  async (browser, accDoc) => {
    await focusIntoInputAndType(accDoc, "input");
  },
  { iframe: true }
);

// Test input that gets role::EDITCOMBOBOX
addAccessibleTask(`<input type="text" id="box">`, async (browser, accDoc) => {
  const box = getNativeInterface(accDoc, "box");
  const editableAncestor = box.getAttributeValue("AXEditableAncestor");
  is(
    editableAncestor.getAttributeValue("AXDOMIdentifier"),
    "box",
    "Editable ancestor is box itself"
  );
  await focusIntoInputAndType(accDoc, "box");
});
