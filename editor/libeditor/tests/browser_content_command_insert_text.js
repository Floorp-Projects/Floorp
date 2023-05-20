/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
const { ContentTaskUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ContentTaskUtils.sys.mjs"
);
const gCUITestUtils = new CustomizableUITestUtils(window);
const gDOMWindowUtils = EventUtils._getDOMWindowUtils(window);

add_task(async function test_setup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

function promiseResettingSearchBarAndFocus() {
  const waitForFocusInSearchBar = BrowserTestUtils.waitForEvent(
    BrowserSearch.searchBar.textbox,
    "focus"
  );
  BrowserSearch.searchBar.textbox.focus();
  BrowserSearch.searchBar.textbox.value = "";
  return Promise.all([
    waitForFocusInSearchBar,
    TestUtils.waitForCondition(
      () =>
        gDOMWindowUtils.IMEStatus === Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED &&
        gDOMWindowUtils.inputContextOrigin ===
          Ci.nsIDOMWindowUtils.INPUT_CONTEXT_ORIGIN_MAIN
    ),
  ]);
}

function promiseIMEStateEnabledByRemote() {
  return TestUtils.waitForCondition(
    () =>
      gDOMWindowUtils.IMEStatus === Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED &&
      gDOMWindowUtils.inputContextOrigin ===
        Ci.nsIDOMWindowUtils.INPUT_CONTEXT_ORIGIN_CONTENT
  );
}

function promiseContentTick(browser) {
  return SpecialPowers.spawn(browser, [], async () => {
    await new Promise(r => {
      content.requestAnimationFrame(() => {
        content.requestAnimationFrame(r);
      });
    });
  });
}

add_task(async function test_text_editor_in_chrome() {
  await promiseResettingSearchBarAndFocus();

  let events = [];
  function logEvent(event) {
    events.push(event);
  }
  BrowserSearch.searchBar.textbox.addEventListener("beforeinput", logEvent);
  gDOMWindowUtils.sendContentCommandEvent("insertText", null, "XYZ");

  is(
    BrowserSearch.searchBar.textbox.value,
    "XYZ",
    "The string should be inserted into the focused search bar"
  );
  is(
    events.length,
    1,
    "One beforeinput event should be fired in the searchbar"
  );
  is(events[0]?.inputType, "insertText", 'inputType should be "insertText"');
  is(events[0]?.data, "XYZ", 'inputType should be "XYZ"');
  is(events[0]?.cancelable, true, "beforeinput event should be cancelable");
  BrowserSearch.searchBar.textbox.removeEventListener("beforeinput", logEvent);

  BrowserSearch.searchBar.textbox.blur();
});

add_task(async function test_text_editor_in_content() {
  for (const test of [
    {
      tag: "input",
      target: "input",
      nonTarget: "input + input",
      page: 'data:text/html,<input value="abc"><input value="def">',
    },
    {
      tag: "textarea",
      target: "textarea",
      nonTarget: "textarea + textarea",
      page: "data:text/html,<textarea>abc</textarea><textarea>def</textarea>",
    },
  ]) {
    // Once, move focus to chrome's searchbar.
    await promiseResettingSearchBarAndFocus();

    await BrowserTestUtils.withNewTab(test.page, async browser => {
      await SpecialPowers.spawn(browser, [test], async function (aTest) {
        content.window.focus();
        await ContentTaskUtils.waitForCondition(() =>
          content.document.hasFocus()
        );
        const target = content.document.querySelector(aTest.target);
        target.focus();
        target.selectionStart = target.selectionEnd = 2;
        content.document.documentElement.scrollTop; // Flush pending things
      });

      await promiseIMEStateEnabledByRemote();
      const waitForBeforeInputEvent = SpecialPowers.spawn(
        browser,
        [test],
        async function (aTest) {
          await new Promise(resolve => {
            content.document.querySelector(aTest.target).addEventListener(
              "beforeinput",
              event => {
                is(
                  event.inputType,
                  "insertText",
                  `The inputType of beforeinput event fired on <${aTest.target}> should be "insertText"`
                );
                is(
                  event.data,
                  "XYZ",
                  `The data of beforeinput event fired on <${aTest.target}> should be "XYZ"`
                );
                is(
                  event.cancelable,
                  true,
                  `The beforeinput event fired on <${aTest.target}> should be cancelable`
                );
                resolve();
              },
              { once: true }
            );
          });
        }
      );
      const waitForInputEvent = BrowserTestUtils.waitForContentEvent(
        browser,
        "input"
      );
      await promiseContentTick(browser); // Ensure "input" event listener in the remote process
      gDOMWindowUtils.sendContentCommandEvent("insertText", null, "XYZ");
      await waitForBeforeInputEvent;
      await waitForInputEvent;

      await SpecialPowers.spawn(browser, [test], async function (aTest) {
        is(
          content.document.querySelector(aTest.target).value,
          "abXYZc",
          `The string should be inserted into the focused <${aTest.target}> element`
        );
        is(
          content.document.querySelector(aTest.nonTarget).value,
          "def",
          `The string should not be inserted into the non-focused <${aTest.nonTarget}> element`
        );
      });
    });

    is(
      BrowserSearch.searchBar.textbox.value,
      "",
      "The string should not be inserted into the previously focused search bar"
    );
  }
});

add_task(async function test_html_editor_in_content() {
  for (const test of [
    {
      mode: "contenteditable",
      target: "div",
      page: "data:text/html,<div contenteditable>abc</div>",
    },
    {
      mode: "designMode",
      target: "div",
      page: "data:text/html,<div>abc</div>",
    },
  ]) {
    // Once, move focus to chrome's searchbar.
    await promiseResettingSearchBarAndFocus();

    await BrowserTestUtils.withNewTab(test.page, async browser => {
      await SpecialPowers.spawn(browser, [test], async function (aTest) {
        content.window.focus();
        await ContentTaskUtils.waitForCondition(() =>
          content.document.hasFocus()
        );
        const target = content.document.querySelector(aTest.target);
        if (aTest.mode == "designMode") {
          content.document.designMode = "on";
          content.window.focus();
        } else {
          target.focus();
        }
        content.window.getSelection().collapse(target.firstChild, 2);
        content.document.documentElement.scrollTop; // Flush pending things
      });

      await promiseIMEStateEnabledByRemote();
      const waitForBeforeInputEvent = SpecialPowers.spawn(
        browser,
        [test],
        async function (aTest) {
          await new Promise(resolve => {
            const eventTarget =
              aTest.mode === "designMode"
                ? content.document
                : content.document.querySelector(aTest.target);
            eventTarget.addEventListener(
              "beforeinput",
              event => {
                is(
                  event.inputType,
                  "insertText",
                  `The inputType of beforeinput event fired on ${aTest.mode} editor should be "insertText"`
                );
                is(
                  event.data,
                  "XYZ",
                  `The data of beforeinput event fired on ${aTest.mode} editor should be "XYZ"`
                );
                is(
                  event.cancelable,
                  true,
                  `The beforeinput event fired on ${aTest.mode} editor should be cancelable`
                );
                resolve();
              },
              { once: true }
            );
          });
        }
      );
      const waitForInputEvent = BrowserTestUtils.waitForContentEvent(
        browser,
        "input"
      );
      await promiseContentTick(browser); // Ensure "input" event listener in the remote process
      gDOMWindowUtils.sendContentCommandEvent("insertText", null, "XYZ");
      await waitForBeforeInputEvent;
      await waitForInputEvent;

      await SpecialPowers.spawn(browser, [test], async function (aTest) {
        is(
          content.document.querySelector(aTest.target).innerHTML,
          "abXYZc",
          `The string should be inserted into the focused ${aTest.mode} editor`
        );
      });
    });

    is(
      BrowserSearch.searchBar.textbox.value,
      "",
      "The string should not be inserted into the previously focused search bar"
    );
  }
});
