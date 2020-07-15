"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

/** Test for Bug 545812 **/

// List of key codes which should exit full-screen mode.
const kKeyList = [
  { key: "Escape", keyCode: "VK_ESCAPE", suppressed: true },
  { key: "F11", keyCode: "VK_F11", suppressed: false },
];

const kStrictKeyPressEvents = SpecialPowers.getBoolPref(
  "dom.keyboardevent.keypress.dispatch_non_printable_keys_only_system_group_in_content"
);

function receiveExpectedKeyEvents(aBrowser, aKeyCode, aTrusted) {
  return SpecialPowers.spawn(
    aBrowser,
    [aKeyCode, aTrusted, kStrictKeyPressEvents],
    (keyCode, trusted, strictKeyPressEvents) => {
      return new Promise(resolve => {
        let events =
          strictKeyPressEvents && trusted
            ? ["keydown", "keyup"]
            : ["keydown", "keypress", "keyup"];
        function listener(event) {
          let expected = events.shift();
          Assert.equal(
            event.type,
            expected,
            `Should receive a ${expected} event`
          );
          Assert.equal(
            event.keyCode,
            keyCode,
            `Should receive the event with key code ${keyCode}`
          );
          if (!events.length) {
            content.document.removeEventListener("keydown", listener, true);
            content.document.removeEventListener("keyup", listener, true);
            content.document.removeEventListener("keypress", listener, true);
            resolve();
          }
        }

        content.document.addEventListener("keydown", listener, true);
        content.document.addEventListener("keyup", listener, true);
        content.document.addEventListener("keypress", listener, true);
      });
    }
  );
}

const kPage =
  "http://example.org/browser/" + "dom/html/test/file_fullscreen-api-keys.html";

add_task(async function() {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]
  );

  let tab = BrowserTestUtils.addTab(gBrowser, kPage);
  let browser = tab.linkedBrowser;
  gBrowser.selectedTab = tab;
  registerCleanupFunction(() => gBrowser.removeTab(tab));
  await waitForDocLoadComplete();

  // Wait for the document being actived, so that
  // fullscreen request won't be denied.
  await SpecialPowers.spawn(browser, [], () => {
    return ContentTaskUtils.waitForCondition(
      () => docShell.isActive && content.document.hasFocus(),
      "document is active"
    );
  });

  // Register listener to capture unexpected events
  let keyEventsCount = 0;
  let fullScreenEventsCount = 0;
  let removeFullScreenListener = BrowserTestUtils.addContentEventListener(
    browser,
    "fullscreenchange",
    () => fullScreenEventsCount++
  );
  let removeKeyDownListener = BrowserTestUtils.addContentEventListener(
    browser,
    "keydown",
    () => keyEventsCount++,
    { wantUntrusted: true }
  );
  let removeKeyPressListener = BrowserTestUtils.addContentEventListener(
    browser,
    "keypress",
    () => keyEventsCount++,
    { wantUntrusted: true }
  );
  let removeKeyUpListener = BrowserTestUtils.addContentEventListener(
    browser,
    "keyup",
    () => keyEventsCount++,
    { wantUntrusted: true }
  );

  let expectedFullScreenEventsCount = 0;
  let expectedKeyEventsCount = 0;

  for (let { key, keyCode, suppressed } of kKeyList) {
    let keyCodeValue = KeyEvent["DOM_" + keyCode];
    info(`Test keycode ${key} (${keyCodeValue})`);

    info("Enter fullscreen");
    let state = await SpecialPowers.spawn(browser, [], () => {
      return new Promise(resolve => {
        content.document.addEventListener(
          "fullscreenchange",
          () => {
            resolve(!!content.document.fullscreenElement);
          },
          { once: true }
        );
        content.document.body.requestFullscreen();
      });
    });
    ok(state, "The content should have entered fullscreen");
    ok(document.fullscreenElement, "The chrome should also be in fullscreen");

    is(
      fullScreenEventsCount,
      ++expectedFullScreenEventsCount,
      "correct number of fullscreen events occurred"
    );

    info("Dispatch untrusted key events from content");
    let promiseExpectedKeyEvents = receiveExpectedKeyEvents(
      browser,
      keyCodeValue,
      false
    );

    SpecialPowers.spawn(browser, [keyCode], keyCodeChild => {
      var evt = new content.CustomEvent("Test:DispatchKeyEvents", {
        detail: Cu.cloneInto({ code: keyCodeChild }, content),
      });
      content.dispatchEvent(evt);
    });
    await promiseExpectedKeyEvents;

    expectedKeyEventsCount += 3;
    is(
      keyEventsCount,
      expectedKeyEventsCount,
      "correct number of key events occurred"
    );

    info("Send trusted key events");

    let fullScreenPromise = SpecialPowers.spawn(browser, [], () => {
      return new Promise(resolve => {
        content.document.addEventListener(
          "fullscreenchange",
          () => {
            resolve(!!content.document.fullscreenElement);
          },
          { once: true }
        );
      });
    });

    promiseExpectedKeyEvents = suppressed
      ? Promise.resolve()
      : receiveExpectedKeyEvents(browser, keyCodeValue, true);
    await SpecialPowers.spawn(browser, [], () => {});

    EventUtils.synthesizeKey("KEY_" + key);
    await promiseExpectedKeyEvents;

    state = await fullScreenPromise;
    ok(!state, "The content should have exited fullscreen");
    ok(
      !document.fullscreenElement,
      "The chrome should also have exited fullscreen"
    );

    is(
      fullScreenEventsCount,
      ++expectedFullScreenEventsCount,
      "correct number of fullscreen events occurred"
    );
    if (!suppressed) {
      expectedKeyEventsCount +=
        kStrictKeyPressEvents && keyCode == "VK_F11" ? 2 : 3;
    }
    is(
      keyEventsCount,
      expectedKeyEventsCount,
      "correct number of key events occurred"
    );
  }

  removeFullScreenListener();
  removeKeyDownListener();
  removeKeyPressListener();
  removeKeyUpListener();
});
