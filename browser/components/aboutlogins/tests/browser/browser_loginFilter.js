/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  const aboutLoginsTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(aboutLoginsTab);
  });
});

add_task(async function focus_filter_by_ctrl_f() {
  const browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    function getActiveElement() {
      let element = content.document.activeElement;

      while (element?.shadowRoot) {
        element = element.shadowRoot.activeElement;
      }

      return element;
    }

    //// State after load

    const loginFilter = content.document
      .querySelector("login-list")
      .shadowRoot.querySelector("login-filter")
      .shadowRoot.querySelector("input");
    Assert.equal(
      getActiveElement(),
      loginFilter,
      "login filter must be focused after opening about:logins"
    );

    //// Focus something else (Create Login button)

    content.document
      .querySelector("login-list")
      .shadowRoot.querySelector("create-login-button")
      .shadowRoot.querySelector("button")
      .focus();
    Assert.notEqual(
      getActiveElement(),
      loginFilter,
      "login filter is not focused"
    );

    //// Ctrl+F key

    EventUtils.synthesizeKey("f", { accelKey: true }, content);
    Assert.equal(
      getActiveElement(),
      loginFilter,
      "Ctrl+F/Cmd+F focused login filter"
    );
  });
});
