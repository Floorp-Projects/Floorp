const PAGE = `
<!doctype html>
<select>
  <option>ABC</option>
  <option>DEFG</option>
</select>
`;

async function openSelectPopup(browser) {
  let popupShownPromise = BrowserTestUtils.waitForSelectPopupShown(window);
  await BrowserTestUtils.synthesizeMouseAtCenter("select", {}, browser);
  return popupShownPromise;
}

function promiseChangeHandlingUserInput(browser) {
  return SpecialPowers.spawn(browser, [], async function() {
    content.document.clearUserGestureActivation();
    let element = content.document.querySelector("select");
    let reply = {};
    function getUserInputState() {
      return {
        isHandlingUserInput: content.window.windowUtils.isHandlingUserInput,
        hasValidTransientUserGestureActivation:
          content.document.hasValidTransientUserGestureActivation,
      };
    }
    reply.before = getUserInputState();
    await ContentTaskUtils.waitForEvent(element, "change", false, () => {
      reply.during = getUserInputState();
      return true;
    });
    await new Promise(r => content.window.setTimeout(r));
    reply.after = getUserInputState();
    return reply;
  });
}

async function testHandlingUserInputOnChange(aTriggerFn) {
  const url = "data:text/html," + encodeURI(PAGE);
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function(browser) {
      let popup = await openSelectPopup(browser);
      let userInputOnChange = promiseChangeHandlingUserInput(browser);
      await aTriggerFn(popup);
      let userInput = await userInputOnChange;
      ok(
        !userInput.before.isHandlingUserInput,
        "Shouldn't be handling user input before test"
      );
      ok(
        !userInput.before.hasValidTransientUserGestureActivation,
        "transient activation should be cleared before test"
      );
      ok(
        userInput.during.hasValidTransientUserGestureActivation,
        "should provide transient activation during event"
      );
      ok(
        userInput.during.isHandlingUserInput,
        "isHandlingUserInput should be true during event"
      );
      ok(
        userInput.after.hasValidTransientUserGestureActivation,
        "should provide transient activation after event"
      );
      ok(
        !userInput.after.isHandlingUserInput,
        "isHandlingUserInput should be false after event"
      );
    }
  );
}

// This test checks if the change/click event is considered as user input event.
add_task(async function test_handling_user_input_key() {
  return testHandlingUserInputOnChange(async function(popup) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    await hideSelectPopup();
  });
});

add_task(async function test_handling_user_input_click() {
  return testHandlingUserInputOnChange(async function(popup) {
    EventUtils.synthesizeMouseAtCenter(popup.lastElementChild, {});
  });
});

add_task(async function test_handling_user_input_click() {
  return testHandlingUserInputOnChange(async function(popup) {
    EventUtils.synthesizeMouseAtCenter(popup.lastElementChild, {});
  });
});
