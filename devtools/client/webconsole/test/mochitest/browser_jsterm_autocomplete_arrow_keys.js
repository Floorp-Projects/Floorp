/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<head><script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    window.foo = Object.create(null, Object.getOwnPropertyDescriptors({
      aa: "a",
      bbb: "b",
      bbbb: "b",
    }));
  </script></head><body>Autocomplete text navigation key usage test</body>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup: popup } = jsterm;

  await checkWordNavigation(hud);
  await checkArrowLeftDismissPopup(hud);
  await checkArrowLeftDismissCompletion(hud);
  await checkArrowRightAcceptCompletion(hud);

  info(
    "Test that Ctrl/Cmd + Right closes the popup if there's text after cursor"
  );
  setInputValue(hud, ".");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  const onPopUpOpen = popup.once("popup-opened");
  EventUtils.sendString("win");
  await onPopUpOpen;
  ok(popup.isOpen, "popup is open");

  const isOSX = Services.appinfo.OS == "Darwin";
  const onPopUpClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_ArrowRight", {
    [isOSX ? "metaKey" : "ctrlKey"]: true,
  });
  await onPopUpClose;
  is(getInputValue(hud), "win.", "input value wasn't modified");
});

async function checkArrowLeftDismissPopup(hud) {
  const popup = hud.jsterm.autocompletePopup;
  let tests;
  if (Services.appinfo.OS == "Darwin") {
    tests = [
      {
        keyOption: null,
        expectedInput: "window.foo.b|b",
      },
      {
        keyOption: { metaKey: true },
        expectedInput: "|window.foo.bb",
      },
      {
        keyOption: { altKey: true },
        expectedInput: "window.foo.|bb",
      },
    ];
  } else {
    tests = [
      {
        keyOption: null,
        expectedInput: "window.foo.b|b",
      },
      {
        keyOption: { ctrlKey: true },
        expectedInput: "window.foo.|bb",
      },
    ];
  }

  for (const test of tests) {
    info("Trigger autocomplete popup opening");
    const onPopUpOpen = popup.once("popup-opened");
    await setInputValueForAutocompletion(hud, "window.foo.bb");
    await onPopUpOpen;

    // checkInput is asserting the cursor position with the "|" char.
    checkInputValueAndCursorPosition(hud, "window.foo.bb|");
    is(popup.isOpen, true, "popup is open");
    checkInputCompletionValue(hud, "b", "completeNode has expected value");

    const { keyOption, expectedInput } = test;
    info(`Test that arrow left closes the popup and clears complete node`);
    const onPopUpClose = popup.once("popup-closed");
    EventUtils.synthesizeKey("KEY_ArrowLeft", keyOption);
    await onPopUpClose;

    checkInputValueAndCursorPosition(hud, expectedInput);
    is(popup.isOpen, false, "popup is closed");
    checkInputCompletionValue(hud, "", "completeNode is empty");
  }
  setInputValue(hud, "");
}

async function checkArrowLeftDismissCompletion(hud) {
  let tests;
  if (Services.appinfo.OS == "Darwin") {
    tests = [
      {
        keyOption: null,
        expectedInput: "window.foo.|a",
      },
      {
        keyOption: { metaKey: true },
        expectedInput: "|window.foo.a",
      },
      {
        keyOption: { altKey: true },
        expectedInput: "window.foo.|a",
      },
    ];
  } else {
    tests = [
      {
        keyOption: null,
        expectedInput: "window.foo.|a",
      },
      {
        keyOption: { ctrlKey: true },
        expectedInput: "window.foo.|a",
      },
    ];
  }

  for (const test of tests) {
    await setInputValueForAutocompletion(hud, "window.foo.a");
    checkInputCompletionValue(hud, "a", "completeNode has expected value");

    info(`Test that arrow left dismiss the completion text`);
    const { keyOption, expectedInput } = test;
    EventUtils.synthesizeKey("KEY_ArrowLeft", keyOption);

    checkInputValueAndCursorPosition(hud, expectedInput);
    checkInputCompletionValue(hud, "", "completeNode is empty");
  }
  setInputValue(hud, "");
}

async function checkArrowRightAcceptCompletion(hud) {
  const popup = hud.jsterm.autocompletePopup;
  let tests;
  if (Services.appinfo.OS == "Darwin") {
    tests = [
      {
        keyOption: null,
      },
      {
        keyOption: { metaKey: true },
      },
      {
        keyOption: { altKey: true },
      },
    ];
  } else {
    tests = [
      {
        keyOption: null,
      },
      {
        keyOption: { ctrlKey: true },
      },
    ];
  }

  for (const test of tests) {
    info("Trigger autocomplete popup opening");
    const onPopUpOpen = popup.once("popup-opened");
    await setInputValueForAutocompletion(hud, `window.foo.bb`);
    await onPopUpOpen;

    // checkInput is asserting the cursor position with the "|" char.
    checkInputValueAndCursorPosition(hud, `window.foo.bb|`);
    is(popup.isOpen, true, "popup is open");
    checkInputCompletionValue(hud, "b", "completeNode has expected value");

    const { keyOption } = test;
    info(`Test that arrow right closes the popup and accepts the completion`);
    const onPopUpClose = popup.once("popup-closed");
    EventUtils.synthesizeKey("KEY_ArrowRight", keyOption);
    await onPopUpClose;

    checkInputValueAndCursorPosition(hud, "window.foo.bbb|");
    is(popup.isOpen, false, "popup is closed");
    checkInputCompletionValue(hud, "", "completeNode is empty");
  }
  setInputValue(hud, "");
}

async function checkWordNavigation(hud) {
  const accelKey = Services.appinfo.OS == "Darwin" ? "altKey" : "ctrlKey";
  const goLeft = () =>
    EventUtils.synthesizeKey("KEY_ArrowLeft", { [accelKey]: true });
  const goRight = () =>
    EventUtils.synthesizeKey("KEY_ArrowRight", { [accelKey]: true });

  setInputValue(hud, "aa bb cc dd");
  checkInputValueAndCursorPosition(hud, "aa bb cc dd|");

  goRight();
  checkInputValueAndCursorPosition(hud, "aa bb cc dd|");

  goLeft();
  checkInputValueAndCursorPosition(hud, "aa bb cc |dd");

  goLeft();
  checkInputValueAndCursorPosition(hud, "aa bb |cc dd");

  goLeft();
  checkInputValueAndCursorPosition(hud, "aa |bb cc dd");

  goLeft();
  checkInputValueAndCursorPosition(hud, "|aa bb cc dd");

  goLeft();
  checkInputValueAndCursorPosition(hud, "|aa bb cc dd");

  goRight();
  // Windows differ from other platforms, going to the start of the next string.
  checkInputValueAndCursorPosition(hud, "aa| bb cc dd");

  goRight();
  checkInputValueAndCursorPosition(hud, "aa bb| cc dd");

  goRight();
  checkInputValueAndCursorPosition(hud, "aa bb cc| dd");

  goRight();
  checkInputValueAndCursorPosition(hud, "aa bb cc dd|");

  setInputValue(hud, "");
}
