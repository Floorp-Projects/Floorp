/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function selectedTextEventPromises(stateChangeType) {
  return [
    waitForMacEventWithInfo("AXSelectedTextChanged", (elem, info) => {
      return (
        info.AXTextStateChangeType == stateChangeType &&
        elem.getAttributeValue("AXDOMIdentifier") == "body"
      );
    }),
    waitForMacEventWithInfo("AXSelectedTextChanged", (elem, info) => {
      return (
        info.AXTextStateChangeType == stateChangeType &&
        elem.getAttributeValue("AXDOMIdentifier") == "input"
      );
    }),
  ];
}

async function testInput(browser, accDoc) {
  let input = getNativeInterface(accDoc, "input");

  is(input.getAttributeValue("AXDescription"), "Name", "Correct input label");
  is(input.getAttributeValue("AXTitle"), "", "Correct input title");
  is(input.getAttributeValue("AXValue"), "Elmer Fudd", "Correct input value");
  is(
    input.getAttributeValue("AXNumberOfCharacters"),
    10,
    "Correct length of value"
  );

  ok(input.attributeNames.includes("AXSelectedText"), "Has AXSelectedText");
  ok(
    input.attributeNames.includes("AXSelectedTextRange"),
    "Has AXSelectedTextRange"
  );

  let evt = Promise.all([
    waitForMacEvent("AXFocusedUIElementChanged", "input"),
    ...selectedTextEventPromises(AXTextStateChangeTypeSelectionMove),
  ]);
  await SpecialPowers.spawn(browser, [], () => {
    content.document.getElementById("input").focus();
  });
  await evt;

  evt = Promise.all(
    selectedTextEventPromises(AXTextStateChangeTypeSelectionExtend)
  );
  await SpecialPowers.spawn(browser, [], () => {
    let elm = content.document.getElementById("input");
    if (elm.setSelectionRange) {
      elm.setSelectionRange(6, 9);
    } else {
      let r = new content.Range();
      let textNode = elm.firstElementChild.firstChild;
      r.setStart(textNode, 6);
      r.setEnd(textNode, 9);

      let s = content.getSelection();
      s.removeAllRanges();
      s.addRange(r);
    }
  });
  await evt;

  is(
    input.getAttributeValue("AXSelectedText"),
    "Fud",
    "Correct text is selected"
  );

  Assert.deepEqual(
    input.getAttributeValue("AXSelectedTextRange"),
    [6, 3],
    "correct range selected"
  );

  ok(
    input.isAttributeSettable("AXSelectedTextRange"),
    "AXSelectedTextRange is settable"
  );

  evt = Promise.all(
    selectedTextEventPromises(AXTextStateChangeTypeSelectionExtend)
  );
  input.setAttributeValue("AXSelectedTextRange", NSRange(1, 7));
  await evt;

  Assert.deepEqual(
    input.getAttributeValue("AXSelectedTextRange"),
    [1, 7],
    "correct range selected"
  );

  is(
    input.getAttributeValue("AXSelectedText"),
    "lmer Fu",
    "Correct text is selected"
  );

  let domSelection = await SpecialPowers.spawn(browser, [], () => {
    let elm = content.document.querySelector("input#input");
    if (elm) {
      return elm.value.substring(elm.selectionStart, elm.selectionEnd);
    }

    return content.getSelection().toString();
  });

  is(domSelection, "lmer Fu", "correct DOM selection");

  is(
    input.getParameterizedAttributeValue("AXStringForRange", NSRange(3, 5)),
    "er Fu",
    "AXStringForRange works"
  );
}

/**
 * Input selection test
 */
addAccessibleTask(
  `<input aria-label="Name" id="input" value="Elmer Fudd">`,
  testInput
);

/**
 * contenteditable selection test
 */
addAccessibleTask(
  `<div aria-label="Name" tabindex="0" role="textbox" aria-multiline="true" id="input" contenteditable>
     <p>Elmer Fudd</p>
   </div>`,
  testInput
);

/**
 * test contenteditable with selection that extends past editable part
 */
addAccessibleTask(
  `<span aria-label="Name"
         tabindex="0"
         role="textbox"
         id="input"
         contenteditable>Elmer Fudd</span> <span id="notinput">is the name</span>`,
  async (browser, accDoc) => {
    let evt = Promise.all([
      waitForMacEvent("AXFocusedUIElementChanged", "input"),
      waitForMacEvent("AXSelectedTextChanged", "body"),
      waitForMacEvent("AXSelectedTextChanged", "input"),
    ]);
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("input").focus();
    });
    await evt;

    evt = waitForEvent(EVENT_TEXT_CARET_MOVED);
    await SpecialPowers.spawn(browser, [], () => {
      let input = content.document.getElementById("input");
      let notinput = content.document.getElementById("notinput");

      let r = new content.Range();
      r.setStart(input.firstChild, 4);
      r.setEnd(notinput.firstChild, 6);

      let s = content.getSelection();
      s.removeAllRanges();
      s.addRange(r);
    });
    await evt;

    let input = getNativeInterface(accDoc, "input");

    is(
      input.getAttributeValue("AXSelectedText"),
      "r Fudd",
      "Correct text is selected in #input"
    );

    is(
      stringForRange(
        input,
        input.getAttributeValue("AXSelectedTextMarkerRange")
      ),
      "r Fudd is the",
      "Correct text is selected in document"
    );
  }
);

/**
 * test nested content editables and their ancestor getters.
 */
addAccessibleTask(
  `<div id="outer" role="textbox" contenteditable="true">
     <p id="p">Bob <a href="#" id="link">Loblaw's</a></p>
     <div id="inner" role="textbox" contenteditable="true">
       Law <a href="#" id="inner_link">Blog</a>
     </div>
   </div>`,
  (browser, accDoc) => {
    let link = getNativeInterface(accDoc, "link");
    let innerLink = getNativeInterface(accDoc, "inner_link");

    let idmatches = (elem, id) => {
      is(elem.getAttributeValue("AXDOMIdentifier"), id, "Matches ID");
    };

    idmatches(link.getAttributeValue("AXEditableAncestor"), "outer");
    idmatches(link.getAttributeValue("AXFocusableAncestor"), "outer");
    idmatches(link.getAttributeValue("AXHighestEditableAncestor"), "outer");

    idmatches(innerLink.getAttributeValue("AXEditableAncestor"), "inner");
    idmatches(innerLink.getAttributeValue("AXFocusableAncestor"), "inner");
    idmatches(
      innerLink.getAttributeValue("AXHighestEditableAncestor"),
      "outer"
    );
  }
);
