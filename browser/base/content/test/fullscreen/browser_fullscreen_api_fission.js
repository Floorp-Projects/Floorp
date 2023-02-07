/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test checks that `document.fullscreenElement` is set correctly and
 * proper fullscreenchange events fire when an element inside of a
 * multi-origin tree of iframes calls `requestFullscreen()`. It is designed
 * to make sure the fullscreen API is working properly in fission when the
 * frame tree spans multiple processes.
 *
 * A similarly purposed Web Platform Test exists, but at the time of writing
 * is manual, so it cannot be run in CI:
 * `element-request-fullscreen-cross-origin-manual.sub.html`
 */

"use strict";

const actorModuleURI = getRootDirectory(gTestPath) + "FullscreenFrame.sys.mjs";
const actorName = "FullscreenFrame";

const fullscreenPath =
  getRootDirectory(gTestPath).replace("chrome://mochitests/content", "") +
  "fullscreen.html";

const fullscreenTarget = "D";
// TOP
//  | \
//  A  B
//  |
//  C
//  |
//  D
//  |
//  E
const frameTree = {
  name: "TOP",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  url: `http://example.com${fullscreenPath}`,
  allow_fullscreen: true,
  children: [
    {
      name: "A",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      url: `http://example.org${fullscreenPath}`,
      allow_fullscreen: true,
      children: [
        {
          name: "C",
          // eslint-disable-next-line @microsoft/sdl/no-insecure-url
          url: `http://example.com${fullscreenPath}`,
          allow_fullscreen: true,
          children: [
            {
              name: "D",
              // eslint-disable-next-line @microsoft/sdl/no-insecure-url
              url: `http://example.com${fullscreenPath}?different-uri=1`,
              allow_fullscreen: true,
              children: [
                {
                  name: "E",
                  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
                  url: `http://example.org${fullscreenPath}`,
                  allow_fullscreen: true,
                  children: [],
                },
              ],
            },
          ],
        },
      ],
    },
    {
      name: "B",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      url: `http://example.net${fullscreenPath}`,
      allow_fullscreen: true,
      children: [],
    },
  ],
};

add_task(async function test_fullscreen_api_cross_origin_tree() {
  await new Promise(r => {
    SpecialPowers.pushPrefEnv(
      {
        set: [
          ["full-screen-api.enabled", true],
          ["full-screen-api.allow-trusted-requests-only", false],
          ["full-screen-api.transition-duration.enter", "0 0"],
          ["full-screen-api.transition-duration.leave", "0 0"],
          ["dom.security.featurePolicy.header.enabled", true],
          ["dom.security.featurePolicy.webidl.enabled", true],
        ],
      },
      r
    );
  });

  // Register a custom window actor to handle tracking events
  // and constructing subframes
  ChromeUtils.registerWindowActor(actorName, {
    child: {
      esModuleURI: actorModuleURI,
      events: {
        fullscreenchange: { mozSystemGroup: true, capture: true },
        fullscreenerror: { mozSystemGroup: true, capture: true },
      },
    },
    allFrames: true,
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: frameTree.url,
  });

  let frames = new Map();
  async function construct_frame_children(browsingContext, tree) {
    let actor = browsingContext.currentWindowGlobal.getActor(actorName);
    frames.set(tree.name, {
      browsingContext,
      actor,
    });

    for (let child of tree.children) {
      // Create the child IFrame and wait for it to load.
      let childBC = await actor.sendQuery("CreateChild", child);
      await construct_frame_children(childBC, child);
    }
  }

  await construct_frame_children(tab.linkedBrowser.browsingContext, frameTree);

  async function check_events(expected_events) {
    for (let [name, expected] of expected_events) {
      let actor = frames.get(name).actor;

      // Each content process fires the fullscreenchange
      // event independently and in parallel making it
      // possible for the promises returned by
      // `requestFullscreen` or `exitFullscreen` to
      // resolve before all events have fired. We wait
      // for the number of events to match before
      // continuing to ensure we don't miss an expected
      // event that hasn't fired yet.
      let events;
      await TestUtils.waitForCondition(async () => {
        events = await actor.sendQuery("GetEvents");
        return events.length == expected.length;
      }, `Waiting for number of events to match`);

      Assert.equal(events.length, expected.length, "Number of events equal");
      events.forEach((value, i) => {
        Assert.equal(value, expected[i], "Event type matches");
      });
    }
  }

  async function check_fullscreenElement(expected_elements) {
    for (let [name, expected] of expected_elements) {
      let element = await frames
        .get(name)
        .actor.sendQuery("GetFullscreenElement");
      Assert.equal(element, expected, "The fullScreenElement matches");
    }
  }

  // Trigger fullscreen from the target frame.
  let target = frames.get(fullscreenTarget);
  await target.actor.sendQuery("RequestFullscreen");
  // true is fullscreenchange and false is fullscreenerror.
  await check_events(
    new Map([
      ["TOP", [true]],
      ["A", [true]],
      ["B", []],
      ["C", [true]],
      ["D", [true]],
      ["E", []],
    ])
  );
  await check_fullscreenElement(
    new Map([
      ["TOP", "child_iframe"],
      ["A", "child_iframe"],
      ["B", "null"],
      ["C", "child_iframe"],
      ["D", "body"],
      ["E", "null"],
    ])
  );

  await target.actor.sendQuery("ExitFullscreen");
  // fullscreenchange should have fired on exit as well.
  // true is fullscreenchange and false is fullscreenerror.
  await check_events(
    new Map([
      ["TOP", [true, true]],
      ["A", [true, true]],
      ["B", []],
      ["C", [true, true]],
      ["D", [true, true]],
      ["E", []],
    ])
  );
  await check_fullscreenElement(
    new Map([
      ["TOP", "null"],
      ["A", "null"],
      ["B", "null"],
      ["C", "null"],
      ["D", "null"],
      ["E", "null"],
    ])
  );

  // Clear previous events before testing exiting fullscreen with ESC.
  for (const frame of frames.values()) {
    frame.actor.sendQuery("ClearEvents");
  }
  await target.actor.sendQuery("RequestFullscreen");

  // Escape should cause the proper events to fire and
  // document.fullscreenElement should be cleared.
  let finished_exiting = target.actor.sendQuery("WaitForChange");
  EventUtils.sendKey("ESCAPE");
  await finished_exiting;
  // true is fullscreenchange and false is fullscreenerror.
  await check_events(
    new Map([
      ["TOP", [true, true]],
      ["A", [true, true]],
      ["B", []],
      ["C", [true, true]],
      ["D", [true, true]],
      ["E", []],
    ])
  );
  await check_fullscreenElement(
    new Map([
      ["TOP", "null"],
      ["A", "null"],
      ["B", "null"],
      ["C", "null"],
      ["D", "null"],
      ["E", "null"],
    ])
  );

  // Remove the tests custom window actor.
  ChromeUtils.unregisterWindowActor("FullscreenFrame");
  BrowserTestUtils.removeTab(tab);
});
