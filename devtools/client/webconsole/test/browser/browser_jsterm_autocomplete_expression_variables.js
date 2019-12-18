/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that variable created in the expression are displayed in the autocomplete.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,Test autocompletion for expression variables<script>
    var testGlobal;
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  await setInputValueForAutocompletion(
    hud,
    `
    var testVar;
    let testLet;
    const testConst;
    class testClass {}
    function testFunc(testParam1, testParam2, ...testParamRest) {
      var [testParamRestFirst] = testParamRest;
      let {testDeconstruct1,testDeconstruct2, ...testDeconstructRest} = testParam1;
      test`
  );

  is(
    getPopupLabels(autocompletePopup).join("\n"),
    [
      "testClass",
      "testConst",
      "testDeconstruct1",
      "testDeconstruct2",
      "testDeconstructRest",
      "testFunc",
      "testGlobal",
      "testLet",
      "testParam1",
      "testParam2",
      "testParamRest",
      "testParamRestFirst",
      "testVar",
    ].join("\n"),
    "Autocomplete popup displays both global and local variables"
  );
});

function getPopupLabels(popup) {
  return popup.getItems().map(item => item.label);
}
