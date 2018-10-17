/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that accepting a code completion with an invalid dot-notation property turns the
// property access into an element access (`x.data-test` -> `x["data-test"]`).

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<p>test code completion
  <script>
    x = Object.create(null);
    x.dataTest = 1;
    x["data-test"] = 2;
    x['da"ta"test'] = 3;
    x["DATA-TEST"] = 4;
  </script>`;

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const {jsterm} = await openNewTabAndConsole(TEST_URI);
  const {autocompletePopup} = jsterm;

  info("Ensure we have the correct items in the autocomplete popup");
  let onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString("x.da");
  await onPopUpOpen;
  is(getAutocompletePopupLabels(autocompletePopup).join("|"),
    "da\"ta\"test|data-test|dataTest|DATA-TEST",
    `popup has expected items`);

  info("Test that accepting the completion of a property with quotes changes the " +
    "property access into an element access");
  checkJsTermCompletionValue(jsterm, `    "ta"test`, `completeNode has expected value`);
  let onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkJsTermValueAndCursor(jsterm, `x["da\\"ta\\"test"]|`,
    "Property access was transformed into an element access, and quotes were properly " +
    "escaped");
  checkJsTermCompletionValue(jsterm, "", `completeNode is empty`);

  jsterm.setInputValue("");

  info("Test that accepting the completion of a property with a dash changes the " +
    "property access into an element access");
  onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString("x.data-");
  await onPopUpOpen;

  checkJsTermCompletionValue(jsterm, `       test`, `completeNode has expected value`);
  onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkJsTermValueAndCursor(jsterm, `x["data-test"]|`,
   "Property access was transformed into an element access");
  checkJsTermCompletionValue(jsterm, "", `completeNode is empty`);

  jsterm.setInputValue("");

  info("Test that accepting the completion of an uppercase property with a dash changes" +
    " the property access into an element access with the correct casing");
  onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString("x.data-");
  await onPopUpOpen;

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkJsTermCompletionValue(jsterm, `       TEST`, `completeNode has expected value`);
  onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkJsTermValueAndCursor(jsterm, `x["DATA-TEST"]|`,
   "Property access was transformed into an element access with the correct casing");
  checkJsTermCompletionValue(jsterm, "", `completeNode is empty`);

  jsterm.setInputValue("");

  info("Test that accepting the completion of an property with a dash, when the popup " +
    " is not displayed still changes the property access into an element access");
  await setInputValueForAutocompletion(jsterm, "x.D");
  checkJsTermCompletionValue(jsterm, `   ATA-TEST`, `completeNode has expected value`);

  EventUtils.synthesizeKey("KEY_Tab");

  checkJsTermValueAndCursor(jsterm, `x["DATA-TEST"]|`,
   "Property access was transformed into an element access with the correct casing");
  checkJsTermCompletionValue(jsterm, "", `completeNode is empty`);
}

function getAutocompletePopupLabels(autocompletePopup) {
  return autocompletePopup.items.map(i => i.label);
}
