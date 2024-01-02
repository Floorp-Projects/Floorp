add_task(async function test() {
  const kTestURI =
    "data:text/html," +
    '<script type="text/javascript">' +
    "  function onMouseDown(aEvent) {" +
    "    document.getElementById('willBeFocused').focus();" +
    "    aEvent.preventDefault();" +
    "  }" +
    "</script>" +
    '<body id="body">' +
    '<button onmousedown="onMouseDown(event);" style="width: 100px; height: 100px;">click here</button>' +
    '<input id="willBeFocused"></body>';

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kTestURI);

  let fm = Services.focus;

  for (var button = 0; button < 3; button++) {
    // Set focus to a chrome element before synthesizing a mouse down event.
    gURLBar.focus();

    is(
      fm.focusedElement,
      gURLBar.inputField,
      "Failed to move focus to search bar: button=" + button
    );

    // We intentionally turn off this a11y check, because the following click
    // is sent on an arbitrary web content that is not expected to be tested
    // by itself with the browser mochitests, therefore this rule check shall
    // be ignored by a11y-checks suite.
    AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
    // Synthesize mouse down event on browser object over the button, such that
    // the event propagates through both processes.
    EventUtils.synthesizeMouse(tab.linkedBrowser, 20, 20, { button });
    AccessibilityUtils.resetEnv();

    isnot(
      fm.focusedElement,
      gURLBar.inputField,
      "Failed to move focus away from search bar: button=" + button
    );

    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [button],
      async function (button) {
        let fm = Services.focus;

        let attempts = 10;
        await new Promise(resolve => {
          function check() {
            if (
              attempts > 0 &&
              content.document.activeElement.id != "willBeFocused"
            ) {
              attempts--;
              content.window.setTimeout(check, 100);
              return;
            }

            Assert.equal(
              content.document.activeElement.id,
              "willBeFocused",
              "The input element isn't active element: button=" + button
            );
            Assert.equal(
              fm.focusedElement,
              content.document.activeElement,
              "The active element isn't focused element in App level: button=" +
                button
            );
            resolve();
          }
          check();
        });
      }
    );
  }

  gBrowser.removeTab(tab);
});
