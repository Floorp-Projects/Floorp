/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the "orientationchange" event is fired when the "rotate button" is clicked.

// TODO: This test should also check that the orientation is set properly on the iframe.
// This is currently not working and should be worked on in Bug 1704830.

const TEST_DOCUMENT = `doc_with_remote_iframe_and_isolated_cross_origin_capabilities.sjs`;
const TEST_COM_URL = URL_ROOT_COM_SSL + TEST_DOCUMENT;

const testDevice = {
  name: "Fake Phone RDM Test",
  width: 320,
  height: 570,
  pixelRatio: 5.5,
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  touch: true,
  firefoxOS: true,
  os: "custom",
  featured: true,
};

// Add the new device to the list
addDeviceForTest(testDevice);

addRDMTask(TEST_COM_URL, async function ({ ui }) {
  await pushPref("devtools.responsive.viewport.angle", 0);

  info("Check the original orientation values before the orientationchange");
  is(
    await getScreenOrientationType(ui.getViewportBrowser()),
    "portrait-primary",
    "Primary orientation type is portrait-primary"
  );

  is(
    await getScreenOrientationAngle(ui.getViewportBrowser()),
    0,
    "Original angle is set at 0 degrees"
  );

  info(
    "Check that rotating the viewport does trigger an orientationchange event"
  );
  let waitForOrientationChangeEvent = isOrientationChangeEventEmitted(
    ui.getViewportBrowser()
  );
  rotateViewport(ui);
  is(
    await waitForOrientationChangeEvent,
    true,
    "'orientationchange' event fired"
  );

  is(
    await getScreenOrientationType(ui.getViewportBrowser()),
    "landscape-primary",
    "Orientation state was updated to landscape-primary"
  );

  is(
    await getScreenOrientationAngle(ui.getViewportBrowser()),
    90,
    "Orientation angle was updated to 90 degrees"
  );

  info("Check that the viewport orientation values persist after reload");
  await reloadBrowser();

  is(
    await getScreenOrientationType(ui.getViewportBrowser()),
    "landscape-primary",
    "Orientation is still landscape-primary"
  );
  is(
    await getInitialScreenOrientationType(ui.getViewportBrowser()),
    "landscape-primary",
    "orientation type was set on the page very early in its lifecycle"
  );
  is(
    await getScreenOrientationAngle(ui.getViewportBrowser()),
    90,
    "Orientation angle is still 90"
  );
  is(
    await getInitialScreenOrientationAngle(ui.getViewportBrowser()),
    90,
    "orientation angle was set on the page early in its lifecycle"
  );

  info(
    "Check that the viewport orientation values persist after navigating to a page that forces the creation of a new browsing context"
  );
  const browser = ui.getViewportBrowser();
  const previousBrowsingContextId = browser.browsingContext.id;
  const waitForReload = await watchForDevToolsReload(browser);

  BrowserTestUtils.startLoadingURIString(
    browser,
    URL_ROOT_ORG_SSL + TEST_DOCUMENT + "?crossOriginIsolated=true"
  );
  await waitForReload();

  isnot(
    browser.browsingContext.id,
    previousBrowsingContextId,
    "A new browsing context was created"
  );

  is(
    await getScreenOrientationType(ui.getViewportBrowser()),
    "landscape-primary",
    "Orientation is still landscape-primary after navigating to a new browsing context"
  );
  is(
    await getInitialScreenOrientationType(ui.getViewportBrowser()),
    "landscape-primary",
    "orientation type was set on the page very early in its lifecycle"
  );
  is(
    await getScreenOrientationAngle(ui.getViewportBrowser()),
    90,
    "Orientation angle is still 90 after navigating to a new browsing context"
  );
  is(
    await getInitialScreenOrientationAngle(ui.getViewportBrowser()),
    90,
    "orientation angle was set on the page early in its lifecycle"
  );

  info(
    "Check the orientationchange event is not dispatched when changing devices."
  );
  waitForOrientationChangeEvent = isOrientationChangeEventEmitted(
    ui.getViewportBrowser()
  );

  await selectDevice(ui, testDevice.name);
  is(
    await waitForOrientationChangeEvent,
    false,
    "orientationchange event was not dispatched when changing devices"
  );

  info("Check the new orientation values after selecting device.");
  is(
    await getScreenOrientationType(ui.getViewportBrowser()),
    "portrait-primary",
    "New orientation type is portrait-primary"
  );

  is(
    await getScreenOrientationAngle(ui.getViewportBrowser()),
    0,
    "Orientation angle is 0"
  );

  info(
    "Check the orientationchange event is not dispatched when calling the command with the same orientation."
  );
  waitForOrientationChangeEvent = isOrientationChangeEventEmitted(
    ui.getViewportBrowser()
  );

  // We're directly calling the command here as there's no way to do such action from the UI.
  await ui.commands.targetConfigurationCommand.updateConfiguration({
    rdmPaneOrientation: {
      type: "portrait-primary",
      angle: 0,
      isViewportRotated: true,
    },
  });
  is(
    await waitForOrientationChangeEvent,
    false,
    "orientationchange event was not dispatched after trying to set the same orientation again"
  );
});

function getScreenOrientationType(browserOrBrowsingContext) {
  return SpecialPowers.spawn(browserOrBrowsingContext, [], () => {
    return content.screen.orientation.type;
  });
}

function getScreenOrientationAngle(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.screen.orientation.angle
  );
}

function getInitialScreenOrientationType(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.wrappedJSObject.initialOrientationType
  );
}

function getInitialScreenOrientationAngle(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.wrappedJSObject.initialOrientationAngle
  );
}

async function isOrientationChangeEventEmitted(browserOrBrowsingContext) {
  const onTimeout = wait(1000).then(() => "TIMEOUT");
  const onOrientationChangeEvent = SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => {
      content.eventController = new content.AbortController();
      return new Promise(resolve => {
        content.window.addEventListener(
          "orientationchange",
          () => {
            resolve();
          },
          {
            signal: content.eventController.signal,
            once: true,
          }
        );
      });
    }
  );

  const result = await Promise.race([onTimeout, onOrientationChangeEvent]);

  // Remove the event listener
  await SpecialPowers.spawn(browserOrBrowsingContext, [], () => {
    content.eventController.abort();
    delete content.eventController;
  });

  return result !== "TIMEOUT";
}
