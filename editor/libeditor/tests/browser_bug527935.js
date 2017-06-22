add_task(async function() {
  await new Promise(resolve => waitForFocus(resolve, window));

  const kPageURL = "http://example.org/browser/editor/libeditor/tests/bug527935.html";
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: kPageURL
  }, async function(aBrowser) {
    var popupShown = false;
    function listener() {
      popupShown = true;
    }
    SpecialPowers.addAutoCompletePopupEventListener(window, "popupshowing", listener);

    await ContentTask.spawn(aBrowser, {}, async function() {
      var window = content.window.wrappedJSObject;
      var document = window.document;
      var formTarget = document.getElementById("formTarget");
      var initValue = document.getElementById("initValue");

      window.loadPromise = new Promise(resolve => {
        formTarget.onload = resolve;
      });

      initValue.focus();
      initValue.value = "foo";
    });

    EventUtils.synthesizeKey("VK_RETURN", {});

    await ContentTask.spawn(aBrowser, {}, async function() {
      var window = content.window.wrappedJSObject;
      var document = window.document;

      await window.loadPromise;

      var newInput = document.createElement("input");
      newInput.setAttribute("name", "test");
      document.body.appendChild(newInput);

      var event = document.createEvent("KeyboardEvent");

      event.initKeyEvent("keypress", true, true, null, false, false,
                         false, false, 0, "f".charCodeAt(0));
      newInput.value = "";
      newInput.focus();
      newInput.dispatchEvent(event);
    });

    await new Promise(resolve => hitEventLoop(resolve, 100));

    ok(!popupShown, "Popup must not be opened");
    SpecialPowers.removeAutoCompletePopupEventListener(window, "popupshowing", listener);
  });
});

function hitEventLoop(func, times) {
  if (times > 0) {
    setTimeout(hitEventLoop, 0, func, times - 1);
  } else {
    setTimeout(func, 0);
  }
}
