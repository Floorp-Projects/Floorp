/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
<script>
let x = 3, y = 4;
function zzyzx() {
  x = 10;
}
function zzyzx2() {
  x = 10;
}
var obj = {propA: "A", propB: "B"};
</script>
`;

const EAGER_EVALUATION_PREF = "devtools.webconsole.input.eagerEvaluation";

// Basic testing of eager evaluation functionality. Expressions which can be
// eagerly evaluated should show their results, and expressions with side
// effects should not perform those side effects.
add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  // Do an evaluation to populate $_
  await executeAndWaitForMessage(
    hud,
    "'result: ' + (x + y)",
    "result: 7",
    ".result"
  );

  setInputValue(hud, "x + y");
  await waitForEagerEvaluationResult(hud, "7");

  setInputValue(hud, "x + y + undefined");
  await waitForEagerEvaluationResult(hud, "NaN");

  setInputValue(hud, "1 - 1");
  await waitForEagerEvaluationResult(hud, "0");

  setInputValue(hud, "!true");
  await waitForEagerEvaluationResult(hud, "false");

  setInputValue(hud, `"ab".slice(0, 0)`);
  await waitForEagerEvaluationResult(hud, `""`);

  setInputValue(hud, `JSON.parse("null")`);
  await waitForEagerEvaluationResult(hud, "null");

  setInputValue(hud, "-x / 0");
  await waitForEagerEvaluationResult(hud, "-Infinity");

  setInputValue(hud, "x = 10");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + 1");
  await waitForEagerEvaluationResult(hud, "4");

  setInputValue(hud, "zzyzx()");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + 2");
  await waitForEagerEvaluationResult(hud, "5");

  setInputValue(hud, "x +");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + z");
  await waitForEagerEvaluationResult(hud, /ReferenceError/);

  setInputValue(hud, "var a = 5");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "x + a");
  await waitForEagerEvaluationResult(hud, /ReferenceError/);

  setInputValue(hud, '"foobar".slice(1, 5)');
  await waitForEagerEvaluationResult(hud, '"ooba"');

  setInputValue(hud, '"foobar".toString()');
  await waitForEagerEvaluationResult(hud, '"foobar"');

  setInputValue(hud, "(new Array()).push(3)");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "(new Uint32Array([1,2,3])).includes(2)");
  await waitForEagerEvaluationResult(hud, "true");

  setInputValue(hud, "Math.round(3.2)");
  await waitForEagerEvaluationResult(hud, "3");

  info("Check that $_ wasn't polluted by eager evaluations");
  setInputValue(hud, "$_");
  await waitForEagerEvaluationResult(hud, `"result: 7"`);

  setInputValue(hud, "'> ' + $_");
  await waitForEagerEvaluationResult(hud, `"> result: 7"`);

  info("Switch to editor mode");
  await toggleLayout(hud);
  await waitForEagerEvaluationResult(hud, `"> result: 7"`);
  ok(true, "eager evaluation is still displayed in editor mode");

  setInputValue(hud, "4 + 7");
  await waitForEagerEvaluationResult(hud, "11");

  setInputValue(hud, "typeof new Proxy({}, {})");
  await waitForEagerEvaluationResult(hud, `"object"`);

  setInputValue(hud, "typeof Proxy.revocable({}, {}).revoke");
  await waitForEagerEvaluationResult(hud, `"function"`);

  setInputValue(hud, "Reflect.apply(() => 1, null, [])");
  await waitForEagerEvaluationResult(hud, "1");
  setInputValue(
    hud,
    `Reflect.apply(() => {
      globalThis.sideEffect = true;
      return 2;
    }, null, [])`
  );
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.construct(Array, []).length");
  await waitForEagerEvaluationResult(hud, "0");
  setInputValue(
    hud,
    `Reflect.construct(function() {
      globalThis.sideEffect = true;
    }, [])`
  );
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.defineProperty({}, 'a', {value: 1})");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.deleteProperty({a: 1}, 'a')");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.get({a: 1}, 'a')");
  await waitForEagerEvaluationResult(hud, "1");
  setInputValue(hud, "Reflect.get({get a(){return 2}, 'a')");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.getOwnPropertyDescriptor({a: 1}, 'a').value");
  await waitForEagerEvaluationResult(hud, "1");
  setInputValue(
    hud,
    `Reflect.getOwnPropertyDescriptor(
      new Proxy({ a: 2 }, { getOwnPropertyDescriptor() {
        globalThis.sideEffect = true;
        return { value: 2 };
      }}),
      "a"
    )`
  );
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.getPrototypeOf({}) === Object.prototype");
  await waitForEagerEvaluationResult(hud, "true");
  setInputValue(
    hud,
    `Reflect.getPrototypeOf(
      new Proxy({}, { getPrototypeOf() {
        globalThis.sideEffect = true;
        return null;
      }})
    )`
  );
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.has({a: 1}, 'a')");
  await waitForEagerEvaluationResult(hud, "true");
  setInputValue(
    hud,
    `Reflect.has(
      new Proxy({ a: 2 }, { has() {
        globalThis.sideEffect = true;
        return true;
      }}), "a"
    )`
  );
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.isExtensible({})");
  await waitForEagerEvaluationResult(hud, "true");
  setInputValue(
    hud,
    `Reflect.isExtensible(
      new Proxy({}, { isExtensible() {
        globalThis.sideEffect = true;
        return true;
      }})
    )`
  );
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.ownKeys({a: 1})[0]");
  await waitForEagerEvaluationResult(hud, `"a"`);
  setInputValue(
    hud,
    `Reflect.ownKeys(
      new Proxy({}, { ownKeys() {
        globalThis.sideEffect = true;
        return ['a'];
      }})
    )`
  );
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.preventExtensions({})");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.set({}, 'a', 1)");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "Reflect.setPrototypeOf({}, null)");
  await waitForNoEagerEvaluationResult(hud);

  setInputValue(hud, "[] instanceof Array");
  await waitForEagerEvaluationResult(hud, "true");

  // go back to inline layout.
  await toggleLayout(hud);
});

// Test that the currently selected autocomplete result is eagerly evaluated.
add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;

  const { autocompletePopup: popup } = jsterm;

  ok(!popup.isOpen, "popup is not open");
  let onPopupOpen = popup.once("popup-opened");
  EventUtils.sendString("zzy");
  await onPopupOpen;

  await waitForEagerEvaluationResult(hud, "function zzyzx()");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await waitForEagerEvaluationResult(hud, "function zzyzx2()");

  // works when the input isn't properly cased but matches an autocomplete item
  setInputValue(hud, "o");
  onPopupOpen = popup.once("popup-opened");
  EventUtils.sendString("B");
  await waitForEagerEvaluationResult(hud, `Object { propA: "A", propB: "B" }`);

  // works when doing element access without quotes
  setInputValue(hud, "obj[p");
  onPopupOpen = popup.once("popup-opened");
  EventUtils.sendString("RoP");
  await waitForEagerEvaluationResult(hud, `"A"`);

  EventUtils.synthesizeKey("KEY_ArrowDown");
  await waitForEagerEvaluationResult(hud, `"B"`);

  // closing the autocomplete popup updates the eager evaluation result
  let onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Escape");
  await onPopupClose;
  await waitForNoEagerEvaluationResult(hud);

  info(
    "Check that closing the popup by adding a space will update the instant eval result"
  );
  await setInputValueForAutocompletion(hud, "x");
  await waitForEagerEvaluationResult(hud, "3");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  // Navigates to the XMLDocument item in the popup
  await waitForEagerEvaluationResult(hud, `function ()`);

  onPopupClose = popup.once("popup-closed");
  EventUtils.sendString(" ");
  await waitForEagerEvaluationResult(hud, `3`);
});

// Test that the setting works as expected.
add_task(async function() {
  // start with the pref off.
  await pushPref(EAGER_EVALUATION_PREF, false);
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Check that the setting is disabled");
  checkConsoleSettingState(
    hud,
    ".webconsole-console-settings-menu-item-eager-evaluation",
    false
  );

  // Wait for the autocomplete popup to be displayed so we know the eager evaluation could
  // have occured.
  const onPopupOpen = hud.jsterm.autocompletePopup.once("popup-opened");
  await setInputValueForAutocompletion(hud, "x + y");
  await onPopupOpen;

  is(
    getEagerEvaluationElement(hud),
    null,
    "There's no eager evaluation element"
  );
  hud.jsterm.autocompletePopup.hidePopup();

  info("Turn on the eager evaluation");
  toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-eager-evaluation"
  );
  await waitFor(() => getEagerEvaluationElement(hud));
  ok(true, "The eager evaluation element is now displayed");
  is(
    Services.prefs.getBoolPref(EAGER_EVALUATION_PREF),
    true,
    "Pref was changed"
  );

  setInputValue(hud, "1 + 2");
  await waitForEagerEvaluationResult(hud, "3");
  ok(true, "Eager evaluation result is displayed");

  info("Turn off the eager evaluation");
  toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-eager-evaluation"
  );
  await waitFor(() => !getEagerEvaluationElement(hud));
  is(
    Services.prefs.getBoolPref(EAGER_EVALUATION_PREF),
    false,
    "Pref was changed"
  );
  ok(true, "Eager evaluation element is no longer displayed");
});
