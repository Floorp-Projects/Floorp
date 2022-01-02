/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure source mapped variables appear in autocompletion
// on an equal footing with variables from the generated source.

"use strict";
/* import-globals-from head.js*/

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-autocomplete-mapped.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup: popup } = jsterm;
  const toolbox = await gDevTools.getToolboxForTab(gBrowser.selectedTab);

  info("Opening Debugger and enabling map scopes");
  await openDebugger();
  const dbg = createDebuggerContext(toolbox);
  dbg.actions.toggleMapScopes();

  info("Waiting for pause");
  // This calls firstCall() on the content page and waits for pause. (firstCall
  // has a debugger statement)
  await pauseDebugger(dbg);

  await toolbox.selectTool("webconsole");
  await setInputValueForAutocompletion(hud, "valu");
  ok(
    hasExactPopupLabels(popup, ["value", "valueOf", "values"]),
    "Autocomplete popup displays original variable name"
  );

  await setInputValueForAutocompletion(hud, "temp");
  ok(
    hasExactPopupLabels(popup, ["temp", "temp2"]),
    "Autocomplete popup displays original variable name when entering a complete variable name"
  );

  await setInputValueForAutocompletion(hud, "t");
  ok(
    hasPopupLabel(popup, "t"),
    "Autocomplete popup displays generated variable name"
  );

  await setInputValueForAutocompletion(hud, "value.to");
  ok(
    hasPopupLabel(popup, "toString"),
    "Autocomplete popup displays properties of original variable"
  );

  await setInputValueForAutocompletion(hud, "imported.imp");
  ok(
    hasPopupLabel(popup, "importResult"),
    "Autocomplete popup displays properties of multi-part variable"
  );

  let tooltip = await setInputValueForGetterConfirmDialog(
    toolbox,
    hud,
    "getter."
  );
  let labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter getter to retrieve the property list?",
    "Dialog has expected text content"
  );

  info(
    "Check that getter confirmation on a variable that maps to two getters invokes both getters"
  );
  let onPopUpOpen = popup.once("popup-opened");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopUpOpen;
  ok(popup.isOpen, "popup is open after Tab");
  ok(hasPopupLabel(popup, "getterResult"), "popup has expected items");

  info(
    "Check that the getter confirmation dialog shows the original variable name"
  );
  tooltip = await setInputValueForGetterConfirmDialog(
    toolbox,
    hud,
    "localWithGetter.value."
  );
  labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter localWithGetter.value to retrieve the property list?",
    "Dialog has expected text content"
  );

  info(
    "Check that hitting Tab does invoke the getter and return its properties"
  );
  onPopUpOpen = popup.once("popup-opened");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopUpOpen;
  ok(popup.isOpen, "popup is open after Tab");
  ok(hasPopupLabel(popup, "then"), "popup has expected items");
  info("got popup items: " + JSON.stringify(getAutocompletePopupLabels(popup)));

  info(
    "Check that authorizing an original getter applies to the generated getter"
  );
  await setInputValueForAutocompletion(hud, "o.value.");
  ok(hasPopupLabel(popup, "then"), "popup has expected items");

  await setInputValueForAutocompletion(hud, "(temp + temp2).");
  ok(
    hasPopupLabel(popup, "toFixed"),
    "Autocomplete popup displays properties of eagerly evaluated value"
  );
  info("got popup items: " + JSON.stringify(getAutocompletePopupLabels(popup)));

  info("Disabling map scopes");
  dbg.actions.toggleMapScopes();
  await setInputValueForAutocompletion(hud, "tem");
  const autocompleteLabels = getAutocompletePopupLabels(popup);
  ok(
    !autocompleteLabels.includes("temp"),
    "Autocomplete popup does not display mapped variables when mapping is disabled"
  );

  await resume(dbg);
});
