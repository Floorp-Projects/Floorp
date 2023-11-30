/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the autocomplete for the class panel input behaves as expected. The test also
// checks that we're using the cache to retrieve the data when we can do so, and that the
// cache gets cleared, and we're getting data from the server, when there's mutation on
// the page.

const TEST_URI = `${URL_ROOT}doc_class_panel_autocomplete.html`;

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  const { addEl: textInput } = view.classListPreviewer;
  await selectNode("#auto-div-id-3", inspector);

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
    "auto-inline-nested-class-1",
    "auto-inline-nested-class-2",
    "auto-inline-nested-class-3",
    "auto-inline-nested-class-4",
    "auto-inline-nested-class-5",
    "auto-inline-nested-class-6",
    "auto-stylesheet-class-1",
    "auto-stylesheet-class-2",
    "auto-stylesheet-class-3",
    "auto-stylesheet-class-4",
    "auto-stylesheet-class-5",
    "auto-stylesheet-nested-class-1",
    "auto-stylesheet-nested-class-2",
    "auto-stylesheet-nested-class-3",
    "auto-stylesheet-nested-class-4",
    "auto-stylesheet-nested-class-5",
    "auto-stylesheet-nested-class-6",
  ];

  const { autocompletePopup } = view.classListPreviewer;
  let onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("a", {}, view.styleWindow);
  await waitForClassApplied("auto-body-class-1", "#auto-div-id-3");
  await onPopupOpened;
  await checkAutocompleteItems(
    autocompletePopup,
    allClasses,
    "The autocomplete popup has all the classes used in the DOM and in stylesheets"
  );

  info(
    "Test that typing more letters filters the autocomplete popup and uses the cache mechanism"
  );
  EventUtils.sendString("uto-b", view.styleWindow);
  await waitForClassApplied("auto-body-class-1", "#auto-div-id-3");

  await checkAutocompleteItems(
    autocompletePopup,
    allClasses.filter(cls => cls.startsWith("auto-b")),
    "The autocomplete popup was filtered with the content of the input"
  );
  ok(true, "The results were retrieved from the cache mechanism");

  info("Test that autocomplete shows up-to-date results");
  // Modify the content page and assert that the new class is displayed in the
  // autocomplete if the user types a new letter.
  const onNewMutation = inspector.inspectorFront.walker.once("new-mutations");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function () {
    content.document.body.classList.add("auto-body-added-by-script");
  });
  await onNewMutation;
  await waitForClassApplied("auto-body-added-by-script", "body");

  // close & reopen the autocomplete so it picks up the added to another element while autocomplete was opened
  let onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Escape", {}, view.styleWindow);
  await onPopupClosed;

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
  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("y", {}, view.styleWindow);
  await waitForClassApplied("auto-bodyy", "#auto-div-id-3");
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
  await waitForClassApplied("auto-body-added-by-script", "#auto-div-id-3");
  await onPopupClosed;
  is(
    textInput.value,
    "auto-body-added-by-script",
    "ArrowRight puts the selected item in the input and closes the popup"
  );

  // Backspace to show the list again
  onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("KEY_Backspace", {}, view.styleWindow);
  await waitForClassApplied("auto-body-added-by-script", "#auto-div-id-3");
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
  await waitForClassRemoved("auto-body-added-by-scrip");
  await onPopupClosed;
  is(
    textInput.value,
    "auto-body-added-by-script",
    "Enter puts the selected item in the input and closes the popup"
  );

  // Backspace to show again
  onPopupOpened = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("KEY_Backspace", {}, view.styleWindow);
  await waitForClassApplied("auto-body-added-by-script", "#auto-div-id-3");
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
  await waitForClassRemoved("auto-body-added-by-scrip");
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
  Assert.deepEqual(items, expectedItems, assertionMessage);
}

function getAutocompleteItems(autocompletePopup) {
  return Array.from(autocompletePopup._panel.querySelectorAll("li")).map(
    el => el.textContent
  );
}

async function waitForClassApplied(cls, selector) {
  info("Wait for class to be applied: " + cls);
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [cls, selector],
    async (_cls, _selector) => {
      return ContentTaskUtils.waitForCondition(() =>
        content.document.querySelector(_selector).classList.contains(_cls)
      );
    }
  );
  // Wait for debounced functions to be executed
  await wait(200);
}

async function waitForClassRemoved(cls) {
  info("Wait for class to be removed: " + cls);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [cls], async _cls => {
    return ContentTaskUtils.waitForCondition(
      () =>
        !content.document
          .querySelector("#auto-div-id-3")
          .classList.contains(_cls)
    );
  });
  // Wait for debounced functions to be executed
  await wait(200);
}
