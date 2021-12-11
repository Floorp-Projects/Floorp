/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the autocomplete for the class panel input behaves as expected. The test also
// checks that we're using the cache to retrieve the data when we can do so, and that the
// cache gets cleared, and we're getting data from the server, when there's mutation on
// the page.

const TEST_URI = `${URL_ROOT}doc_class_panel_autocomplete.html`;

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  const { addEl: textInput } = view.classListPreviewer;

  info("Open the class panel");
  view.showClassPanel();

  textInput.focus();

  info("Type a letter and check that the popup has the expected items");
  const allClasses = [
    "auto-body-class-1",
    "auto-body-class-2",
    "auto-bold",
    "auto-cssom-primary-color",
    "auto-div-class-1",
    "auto-div-class-2",
    "auto-html-class-1",
    "auto-html-class-2",
    "auto-inline-class-1",
    "auto-inline-class-2",
    "auto-inline-class-3",
    "auto-inline-class-4",
    "auto-inline-class-5",
    "auto-stylesheet-class-1",
    "auto-stylesheet-class-2",
    "auto-stylesheet-class-3",
    "auto-stylesheet-class-4",
    "auto-stylesheet-class-5",
  ];

  const { autocompletePopup } = view.classListPreviewer;
  let onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("a", {}, view.styleWindow);
  await onPopupOpened;
  await checkAutocompleteItems(
    autocompletePopup,
    allClasses,
    "The autocomplete popup has all the classes used in the DOM and in stylesheets"
  );

  info(
    "Test that typing more letters filters the autocomplete popup and uses the cache mechanism"
  );
  const onCacheHit = inspector.inspectorFront.pageStyle.once(
    "getAttributesInOwnerDocument-cache-hit"
  );
  EventUtils.sendString("uto-b", view.styleWindow);

  await checkAutocompleteItems(
    autocompletePopup,
    allClasses.filter(cls => cls.startsWith("auto-b")),
    "The autocomplete popup was filtered with the content of the input"
  );
  await onCacheHit;
  ok(true, "The results were retrieved from the cache mechanism");

  info("Test that autocomplete shows up-to-date results");
  // Modify the content page and assert that the new class is displayed in the
  // autocomplete if the user types a new letter.
  const onNewMutation = inspector.inspectorFront.walker.once("new-mutations");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    content.document.body.classList.add("auto-body-added-by-script");
  });
  await onNewMutation;

  // input is now auto-body
  onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.sendString("ody", view.styleWindow);
  await onPopupOpened;
  await checkAutocompleteItems(
    autocompletePopup,
    [
      ...allClasses.filter(cls => cls.startsWith("auto-body")),
      "auto-body-added-by-script",
    ].sort(),
    "The autocomplete popup was filtered with the content of the input"
  );

  info(
    "Test that typing a letter that won't match any of the item closes the popup"
  );
  // input is now auto-bodyy
  let onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("y", {}, view.styleWindow);
  await onPopupClosed;
  ok(true, "The popup was closed as expected");
  await checkAutocompleteItems(autocompletePopup, [], "The popup was cleared");

  info("Clear the input and try to autocomplete again");
  textInput.select();
  EventUtils.synthesizeKey("KEY_Backspace", {}, view.styleWindow);
  // Wait a bit so the debounced function can be executed
  await wait(200);

  onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("a", {}, view.styleWindow);
  await onPopupOpened;

  await checkAutocompleteItems(
    autocompletePopup,
    [...allClasses, "auto-body-added-by-script"].sort(),
    "The autocomplete popup was updated with the new class added to the DOM"
  );

  info("Test keyboard shortcut when the popup is displayed");
  // Escape to hide
  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Escape", {}, view.styleWindow);
  await onPopupClosed;
  ok(true, "The popup was closed when hitting escape");

  // Ctrl + space to show again
  onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey(" ", { ctrlKey: true }, view.styleWindow);
  await onPopupOpened;
  ok(true, "Popup was opened again with Ctrl+Space");
  await checkAutocompleteItems(
    autocompletePopup,
    [...allClasses, "auto-body-added-by-script"].sort()
  );

  // Arrow left to hide
  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_ArrowLeft", {}, view.styleWindow);
  await onPopupClosed;
  ok(true, "The popup was closed as when hitting ArrowLeft");

  // Arrow right and Ctrl + space to show again, and Arrow Right to accept
  onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("KEY_ArrowRight", {}, view.styleWindow);
  EventUtils.synthesizeKey(" ", { ctrlKey: true }, view.styleWindow);
  await onPopupOpened;

  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_ArrowRight", {}, view.styleWindow);
  await onPopupClosed;
  is(
    textInput.value,
    "auto-body-added-by-script",
    "ArrowRight puts the selected item in the input and closes the popup"
  );

  // Backspace to show the list again
  onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("KEY_Backspace", {}, view.styleWindow);
  await onPopupOpened;
  is(
    textInput.value,
    "auto-body-added-by-scrip",
    "ArrowRight puts the selected item in the input and closes the popup"
  );
  await checkAutocompleteItems(
    autocompletePopup,
    ["auto-body-added-by-script"],
    "The autocomplete does show the matching items after hitting backspace"
  );

  // Enter to accept
  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Enter", {}, view.styleWindow);
  await onPopupClosed;
  is(
    textInput.value,
    "auto-body-added-by-script",
    "Enter puts the selected item in the input and closes the popup"
  );

  // Backspace to show again
  onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("KEY_Backspace", {}, view.styleWindow);
  await onPopupOpened;
  is(
    textInput.value,
    "auto-body-added-by-scrip",
    "ArrowRight puts the selected item in the input and closes the popup"
  );
  await checkAutocompleteItems(
    autocompletePopup,
    ["auto-body-added-by-script"],
    "The autocomplete does show the matching items after hitting backspace"
  );

  // Tab to accept
  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab", {}, view.styleWindow);
  await onPopupClosed;
  is(
    textInput.value,
    "auto-body-added-by-script",
    "Tab puts the selected item in the input and closes the popup"
  );
});

async function checkAutocompleteItems(
  autocompletePopup,
  expectedItems,
  assertionMessage
) {
  await waitForSuccess(
    () =>
      getAutocompleteItems(autocompletePopup).length === expectedItems.length
  );
  const items = getAutocompleteItems(autocompletePopup);
  const formatList = list => `\n${list.join("\n")}\n`;
  is(formatList(items), formatList(expectedItems), assertionMessage);
}

function getAutocompleteItems(autocompletePopup) {
  return Array.from(autocompletePopup._panel.querySelectorAll("li")).map(
    el => el.textContent
  );
}
