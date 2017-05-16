"use strict";

let EventUtils = {};
Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

/**
 * Dragging an URL to a tab without userContextId set.
 */
add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

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
  EventUtils.synthesizeDrop(tab, tab, [[{type: "text/plain", data: "http://test1.example.com/"}]], "link", window, undefined, event);

  await awaitDrop;

  let tab2 = await newTabPromise;
  Assert.ok(!tab2.hasAttribute("usercontextid"), "Tab shouldn't have usercontextid attribute");

  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  await ContentTask.spawn(tab2.linkedBrowser, {}, async function() {
    Assert.equal(content.document.documentURI, "http://test1.example.com/");
    Assert.equal(content.document.nodePrincipal.originAttributes.userContextId, 0);

    // referrer is empty when urls are dragged to new or existing tabs.
    // If this changes in the future, it would be okay to send the referrer
    // in this case because we are creating a new tab with the default
    // usercontextid as the original tab.
    Assert.equal(content.document.referrer, "", "referrer should be empty");
  });

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(tab2);
});

/**
 * When dragging an URL to a new tab, the new tab should have the same
 * userContextId as the original tab.
 */
add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "http://example.com/", {userContextId: 1});
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

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
  EventUtils.synthesizeDrop(tab, tab, [[{type: "text/plain", data: "http://test1.example.com/"}]], "link", window, undefined, event);

  await awaitDrop;

  let tab2 = await newTabPromise;
  Assert.equal(tab2.getAttribute("usercontextid"), 1);

  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  await ContentTask.spawn(tab2.linkedBrowser, {}, async function() {
    Assert.equal(content.document.documentURI, "http://test1.example.com/");
    Assert.equal(content.document.nodePrincipal.originAttributes.userContextId, 1);

    // referrer is empty when urls are dragged to new or existing tabs.
    // If this changes in the future, it would be okay to send the referrer
    // in this case because we are creating a new tab with the same
    // usercontextid as the original tab.
    Assert.equal(content.document.referrer, "", "referrer should be empty");
  });

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(tab2);
});

/**
 * When dragging a URL from one tab or link on a tab to an existing tab, the
 * existing tab should not change its userContextId.
 * Ex: if you drag a link from tab 1 with userContext 1 to tab 2 with
 * userContext 2, the link will open in tab 2 with userContext 2.
 */
add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "http://example.com/", {userContextId: 1});
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let tab2 = BrowserTestUtils.addTab(gBrowser, "http://example.org/", {userContextId: 2});
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  let awaitDrop = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "drop");

  EventUtils.synthesizeDrop(tab, tab2, [[{type: "text/plain", data: "http://test1.example.com/"}]], "link", window);

  await awaitDrop;
  Assert.equal(tab2.getAttribute("usercontextid"), 2);

  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  await ContentTask.spawn(tab2.linkedBrowser, {}, async function() {
    Assert.equal(content.document.documentURI, "http://test1.example.com/");
    Assert.equal(content.document.nodePrincipal.originAttributes.userContextId, 2);

    // referrer is empty when urls are dragged to new or existing tabs.
    // If this changes in the future, we should ensure that we are not sending
    // a referrer for this case!  When opening links across user contexts, we
    // don't want the referrer to follow the user from one context to another.
    Assert.equal(content.document.referrer, "", "referrer should be empty");
  });

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(tab2);
});
