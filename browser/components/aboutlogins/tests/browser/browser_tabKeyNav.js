/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_tab_key_nav() {
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    const EventUtils = ContentTaskUtils.getEventUtils(content);

    async function tab() {
      EventUtils.synthesizeKey("KEY_Tab", {}, content);
      await new Promise(resolve => content.requestAnimationFrame(resolve));
      // The following line can help with focus trap debugging:
      // await new Promise(resolve => content.window.setTimeout(resolve, 100));
    }

    // Getting focused shadow DOM element itself instead of shadowRoot,
    // using recursion for any component-nesting level, as in:
    // document.activeElement.shadowRoot.activeElement.shadowRoot.activeElement
    function getFocusedEl() {
      let el = content.document.activeElement;
      const getShadowRootFocus = e => {
        if (e.shadowRoot) {
          return getShadowRootFocus(e.shadowRoot.activeElement);
        }
        return e;
      };
      return getShadowRootFocus(el);
    }

    // We’re making two passes with focused and focusedAgain sets of DOM elements,
    // rather than just checking we get back to the first focused element,
    // so we can cover more edge-cases involving issues with focus tree.
    const focused = new Set();
    const focusedAgain = new Set();

    // Initializing: some element is supposed to be automatically focused
    const firstEl = getFocusedEl();
    focused.add(firstEl);

    // Setting a maximum number of keypresses to escape the loop,
    // should we fall into a focus trap.
    // Fixed amount of keypresses also helps assess consistency with keyboard navigation.
    const maxKeypresses = content.document.getElementsByTagName("*").length * 2;
    let keypresses = 1;

    while (focusedAgain.size < focused.size && keypresses <= maxKeypresses) {
      await tab();
      keypresses++;
      let el = getFocusedEl();

      // The following block is needed to get back to document
      // after tabbing to browser chrome.
      // This focus trap in browser chrome is a testing artifact we’re fixing below.
      if (el.tagName === "BODY" && el !== firstEl) {
        firstEl.focus();
        await new Promise(resolve => content.requestAnimationFrame(resolve));
        el = getFocusedEl();
      }

      if (focused.has(el)) {
        focusedAgain.add(el);
      } else {
        focused.add(el);
      }
    }

    is(
      focusedAgain.size,
      focused.size,
      "All focusable elements should be kept accessible with TAB key (no focus trap)."
    );
  });
});
