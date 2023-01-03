/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that adding new CSS properties with special characters in the property
// name does note create duplicate entries.

const PROPERTY_NAME = '"abc"';
const INITIAL_VALUE = "foo";
// For assertions the quotes in the property will be escaped.
const EXPECTED_PROPERTY_NAME = '\\"abc\\"';

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
  </style>
  <div>test</div>
`;

add_task(async function addWithSpecialCharacter() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);

  await selectNode("div", inspector);

  const ruleEditor = getRuleViewRuleEditor(ruleView, 1);
  const editor = await focusEditableField(ruleView, ruleEditor.closeBrace);

  const input = editor.input;
  input.value = `${PROPERTY_NAME}: ${INITIAL_VALUE};`;

  let onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  info("Pressing return to commit and focus the new value field");
  const onModifications = ruleView.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, ruleView.styleWindow);
  await onModifications;
  await onTrackChange;
  await assertAddedDeclaration(doc, EXPECTED_PROPERTY_NAME, INITIAL_VALUE);

  let newValue = "def";
  info(`Change the CSS declaration value to ${newValue}`);
  const prop = getTextProperty(ruleView, 1, { [PROPERTY_NAME]: INITIAL_VALUE });
  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  // flushCount needs to be set to 2 once when quotes are involved.
  await setProperty(ruleView, prop, newValue, { flushCount: 2 });
  await onTrackChange;
  await assertAddedDeclaration(doc, EXPECTED_PROPERTY_NAME, newValue);

  newValue = "123";
  info(`Change the CSS declaration value to ${newValue}`);
  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  await setProperty(ruleView, prop, newValue);
  await onTrackChange;
  await assertAddedDeclaration(doc, EXPECTED_PROPERTY_NAME, newValue);
});

/**
 * Check that we only received a single added declaration with the expected
 * value.
 */
async function assertAddedDeclaration(doc, expectedName, expectedValue) {
  await waitFor(() => {
    const addDecl = getAddedDeclarations(doc);
    return (
      addDecl.length == 1 &&
      addDecl[0].value == expectedValue &&
      addDecl[0].property == expectedName
    );
  }, "Got the expected declaration");
  is(getAddedDeclarations(doc).length, 1, "Only one added declaration");
  is(getRemovedDeclarations(doc).length, 0, "No removed declaration");
}
