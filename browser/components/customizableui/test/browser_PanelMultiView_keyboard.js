/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the keyboard behavior of PanelViews.
 */

const { PanelMultiView } = ChromeUtils.import(
  "resource:///modules/PanelMultiView.jsm"
);
const kEmbeddedDocUrl =
  'data:text/html,<textarea id="docTextarea">value</textarea><button id="docButton"></button>';

let gAnchor;
let gPanel;
let gPanelMultiView;
let gMainView;
let gMainContext;
let gMainButton1;
let gMainMenulist;
let gMainRadiogroup;
let gMainTextbox;
let gMainButton2;
let gMainButton3;
let gCheckbox;
let gMainTabOrder;
let gMainArrowOrder;
let gSubView;
let gSubButton;
let gSubTextarea;
let gBrowserView;
let gBrowserBrowser;
let gIframeView;
let gIframeIframe;

async function openPopup() {
  let shown = BrowserTestUtils.waitForEvent(gMainView, "ViewShown");
  PanelMultiView.openPopup(gPanel, gAnchor, "bottomcenter topright");
  await shown;
}

async function hidePopup() {
  let hidden = BrowserTestUtils.waitForEvent(gPanel, "popuphidden");
  PanelMultiView.hidePopup(gPanel);
  await hidden;
}

async function showSubView(view = gSubView) {
  let shown = BrowserTestUtils.waitForEvent(view, "ViewShown");
  // We must show with an anchor so the Back button is generated.
  gPanelMultiView.showSubView(view, gMainButton1);
  await shown;
}

async function expectFocusAfterKey(aKey, aFocus) {
  let res = aKey.match(/^(Shift\+)?(.+)$/);
  let shift = Boolean(res[1]);
  let key;
  if (res[2].length == 1) {
    key = res[2]; // Character.
  } else {
    key = "KEY_" + res[2]; // Tab, ArrowRight, etc.
  }
  info("Waiting for focus on " + aFocus.id);
  let focused = BrowserTestUtils.waitForEvent(aFocus, "focus");
  EventUtils.synthesizeKey(key, { shiftKey: shift });
  await focused;
  ok(true, aFocus.id + " focused after " + aKey + " pressed");
}

add_task(async function setup() {
  // This shouldn't be necessary - but it is, because we use same-process frames.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1565276 covers improving this.
  await SpecialPowers.pushPrefEnv({
    set: [["security.allow_unsafe_parent_loads", true]],
  });
  let navBar = document.getElementById("nav-bar");
  gAnchor = document.createXULElement("toolbarbutton");
  navBar.appendChild(gAnchor);
  gPanel = document.createXULElement("panel");
  navBar.appendChild(gPanel);
  gPanelMultiView = document.createXULElement("panelmultiview");
  gPanelMultiView.setAttribute("mainViewId", "testMainView");
  gPanel.appendChild(gPanelMultiView);

  gMainView = document.createXULElement("panelview");
  gMainView.id = "testMainView";
  gPanelMultiView.appendChild(gMainView);
  gMainContext = document.createXULElement("menupopup");
  gMainContext.id = "gMainContext";
  gMainView.appendChild(gMainContext);
  gMainContext.appendChild(document.createXULElement("menuitem"));
  gMainButton1 = document.createXULElement("button");
  gMainButton1.id = "gMainButton1";
  gMainView.appendChild(gMainButton1);
  // We use this for anchoring subviews, so it must have a label.
  gMainButton1.setAttribute("label", "gMainButton1");
  gMainButton1.setAttribute("context", "gMainContext");
  gMainMenulist = document.createXULElement("menulist");
  gMainMenulist.id = "gMainMenulist";
  gMainView.appendChild(gMainMenulist);
  let menuPopup = document.createXULElement("menupopup");
  gMainMenulist.appendChild(menuPopup);
  let item = document.createXULElement("menuitem");
  item.setAttribute("value", "1");
  item.setAttribute("selected", "true");
  menuPopup.appendChild(item);
  item = document.createXULElement("menuitem");
  item.setAttribute("value", "2");
  menuPopup.appendChild(item);
  gMainRadiogroup = document.createXULElement("radiogroup");
  gMainRadiogroup.id = "gMainRadiogroup";
  gMainView.appendChild(gMainRadiogroup);
  let radio = document.createXULElement("radio");
  radio.setAttribute("value", "1");
  radio.setAttribute("selected", "true");
  gMainRadiogroup.appendChild(radio);
  radio = document.createXULElement("radio");
  radio.setAttribute("value", "2");
  gMainRadiogroup.appendChild(radio);
  gMainTextbox = document.createElement("input");
  gMainTextbox.id = "gMainTextbox";
  gMainView.appendChild(gMainTextbox);
  gMainTextbox.setAttribute("value", "value");
  gMainButton2 = document.createXULElement("button");
  gMainButton2.id = "gMainButton2";
  gMainView.appendChild(gMainButton2);
  gMainButton3 = document.createXULElement("button");
  gMainButton3.id = "gMainButton3";
  gMainView.appendChild(gMainButton3);
  gCheckbox = document.createXULElement("checkbox");
  gCheckbox.id = "gCheckbox";
  gMainView.appendChild(gCheckbox);
  gMainTabOrder = [
    gMainButton1,
    gMainMenulist,
    gMainRadiogroup,
    gMainTextbox,
    gMainButton2,
    gMainButton3,
    gCheckbox,
  ];
  gMainArrowOrder = [gMainButton1, gMainButton2, gMainButton3, gCheckbox];

  gSubView = document.createXULElement("panelview");
  gSubView.id = "testSubView";
  gPanelMultiView.appendChild(gSubView);
  gSubButton = document.createXULElement("button");
  gSubView.appendChild(gSubButton);
  gSubTextarea = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "textarea"
  );
  gSubTextarea.id = "gSubTextarea";
  gSubView.appendChild(gSubTextarea);
  gSubTextarea.value = "value";

  gBrowserView = document.createXULElement("panelview");
  gBrowserView.id = "testBrowserView";
  gPanelMultiView.appendChild(gBrowserView);
  gBrowserBrowser = document.createXULElement("browser");
  gBrowserBrowser.id = "GBrowserBrowser";
  gBrowserBrowser.setAttribute("type", "content");
  gBrowserBrowser.setAttribute("src", kEmbeddedDocUrl);
  gBrowserBrowser.setAttribute("width", 100);
  gBrowserBrowser.setAttribute("height", 100);
  gBrowserView.appendChild(gBrowserBrowser);

  gIframeView = document.createXULElement("panelview");
  gIframeView.id = "testIframeView";
  gPanelMultiView.appendChild(gIframeView);
  gIframeIframe = document.createXULElement("iframe");
  gIframeIframe.id = "gIframeIframe";
  gIframeIframe.setAttribute("src", kEmbeddedDocUrl);
  gIframeView.appendChild(gIframeIframe);

  registerCleanupFunction(() => {
    gAnchor.remove();
    gPanel.remove();
  });
});

// Test that the tab key focuses all expected controls.
add_task(async function testTab() {
  await openPopup();
  for (let elem of gMainTabOrder) {
    await expectFocusAfterKey("Tab", elem);
  }
  // Wrap around.
  await expectFocusAfterKey("Tab", gMainTabOrder[0]);
  await hidePopup();
});

// Test that the shift+tab key focuses all expected controls.
add_task(async function testShiftTab() {
  await openPopup();
  for (let i = gMainTabOrder.length - 1; i >= 0; --i) {
    await expectFocusAfterKey("Shift+Tab", gMainTabOrder[i]);
  }
  // Wrap around.
  await expectFocusAfterKey(
    "Shift+Tab",
    gMainTabOrder[gMainTabOrder.length - 1]
  );
  await hidePopup();
});

// Test that the down arrow key skips menulists and textboxes.
add_task(async function testDownArrow() {
  await openPopup();
  for (let elem of gMainArrowOrder) {
    await expectFocusAfterKey("ArrowDown", elem);
  }
  // Wrap around.
  await expectFocusAfterKey("ArrowDown", gMainArrowOrder[0]);
  await hidePopup();
});

// Test that the up arrow key skips menulists and textboxes.
add_task(async function testUpArrow() {
  await openPopup();
  for (let i = gMainArrowOrder.length - 1; i >= 0; --i) {
    await expectFocusAfterKey("ArrowUp", gMainArrowOrder[i]);
  }
  // Wrap around.
  await expectFocusAfterKey(
    "ArrowUp",
    gMainArrowOrder[gMainArrowOrder.length - 1]
  );
  await hidePopup();
});

// Test that the home/end keys move to the first/last controls.
add_task(async function testHomeEnd() {
  await openPopup();
  await expectFocusAfterKey("Home", gMainArrowOrder[0]);
  await expectFocusAfterKey("End", gMainArrowOrder[gMainArrowOrder.length - 1]);
  await hidePopup();
});

// Test that the up/down arrow keys work as expected in menulists.
add_task(async function testArrowsMenulist() {
  await openPopup();
  gMainMenulist.focus();
  is(document.activeElement, gMainMenulist, "menulist focused");
  is(gMainMenulist.value, "1", "menulist initial value 1");
  if (AppConstants.platform == "macosx") {
    // On Mac, down/up arrows just open the menulist.
    let popup = gMainMenulist.menupopup;
    for (let key of ["ArrowDown", "ArrowUp"]) {
      let shown = BrowserTestUtils.waitForEvent(popup, "popupshown");
      EventUtils.synthesizeKey("KEY_" + key);
      await shown;
      ok(gMainMenulist.open, "menulist open after " + key);
      let hidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
      EventUtils.synthesizeKey("KEY_Escape");
      await hidden;
      ok(!gMainMenulist.open, "menulist closed after Escape");
    }
  } else {
    // On other platforms, down/up arrows change the value without opening the
    // menulist.
    EventUtils.synthesizeKey("KEY_ArrowDown");
    is(
      document.activeElement,
      gMainMenulist,
      "menulist still focused after ArrowDown"
    );
    is(gMainMenulist.value, "2", "menulist value 2 after ArrowDown");
    EventUtils.synthesizeKey("KEY_ArrowUp");
    is(
      document.activeElement,
      gMainMenulist,
      "menulist still focused after ArrowUp"
    );
    is(gMainMenulist.value, "1", "menulist value 1 after ArrowUp");
  }
  await hidePopup();
});

// Test that the tab key closes an open menu list.
add_task(async function testTabOpenMenulist() {
  await openPopup();
  gMainMenulist.focus();
  is(document.activeElement, gMainMenulist, "menulist focused");
  let popup = gMainMenulist.menupopup;
  let shown = BrowserTestUtils.waitForEvent(popup, "popupshown");
  gMainMenulist.open = true;
  await shown;
  ok(gMainMenulist.open, "menulist open");
  let menuHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  let panelHidden = BrowserTestUtils.waitForEvent(gPanel, "popuphidden");
  EventUtils.synthesizeKey("KEY_Tab");
  await menuHidden;
  ok(!gMainMenulist.open, "menulist closed after Tab");
  // Tab in an open menulist closes the menulist, but also dismisses the panel
  // above it (bug 1566673). So, we just wait for the panel to hide rather than
  // using hidePopup().
  await panelHidden;
});

if (AppConstants.platform == "macosx") {
  // Test that using the mouse to open a menulist still allows keyboard navigation
  // inside it.
  add_task(async function testNavigateMouseOpenedMenulist() {
    await openPopup();
    let popup = gMainMenulist.menupopup;
    let shown = BrowserTestUtils.waitForEvent(popup, "popupshown");
    gMainMenulist.open = true;
    await shown;
    ok(gMainMenulist.open, "menulist open");
    let oldFocus = document.activeElement;
    let oldSelectedItem = gMainMenulist.selectedItem;
    ok(
      oldSelectedItem.hasAttribute("_moz-menuactive"),
      "Selected item should show up as active"
    );
    EventUtils.synthesizeKey("KEY_ArrowDown");
    await TestUtils.waitForCondition(
      () => !oldSelectedItem.hasAttribute("_moz-menuactive")
    );
    is(oldFocus, document.activeElement, "Focus should not move on mac");
    ok(
      !oldSelectedItem.hasAttribute("_moz-menuactive"),
      "Selected item should change"
    );

    let menuHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
    let panelHidden = BrowserTestUtils.waitForEvent(gPanel, "popuphidden");
    EventUtils.synthesizeKey("KEY_Tab");
    await menuHidden;
    ok(!gMainMenulist.open, "menulist closed after Tab");
    // Tab in an open menulist closes the menulist, but also dismisses the panel
    // above it (bug 1566673). So, we just wait for the panel to hide rather than
    // using hidePopup().
    await panelHidden;
  });
}

// Test that the up/down arrow keys work as expected in radiogroups.
add_task(async function testArrowsRadiogroup() {
  await openPopup();
  gMainRadiogroup.focus();
  is(document.activeElement, gMainRadiogroup, "radiogroup focused");
  is(gMainRadiogroup.value, "1", "radiogroup initial value 1");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    document.activeElement,
    gMainRadiogroup,
    "radiogroup still focused after ArrowDown"
  );
  is(gMainRadiogroup.value, "2", "radiogroup value 2 after ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    document.activeElement,
    gMainRadiogroup,
    "radiogroup still focused after ArrowUp"
  );
  is(gMainRadiogroup.value, "1", "radiogroup value 1 after ArrowUp");
  await hidePopup();
});

// Test that pressing space in a textbox inserts a space (instead of trying to
// activate the control).
add_task(async function testSpaceTextbox() {
  await openPopup();
  gMainTextbox.focus();
  gMainTextbox.selectionStart = gMainTextbox.selectionEnd = 0;
  EventUtils.synthesizeKey(" ");
  is(gMainTextbox.value, " value", "Space typed into textbox");
  gMainTextbox.value = "value";
  await hidePopup();
});

// Tests that the left arrow key normally moves back to the previous view.
add_task(async function testLeftArrow() {
  await openPopup();
  await showSubView();
  let shown = BrowserTestUtils.waitForEvent(gMainView, "ViewShown");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  await shown;
  ok("Moved to previous view after ArrowLeft");
  await hidePopup();
});

// Tests that the left arrow key moves the caret in a textarea in a subview
// (instead of going back to the previous view).
add_task(async function testLeftArrowTextarea() {
  await openPopup();
  await showSubView();
  gSubTextarea.focus();
  is(document.activeElement, gSubTextarea, "textarea focused");
  EventUtils.synthesizeKey("KEY_End");
  is(gSubTextarea.selectionStart, 5, "selectionStart 5 after End");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  is(gSubTextarea.selectionStart, 4, "selectionStart 4 after ArrowLeft");
  is(document.activeElement, gSubTextarea, "textarea still focused");
  await hidePopup();
});

// Test navigation to a button which is initially disabled and later enabled.
add_task(async function testDynamicButton() {
  gMainButton2.disabled = true;
  await openPopup();
  await expectFocusAfterKey("ArrowDown", gMainButton1);
  await expectFocusAfterKey("ArrowDown", gMainButton3);
  gMainButton2.disabled = false;
  await expectFocusAfterKey("ArrowUp", gMainButton2);
  await hidePopup();
});

add_task(async function testActivation() {
  function checkActivated(elem, activationFn, reason) {
    let activated = false;
    elem.onclick = function() {
      activated = true;
    };
    activationFn();
    ok(activated, "Should have activated button after " + reason);
    elem.onclick = null;
  }
  await openPopup();
  await expectFocusAfterKey("ArrowDown", gMainButton1);
  checkActivated(
    gMainButton1,
    () => EventUtils.synthesizeKey("KEY_Enter"),
    "pressing enter"
  );
  checkActivated(
    gMainButton1,
    () => EventUtils.synthesizeKey(" "),
    "pressing space"
  );
  checkActivated(
    gMainButton1,
    () => EventUtils.synthesizeKey("KEY_Enter", { code: "NumpadEnter" }),
    "pressing numpad enter"
  );
  await hidePopup();
});

// Test that keyboard activation works for buttons responding to mousedown
// events (instead of command or click). The Library button does this, for
// example.
add_task(async function testActivationMousedown() {
  await openPopup();
  await expectFocusAfterKey("ArrowDown", gMainButton1);
  let activated = false;
  gMainButton1.onmousedown = function() {
    activated = true;
  };
  EventUtils.synthesizeKey(" ");
  ok(activated, "mousedown activated after space");
  gMainButton1.onmousedown = null;
  await hidePopup();
});

// Test that tab and the arrow keys aren't overridden in embedded documents.
async function testTabArrowsEmbeddedDoc(aView, aEmbedder) {
  await openPopup();
  await showSubView(aView);
  let doc = aEmbedder.contentDocument;
  if (doc.readyState != "complete" || doc.location.href != kEmbeddedDocUrl) {
    info(`Embedded doc readyState ${doc.readyState}, location ${doc.location}`);
    info("Waiting for load on embedder");
    // Browsers don't fire load events, and iframes don't fire load events in
    // typeChrome windows. We can handle both by using a capturing event
    // listener to capture the load event from the child document.
    await BrowserTestUtils.waitForEvent(aEmbedder, "load", true);
    // The original doc might have been a temporary about:blank, so fetch it
    // again.
    doc = aEmbedder.contentDocument;
  }
  is(doc.location.href, kEmbeddedDocUrl, "Embedded doc has correct URl");
  let backButton = aView.querySelector(".subviewbutton-back");
  backButton.id = "docBack";
  await expectFocusAfterKey("Tab", backButton);
  // Documents don't have an id property, but expectFocusAfterKey wants one.
  doc.id = "doc";
  await expectFocusAfterKey("Tab", doc);
  // Make sure tab/arrows aren't overridden within the embedded document.
  let textarea = doc.getElementById("docTextarea");
  // Tab should really focus the textarea, but default tab handling seems to
  // skip everything inside the embedder element when run in this test. This
  // behaves as expected in real panels, though. Force focus to the textarea
  // and then test from there.
  textarea.focus();
  is(doc.activeElement, textarea, "textarea focused");
  is(textarea.selectionStart, 0, "selectionStart initially 0");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  is(textarea.selectionStart, 1, "selectionStart 1 after ArrowRight");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  is(textarea.selectionStart, 0, "selectionStart 0 after ArrowLeft");
  is(doc.activeElement, textarea, "textarea still focused");
  let docButton = doc.getElementById("docButton");
  await expectFocusAfterKey("Tab", docButton);
  await hidePopup();
}

// Test that tab and the arrow keys aren't overridden in embedded browsers.
add_task(async function testTabArrowsBrowser() {
  await testTabArrowsEmbeddedDoc(gBrowserView, gBrowserBrowser);
});

// Test that tab and the arrow keys aren't overridden in embedded iframes.
add_task(async function testTabArrowsIframe() {
  await testTabArrowsEmbeddedDoc(gIframeView, gIframeIframe);
});

// Test that the arrow keys aren't overridden in context menus.
add_task(async function testArowsContext() {
  await openPopup();
  await expectFocusAfterKey("ArrowDown", gMainButton1);
  let shown = BrowserTestUtils.waitForEvent(gMainContext, "popupshown");
  // There's no cross-platform way to open a context menu from the keyboard.
  gMainContext.openPopup();
  await shown;
  let item = gMainContext.children[0];
  ok(
    !item.getAttribute("_moz-menuactive"),
    "First context menu item initially inactive"
  );
  let active = BrowserTestUtils.waitForEvent(item, "DOMMenuItemActive");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await active;
  ok(
    item.getAttribute("_moz-menuactive"),
    "First context menu item active after ArrowDown"
  );
  is(
    document.activeElement,
    gMainButton1,
    "gMainButton1 still focused after ArrowDown"
  );
  let hidden = BrowserTestUtils.waitForEvent(gMainContext, "popuphidden");
  gMainContext.hidePopup();
  await hidden;
  await hidePopup();
});
