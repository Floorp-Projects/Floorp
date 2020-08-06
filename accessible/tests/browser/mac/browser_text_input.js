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

// AXTextStateChangeType enum values
const AXTextStateChangeTypeEdit = 1;

// AXTextEditType enum values
const AXTextEditTypeDelete = 1;
const AXTextEditTypeTyping = 3;

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
    "AXStringForTextMarkerRange_",
    range,
    "correct word before caret"
  );
  is(str, expectedWordAtLeft);
}

async function synthKeyAndTestEvent(
  synthKey,
  synthEvent,
  expectedId,
  expectedChangeValue,
  expectedEditType,
  expectedWordAtLeft
) {
  let valueChangedEvents = Promise.all([
    waitForMacEventWithInfo("AXValueChanged", (iface, data) => {
      return (
        iface.getAttributeValue("AXRole") == "AXWebArea" &&
        !!data &&
        data.AXTextChangeElement.getAttributeValue("AXDOMIdentifier") == "input"
      );
    }),
    waitForMacEventWithInfo(
      "AXValueChanged",
      (iface, data) =>
        iface.getAttributeValue("AXDOMIdentifier") == "input" && !!data
    ),
  ]);

  EventUtils.synthesizeKey(synthKey, synthEvent);
  let [webareaEvent, inputEvent] = await valueChangedEvents;

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

// Test text input
addAccessibleTask(
  `<a href="#">link</a> <input id="input">`,
  async (browser, accDoc) => {
    let input = getNativeInterface(accDoc, "input");
    ok(!input.getAttributeValue("AXFocused"), "input is not focused");
    ok(input.isAttributeSettable("AXFocused"), "input is focusable");
    let focusChanged = waitForMacEvent(
      "AXFocusedUIElementChanged",
      iface => iface.getAttributeValue("AXDOMIdentifier") == "input"
    );
    input.setAttributeValue("AXFocused", true);
    await focusChanged;

    async function testTextInput(
      synthKey,
      expectedChangeValue,
      expectedWordAtLeft
    ) {
      await synthKeyAndTestEvent(
        synthKey,
        null,
        "input",
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
      await synthKeyAndTestEvent(
        "KEY_Backspace",
        null,
        "input",
        expectedChangeValue,
        AXTextEditTypeDelete,
        expectedWordAtLeft
      );
    }

    await testTextDelete("d", "worl");
    await testTextDelete("l", "wor");
  }
);
