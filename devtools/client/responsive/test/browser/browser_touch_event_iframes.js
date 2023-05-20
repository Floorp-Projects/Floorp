/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test simulated touch events can correctly target embedded iframes.

// These tests put a target iframe in a small embedding area, nested
// different ways. Then a simulated mouse click is made on top of the
// target iframe. If everything works, the translation done in
// touch-simulator.js should exactly match the translation done in the
// Platform code, such that the target is hit by the synthesized tap
// is at the expected location.

info("--- Starting viewport test output ---");

info(`*** WARNING *** This test will move the mouse pointer to simulate
native mouse clicks. Do not move the mouse during this test or you may
cause intermittent failures.`);

// This test could run awhile, so request a 4x timeout duration.
requestLongerTimeout(4);

// The viewport will be square, set to VIEWPORT_DIMENSION on each axis.
const VIEWPORT_DIMENSION = 200;

const META_VIEWPORT_CONTENTS = ["width=device-width", "width=400"];

const DPRS = [1, 2, 3];

const URL_ROOT_2 = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);
const IFRAME_PATHS = [`${URL_ROOT}`, `${URL_ROOT_2}`];

const TESTS = [
  {
    description: "untranslated iframe",
    style: {},
  },
  {
    description: "translated 50% iframe",
    style: {
      position: "absolute",
      left: "50%",
      top: "50%",
      transform: "translate(-50%, -50%)",
    },
  },
  {
    description: "translated 100% iframe",
    style: {
      position: "absolute",
      left: "100%",
      top: "100%",
      transform: "translate(-100%, -100%)",
    },
  },
];

let testID = 0;

for (const mvcontent of META_VIEWPORT_CONTENTS) {
  info(`Starting test series with meta viewport content "${mvcontent}".`);

  const TEST_URL =
    `data:text/html;charset=utf-8,` +
    `<html><meta name="viewport" content="${mvcontent}">` +
    `<body style="margin:0; width:100%; height:200%;">` +
    `<iframe id="host" ` +
    `style="margin:0; border:0; width:100%; height:100%"></iframe>` +
    `</body></html>`;

  addRDMTask(TEST_URL, async function ({ ui, manager, browser }) {
    await setViewportSize(ui, manager, VIEWPORT_DIMENSION, VIEWPORT_DIMENSION);
    await setTouchAndMetaViewportSupport(ui, true);

    // Figure out our window origin in screen space, which we'll need as we calculate
    // coordinates for our simulated click events. These values are in CSS units, which
    // is weird, but we compensate for that later.
    const screenToWindowX = window.mozInnerScreenX;
    const screenToWindowY = window.mozInnerScreenY;

    for (const dpr of DPRS) {
      await selectDevicePixelRatio(ui, dpr);

      for (const path of IFRAME_PATHS) {
        for (const test of TESTS) {
          const { description, style } = test;

          const title = `ID ${testID} - ${description} with DPR ${dpr} and path ${path}`;

          info(`Starting test ${title}.`);

          await spawnViewportTask(
            ui,
            {
              title,
              style,
              path,
              VIEWPORT_DIMENSION,
              screenToWindowX,
              screenToWindowY,
            },
            async args => {
              // Define a function that returns a promise for one message that
              // contains, at least, the supplied prop, and resolves with the
              // data from that message. If a timeout value is supplied, the
              // promise will reject if the timeout elapses first.
              const oneMatchingMessageWithTimeout = (win, prop, timeout) => {
                return new Promise((resolve, reject) => {
                  let ourTimeoutID = 0;

                  const ourListener = win.addEventListener("message", e => {
                    if (typeof e.data[prop] !== "undefined") {
                      if (ourTimeoutID) {
                        win.clearTimeout(ourTimeoutID);
                      }
                      win.removeEventListener("message", ourListener);
                      resolve(e.data);
                    }
                  });

                  if (timeout) {
                    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
                    ourTimeoutID = win.setTimeout(() => {
                      win.removeEventListener("message", ourListener);
                      reject(
                        `Timeout waiting for message with prop ${prop} after ${timeout}ms.`
                      );
                    }, timeout);
                  }
                });
              };

              // Our checks are not always precise, due to rounding errors in the
              // scaling from css to screen and back. For now we use an epsilon and
              // a locally-defined isfuzzy to compensate. We can't use
              // SimpleTest.isfuzzy, because it's not bridged to the ContentTask.
              // If that is ever bridged, we can remove the isfuzzy definition here and
              // everything should "just work".
              function isfuzzy(actual, expected, epsilon, msg) {
                if (
                  actual >= expected - epsilon &&
                  actual <= expected + epsilon
                ) {
                  ok(true, msg);
                } else {
                  // This will trigger the usual failure message for is.
                  is(actual, expected, msg);
                }
              }

              // This function takes screen coordinates in css pixels.
              // TODO: This should stop using nsIDOMWindowUtils.sendNativeMouseEvent
              //       directly, and use `EventUtils.synthesizeNativeMouseEvent` in
              //       a message listener in the chrome.
              function synthesizeNativeMouseClick(win, screenX, screenY) {
                const utils = win.windowUtils;
                const scale = win.devicePixelRatio;

                return new Promise(resolve => {
                  utils.sendNativeMouseEvent(
                    screenX * scale,
                    screenY * scale,
                    utils.NATIVE_MOUSE_MESSAGE_BUTTON_DOWN,
                    0,
                    0,
                    win.document.documentElement,
                    () => {
                      utils.sendNativeMouseEvent(
                        screenX * scale,
                        screenY * scale,
                        utils.NATIVE_MOUSE_MESSAGE_BUTTON_UP,
                        0,
                        0,
                        win.document.documentElement,
                        resolve
                      );
                    }
                  );
                });
              }

              // We're done defining functions; start the actual loading of the iframe
              // and triggering the onclick handler in its content.
              const host = content.document.getElementById("host");

              // Modify the iframe style by adding the properties in the
              // provided style object.
              for (const prop in args.style) {
                info(`Setting style.${prop} to ${args.style[prop]}.`);
                host.style[prop] = args.style[prop];
              }

              // Set the iframe source, and await the ready message.
              const IFRAME_URL = args.path + "touch_event_target.html";
              const READY_TIMEOUT_MS = 5000;
              const iframeReady = oneMatchingMessageWithTimeout(
                content,
                "ready",
                READY_TIMEOUT_MS
              );
              host.src = IFRAME_URL;
              try {
                await iframeReady;
              } catch (error) {
                ok(false, `${args.title} ${error}`);
                return;
              }

              info(`iframe has finished loading.`);

              // Await reflow of the parent window.
              await new Promise(resolve => {
                content.requestAnimationFrame(() => {
                  content.requestAnimationFrame(resolve);
                });
              });

              // Now we're going to calculate screen coordinates for the upper-left
              // quadrant of the target area. We're going to do that by using the
              // following sources:
              // 1) args.screenToWindow: the window position in screen space, in CSS
              //    pixels.
              // 2) host.getBoxQuadsFromWindowOrigin(): the iframe position, relative
              //    to the window origin, in CSS pixels.
              // 3) args.VIEWPORT_DIMENSION: the viewport size, in CSS pixels.
              // We calculate the screen position of the center of the upper-left
              // quadrant of the iframe, then use sendNativeMouseEvent to dispatch
              // a click at that position. It should trigger the RDM TouchSimulator
              // and turn the mouse click into a touch event that hits the onclick
              // handler in the iframe content. If it's done correctly, the message
              // we get back should have x,y coordinates that match the center of the
              // upper left quadrant of the iframe, in CSS units.

              const hostBounds = host
                .getBoxQuadsFromWindowOrigin()[0]
                .getBounds();
              const windowToHostX = hostBounds.left;
              const windowToHostY = hostBounds.top;

              const screenToHostX = args.screenToWindowX + windowToHostX;
              const screenToHostY = args.screenToWindowY + windowToHostY;

              const quadrantOffsetDoc = hostBounds.width * 0.25;
              const hostUpperLeftQuadrantDocX = quadrantOffsetDoc;
              const hostUpperLeftQuadrantDocY = quadrantOffsetDoc;

              const quadrantOffsetViewport = args.VIEWPORT_DIMENSION * 0.25;
              const hostUpperLeftQuadrantViewportX = quadrantOffsetViewport;
              const hostUpperLeftQuadrantViewportY = quadrantOffsetViewport;

              const targetX = screenToHostX + hostUpperLeftQuadrantViewportX;
              const targetY = screenToHostY + hostUpperLeftQuadrantViewportY;

              // We're going to try a few times to click on the target area. Our method
              // for triggering a native mouse click is vulnerable to interactive mouse
              // moves while the test is running. Letting the click timeout gives us a
              // chance to try again.
              const CLICK_TIMEOUT_MS = 1000;
              const CLICK_ATTEMPTS = 3;
              let eventWasReceived = false;

              for (let attempt = 0; attempt < CLICK_ATTEMPTS; attempt++) {
                const gotXAndY = oneMatchingMessageWithTimeout(
                  content,
                  "x",
                  CLICK_TIMEOUT_MS
                );
                info(
                  `Sending native mousedown and mouseup to screen position ${targetX}, ${targetY} (attempt ${attempt}).`
                );
                await synthesizeNativeMouseClick(content, targetX, targetY);
                try {
                  const { x, y, screenX, screenY } = await gotXAndY;
                  eventWasReceived = true;
                  isfuzzy(
                    x,
                    hostUpperLeftQuadrantDocX,
                    1,
                    `${args.title} got click at close enough X ${x}, screen is ${screenX}.`
                  );
                  isfuzzy(
                    y,
                    hostUpperLeftQuadrantDocY,
                    1,
                    `${args.title} got click at close enough Y ${y}, screen is ${screenY}.`
                  );
                  break;
                } catch (error) {
                  // That click didn't work. The for loop will trigger another attempt,
                  // or give up.
                }
              }

              if (!eventWasReceived) {
                ok(
                  false,
                  `${args.title} failed to get a click after ${CLICK_ATTEMPTS} tries.`
                );
              }
            }
          );

          testID++;
        }
      }
    }
  });
}
