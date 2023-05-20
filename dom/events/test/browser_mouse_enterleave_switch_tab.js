"use strict";

async function synthesizeMouseAndWait(aBrowser, aEvent) {
  let promise = SpecialPowers.spawn(aBrowser, [aEvent], async event => {
    await new Promise(resolve => {
      content.document.documentElement.addEventListener(event, resolve, {
        once: true,
      });
    });
  });
  // Ensure content has been added event listener.
  await SpecialPowers.spawn(aBrowser, [], () => {});
  EventUtils.synthesizeMouse(aBrowser, 10, 10, { type: aEvent });
  return promise;
}

function AddMouseEventListener(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], () => {
    content.catchedEvents = [];
    let listener = function (aEvent) {
      content.catchedEvents.push(aEvent.type);
    };

    let target = content.document.querySelector("p");
    target.onmouseenter = listener;
    target.onmouseleave = listener;
  });
}

function clearMouseEventListenerAndCheck(aBrowser, aExpectedEvents) {
  return SpecialPowers.spawn(aBrowser, [aExpectedEvents], events => {
    let target = content.document.querySelector("p");
    target.onmouseenter = null;
    target.onmouseleave = null;

    Assert.deepEqual(content.catchedEvents, events);
  });
}

add_task(async function testSwitchTabs() {
  const tabFirst = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/browser/browser/base/content/test/general/dummy_page.html",
    true
  );

  info("Initial mouse move");
  await EventUtils.synthesizeAndWaitNativeMouseMove(
    tabFirst.linkedBrowser,
    10,
    10
  );

  info("Open and move to a new tab");
  await AddMouseEventListener(tabFirst.linkedBrowser);
  const tabSecond = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/browser/browser/base/content/test/general/dummy_page.html"
  );
  // Synthesize a mousemove to generate corresponding mouseenter and mouseleave
  // events.
  await EventUtils.synthesizeAndWaitNativeMouseMove(
    tabSecond.linkedBrowser,
    10,
    10
  );
  // Wait a bit to see if there is any unexpected mouse event.
  await TestUtils.waitForTick();
  await clearMouseEventListenerAndCheck(tabFirst.linkedBrowser, ["mouseleave"]);

  info("switch back to the previous tab");
  await AddMouseEventListener(tabFirst.linkedBrowser);
  await AddMouseEventListener(tabSecond.linkedBrowser);
  await BrowserTestUtils.switchTab(gBrowser, tabFirst);
  // Synthesize a mousemove to generate corresponding mouseenter and mouseleave
  // events.
  await EventUtils.synthesizeAndWaitNativeMouseMove(
    tabFirst.linkedBrowser,
    10,
    10
  );
  // Wait a bit to see if there is any unexpected mouse event.
  await TestUtils.waitForTick();
  await clearMouseEventListenerAndCheck(tabFirst.linkedBrowser, ["mouseenter"]);
  await clearMouseEventListenerAndCheck(tabSecond.linkedBrowser, [
    "mouseleave",
  ]);

  info("Close tabs");
  BrowserTestUtils.removeTab(tabFirst);
  BrowserTestUtils.removeTab(tabSecond);
});

add_task(async function testSwitchTabsWithMouseDown() {
  const tabFirst = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/browser/browser/base/content/test/general/dummy_page.html",
    true
  );

  info("Initial mouse move");
  await EventUtils.synthesizeAndWaitNativeMouseMove(
    tabFirst.linkedBrowser,
    10,
    10
  );

  info("mouse down");
  await synthesizeMouseAndWait(tabFirst.linkedBrowser, "mousedown");

  info("Open and move to a new tab");
  await AddMouseEventListener(tabFirst.linkedBrowser);
  const tabSecond = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/browser/browser/base/content/test/general/dummy_page.html"
  );
  // Synthesize a mousemove to generate corresponding mouseenter and mouseleave
  // events.
  await EventUtils.synthesizeAndWaitNativeMouseMove(
    tabSecond.linkedBrowser,
    10,
    10
  );

  info("mouse up");
  await synthesizeMouseAndWait(tabSecond.linkedBrowser, "mouseup");
  // Wait a bit to see if there is any unexpected mouse event.
  await TestUtils.waitForTick();
  await clearMouseEventListenerAndCheck(tabFirst.linkedBrowser, ["mouseleave"]);

  info("mouse down");
  await synthesizeMouseAndWait(tabSecond.linkedBrowser, "mousedown");

  info("switch back to the previous tab");
  await AddMouseEventListener(tabFirst.linkedBrowser);
  await AddMouseEventListener(tabSecond.linkedBrowser);
  await BrowserTestUtils.switchTab(gBrowser, tabFirst);
  // Synthesize a mousemove to generate corresponding mouseenter and mouseleave
  // events.
  await EventUtils.synthesizeAndWaitNativeMouseMove(
    tabFirst.linkedBrowser,
    10,
    10
  );

  info("mouse up");
  await synthesizeMouseAndWait(tabFirst.linkedBrowser, "mouseup");
  // Wait a bit to see if there is any unexpected mouse event.
  await TestUtils.waitForTick();
  await clearMouseEventListenerAndCheck(tabFirst.linkedBrowser, ["mouseenter"]);
  await clearMouseEventListenerAndCheck(tabSecond.linkedBrowser, [
    "mouseleave",
  ]);

  info("Close tabs");
  BrowserTestUtils.removeTab(tabFirst);
  BrowserTestUtils.removeTab(tabSecond);
});
