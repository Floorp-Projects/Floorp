/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_events_prevented() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHORT_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      await ContentTask.spawn(browser, null, async () => {
        let { ScreenshotsComponentChild } = ChromeUtils.importESModule(
          "resource:///actors/ScreenshotsComponentChild.sys.mjs"
        );
        let allOverlayEvents = ScreenshotsComponentChild.OVERLAY_EVENTS.concat(
          ScreenshotsComponentChild.PREVENTABLE_EVENTS
        );

        content.eventsReceived = [];

        function eventListener(event) {
          content.window.eventsReceived.push(event.type);
        }

        for (let eventName of [...allOverlayEvents, "wheel"]) {
          content.addEventListener(eventName, eventListener, true);
        }
      });

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      // key events
      await key.down("s");
      await key.up("s");
      await key.press("s");

      // touch events
      await touch.start(10, 10);
      await touch.move(20, 20);
      await touch.end(20, 20);

      // pointermove/mousemove, pointerdown/mousedown, pointerup/mouseup events
      await helper.clickTestPageElement();

      // pointerover/mouseover, pointerout/mouseout
      await mouse.over(100, 100);
      await mouse.out(100, 100);

      // click events and contextmenu
      await mouse.dblclick(100, 100);
      await mouse.auxclick(100, 100, { button: 1 });
      await mouse.click(100, 100);
      await mouse.contextmenu(100, 100);

      let wheelEventPromise = helper.waitForContentEventOnce("wheel");
      await ContentTask.spawn(browser, null, () => {
        content.dispatchEvent(new content.WheelEvent("wheel"));
      });
      await wheelEventPromise;

      let contentEventsReceived = await ContentTask.spawn(
        browser,
        null,
        async () => {
          return content.eventsReceived;
        }
      );

      // Events are synchronous so if we only have 1 wheel at the end,
      // we did not receive any other events
      is(
        contentEventsReceived.length,
        1,
        "Only 1 wheel event should reach the content document because everything else was prevent and stopped propagation"
      );
      is(
        contentEventsReceived[0],
        "wheel",
        "Only 1 wheel event should reach the content document because everything else was prevent and stopped propagation"
      );
    }
  );
});
