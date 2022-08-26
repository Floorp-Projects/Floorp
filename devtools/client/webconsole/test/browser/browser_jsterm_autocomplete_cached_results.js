/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the cached autocomplete results are used when the new
// user input is a subset of the existing completion results.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html><script>
    x = Object.create(null, Object.getOwnPropertyDescriptors({
      dog: "woof",
      dos: "-",
      dot: ".",
      duh: 1,
      wut: 2,
    }))
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup: popup } = jsterm;

  const jstermComplete = (value, pos) =>
    setInputValueForAutocompletion(hud, value, pos);

  await jstermComplete("x.");
  is(
    getAutocompletePopupLabels(popup).toString(),
    ["dog", "dos", "dot", "duh", "wut"].toString(),
    "'x.' gave a list of suggestions"
  );
  ok(popup.isOpen, "popup is opened");

  info("Add a property on the object");
  let result = await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.x.docfoobar = "added";
    return content.wrappedJSObject.x.docfoobar;
  });

  is(result, "added", "The property was added on the window object");

  info("Test typing d (i.e. input is now 'x.d')");
  let onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("d");
  await onUpdated;
  ok(
    hasExactPopupLabels(popup, ["dog", "dos", "dot", "duh"]),
    "autocomplete popup does not contain docfoobar. List has not been updated"
  );

  // Test typing o (i.e. input is now 'x.do').
  onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("o");
  await onUpdated;
  ok(
    hasExactPopupLabels(popup, ["dog", "dos", "dot"]),
    "autocomplete popup still does not contain docfoobar. List has not been updated"
  );

  // Test that backspace does not cause a request to the server
  onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("KEY_Backspace");
  await onUpdated;
  ok(
    hasExactPopupLabels(popup, ["dog", "dos", "dot", "duh"]),
    "autocomplete cached results do not contain docfoobar. list has not been updated"
  );

  result = await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.x.docfoobar = "added";
    delete content.wrappedJSObject.x.docfoobar;
    return typeof content.wrappedJSObject.x.docfoobar;
  });
  is(result, "undefined", "The property was removed");

  // Test if 'window.getC' gives 'getComputedStyle'
  await jstermComplete("window.");
  await jstermComplete("window.getC");
  ok(
    hasPopupLabel(popup, "getComputedStyle"),
    "autocomplete results do contain getComputedStyle"
  );

  // Test if 'dump(d' gives non-zero results
  await jstermComplete("dump(d");
  ok(!!popup.getItems().length, "'dump(d' gives non-zero results");

  // Test that 'dump(x.)' works.
  await jstermComplete("dump(x)", -1);
  onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString(".");
  await onUpdated;
  ok(
    hasExactPopupLabels(popup, ["dog", "dos", "dot", "duh", "wut"]),
    "'dump(x.' gave a list of suggestions"
  );

  info("Add a property on the x object");
  result = await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.x.docfoobar = "added";
    return content.wrappedJSObject.x.docfoobar;
  });
  is(result, "added", "The property was added on the x object again");

  // Make sure 'dump(x.d)' does not contain 'docfoobar'.
  onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("d");
  await onUpdated;

  ok(
    !hasPopupLabel(popup, "docfoobar"),
    "autocomplete cached results do not contain docfoobar. list has not been updated"
  );

  info("Ensure filtering from the cache does work");
  execute(
    hud,
    `
    window.testObject = Object.create(null);
    window.testObject.zz = "zz";
    window.testObject.zzz = "zzz";
    window.testObject.zzzz = "zzzz";
  `
  );
  await jstermComplete("window.testObject.");
  await jstermComplete("window.testObject.z");
  ok(
    hasExactPopupLabels(popup, ["zz", "zzz", "zzzz"]),
    "results are the expected ones"
  );

  onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("z");
  await onUpdated;
  ok(
    hasExactPopupLabels(popup, ["zz", "zzz", "zzzz"]),
    "filtering from the cache works - step 1"
  );

  onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("z");
  await onUpdated;
  ok(
    hasExactPopupLabels(popup, ["zzz", "zzzz"]),
    "filtering from the cache works - step 2"
  );
});
