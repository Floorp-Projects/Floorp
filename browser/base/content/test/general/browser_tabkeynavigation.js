/*
 * This test checks that keyboard navigation for tabs isn't blocked by content
 */
add_task(function* test() {

  let testPage1 = "data:text/html,<html id='tab1'><body><button id='button1'>Tab 1</button></body></html>";
  let testPage2 = "data:text/html,<html id='tab2'><body><button id='button2'>Tab 2</button><script>function preventDefault(event) { event.preventDefault(); event.stopImmediatePropagation(); } window.addEventListener('keydown', preventDefault, true); window.addEventListener('keypress', preventDefault, true);</script></body></html>";
  let testPage3 = "data:text/html,<html id='tab3'><body><button id='button3'>Tab 3</button></body></html>";

  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, testPage1);
  let browser1 = gBrowser.getBrowserForTab(tab1);
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, testPage2);
  let tab3 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, testPage3);

  // Kill the animation for simpler test.
  Services.prefs.setBoolPref("toolkit.cosmeticAnimations.enabled", false);

  gBrowser.selectedTab = tab1;
  browser1.focus();

  is(gBrowser.selectedTab, tab1,
     "Tab1 should be activated");
  EventUtils.synthesizeKey("VK_TAB", { ctrlKey: true });
  is(gBrowser.selectedTab, tab2,
     "Tab2 should be activated by pressing Ctrl+Tab on Tab1");

  EventUtils.synthesizeKey("VK_TAB", { ctrlKey: true });
  is(gBrowser.selectedTab, tab3,
     "Tab3 should be activated by pressing Ctrl+Tab on Tab2");

  EventUtils.synthesizeKey("VK_TAB", { ctrlKey: true, shiftKey: true });
  is(gBrowser.selectedTab, tab2,
     "Tab2 should be activated by pressing Ctrl+Shift+Tab on Tab3");

  EventUtils.synthesizeKey("VK_TAB", { ctrlKey: true, shiftKey: true });
  is(gBrowser.selectedTab, tab1,
     "Tab1 should be activated by pressing Ctrl+Shift+Tab on Tab2");

  gBrowser.selectedTab = tab1;
  browser1.focus();

  is(gBrowser.selectedTab, tab1,
     "Tab1 should be activated");
  EventUtils.synthesizeKey("VK_PAGE_DOWN", { ctrlKey: true });
  is(gBrowser.selectedTab, tab2,
     "Tab2 should be activated by pressing Ctrl+PageDown on Tab1");

  EventUtils.synthesizeKey("VK_PAGE_DOWN", { ctrlKey: true });
  is(gBrowser.selectedTab, tab3,
     "Tab3 should be activated by pressing Ctrl+PageDown on Tab2");

  EventUtils.synthesizeKey("VK_PAGE_UP", { ctrlKey: true });
  is(gBrowser.selectedTab, tab2,
     "Tab2 should be activated by pressing Ctrl+PageUp on Tab3");

  EventUtils.synthesizeKey("VK_PAGE_UP", { ctrlKey: true });
  is(gBrowser.selectedTab, tab1,
     "Tab1 should be activated by pressing Ctrl+PageUp on Tab2");

  if (gBrowser.mTabBox._handleMetaAltArrows) {
    gBrowser.selectedTab = tab1;
    browser1.focus();

    let ltr = window.getComputedStyle(gBrowser.mTabBox).direction == "ltr";
    let advanceKey = ltr ? "VK_RIGHT" : "VK_LEFT";
    let reverseKey = ltr ? "VK_LEFT" : "VK_RIGHT";

    is(gBrowser.selectedTab, tab1,
       "Tab1 should be activated");
    EventUtils.synthesizeKey(advanceKey, { altKey: true, metaKey: true });
    is(gBrowser.selectedTab, tab2,
       "Tab2 should be activated by pressing Ctrl+" + advanceKey + " on Tab1");

    EventUtils.synthesizeKey(advanceKey, { altKey: true, metaKey: true });
    is(gBrowser.selectedTab, tab3,
       "Tab3 should be activated by pressing Ctrl+" + advanceKey + " on Tab2");

    EventUtils.synthesizeKey(reverseKey, { altKey: true, metaKey: true });
    is(gBrowser.selectedTab, tab2,
       "Tab2 should be activated by pressing Ctrl+" + reverseKey + " on Tab3");

    EventUtils.synthesizeKey(reverseKey, { altKey: true, metaKey: true });
    is(gBrowser.selectedTab, tab1,
       "Tab1 should be activated by pressing Ctrl+" + reverseKey + " on Tab2");
  }

  gBrowser.selectedTab = tab2;
  is(gBrowser.selectedTab, tab2,
      "Tab2 should be activated");
  is(gBrowser.tabContainer.selectedIndex, 2,
     "Tab2 index should be 2");

  EventUtils.synthesizeKey("VK_PAGE_DOWN", { ctrlKey: true, shiftKey: true });
  is(gBrowser.selectedTab, tab2,
     "Tab2 should be activated after Ctrl+Shift+PageDown");
  is(gBrowser.tabContainer.selectedIndex, 3,
     "Tab2 index should be 1 after Ctrl+Shift+PageDown");

  EventUtils.synthesizeKey("VK_PAGE_UP", { ctrlKey: true, shiftKey: true });
  is(gBrowser.selectedTab, tab2,
     "Tab2 should be activated after Ctrl+Shift+PageUp");
  is(gBrowser.tabContainer.selectedIndex, 2,
     "Tab2 index should be 2 after Ctrl+Shift+PageUp");

  if (navigator.platform.indexOf("Mac") == 0) {
    gBrowser.selectedTab = tab1;
    browser1.focus();

    // XXX Currently, Command + "{" and "}" don't work if keydown event is
    //     consumed because following keypress event isn't fired.

    let ltr = window.getComputedStyle(gBrowser.mTabBox).direction == "ltr";
    let advanceKey = ltr ? "}" : "{";
    let reverseKey = ltr ? "{" : "}";

    is(gBrowser.selectedTab, tab1,
       "Tab1 should be activated");

    EventUtils.synthesizeKey(advanceKey, { metaKey: true });
    is(gBrowser.selectedTab, tab2,
       "Tab2 should be activated by pressing Ctrl+" + advanceKey + " on Tab1");

    EventUtils.synthesizeKey(advanceKey, { metaKey: true });
    is(gBrowser.selectedTab, tab3,
       "Tab3 should be activated by pressing Ctrl+" + advanceKey + " on Tab2");

    EventUtils.synthesizeKey(reverseKey, { metaKey: true });
    is(gBrowser.selectedTab, tab2,
       "Tab2 should be activated by pressing Ctrl+" + reverseKey + " on Tab3");

    EventUtils.synthesizeKey(reverseKey, { metaKey: true });
    is(gBrowser.selectedTab, tab1,
       "Tab1 should be activated by pressing Ctrl+" + reverseKey + " on Tab2");
  } else {
    gBrowser.selectedTab = tab2;
    EventUtils.synthesizeKey("VK_F4", { type: "keydown", ctrlKey: true });

    isnot(gBrowser.selectedTab, tab2,
          "Tab2 should be closed by pressing Ctrl+F4 on Tab2");
    is(gBrowser.tabs.length, 3,
      "The count of tabs should be 3 since tab2 should be closed");

    // NOTE: keypress event shouldn't be fired since the keydown event should
    //       be consumed by tab2.
      EventUtils.synthesizeKey("VK_F4", { type: "keyup", ctrlKey: true });
      is(gBrowser.tabs.length, 3,
        "The count of tabs should be 3 since renaming key events shouldn't close other tabs");
  }

  gBrowser.selectedTab = tab3;
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }

    Services.prefs.clearUserPref("toolkit.cosmeticAnimations.enabled");
});
