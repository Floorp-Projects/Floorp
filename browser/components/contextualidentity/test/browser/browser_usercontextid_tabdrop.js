"use strict";

let ChromeUtils = {};
Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

/**
 * Dragging an URL to a tab without userContextId set.
 */
add_task(function* () {
  let tab = gBrowser.addTab("http://example.com/");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let awaitDrop = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "drop");
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://test1.example.com/");

  // A drop type of "link" onto an existing tab would normally trigger a
  // load in that same tab, but tabbrowser code in _getDragTargetTab treats
  // drops on the outer edges of a tab differently (loading a new tab
  // instead). Make events created by synthesizeDrop have all of their
  // coordinates set to 0 (screenX/screenY), so they're treated as drops
  // on the outer edge of the tab, thus they open new tabs.
  let event = {
    clientX: 0,
    clientY: 0,
    screenX: 0,
    screenY: 0,
  };
  ChromeUtils.synthesizeDrop(tab, tab, [[{type: "text/plain", data: "http://test1.example.com/"}]], "link", window, undefined, event);

  yield awaitDrop;

  let tab2 = yield newTabPromise;
  Assert.ok(!tab2.hasAttribute("usercontextid"), "Tab shouldn't have usercontextid attribute");

  yield BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  yield ContentTask.spawn(tab2.linkedBrowser, {}, function* () {
    Assert.equal(content.document.documentURI, "http://test1.example.com/");
    Assert.equal(content.document.nodePrincipal.originAttributes.userContextId, 0);
  });

  yield BrowserTestUtils.removeTab(tab);
  yield BrowserTestUtils.removeTab(tab2);
});

/**
 * When dragging an URL to a new tab, the new tab should have the same
 * userContextId as the original tab.
 */
add_task(function* () {
  let tab = gBrowser.addTab("http://example.com/", {userContextId: 1});
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let awaitDrop = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "drop");
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://test1.example.com/");

  // A drop type of "link" onto an existing tab would normally trigger a
  // load in that same tab, but tabbrowser code in _getDragTargetTab treats
  // drops on the outer edges of a tab differently (loading a new tab
  // instead). Make events created by synthesizeDrop have all of their
  // coordinates set to 0 (screenX/screenY), so they're treated as drops
  // on the outer edge of the tab, thus they open new tabs.
  let event = {
    clientX: 0,
    clientY: 0,
    screenX: 0,
    screenY: 0,
  };
  ChromeUtils.synthesizeDrop(tab, tab, [[{type: "text/plain", data: "http://test1.example.com/"}]], "link", window, undefined, event);

  yield awaitDrop;

  let tab2 = yield newTabPromise;
  Assert.equal(tab2.getAttribute("usercontextid"), 1);

  yield BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  yield ContentTask.spawn(tab2.linkedBrowser, {}, function* () {
    Assert.equal(content.document.documentURI, "http://test1.example.com/");
    Assert.equal(content.document.nodePrincipal.originAttributes.userContextId, 1);
  });

  yield BrowserTestUtils.removeTab(tab);
  yield BrowserTestUtils.removeTab(tab2);
});
