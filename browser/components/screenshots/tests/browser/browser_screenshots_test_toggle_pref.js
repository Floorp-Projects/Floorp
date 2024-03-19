/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});
ChromeUtils.defineLazyGetter(this, "ExtensionManagement", () => {
  const { Management } = ChromeUtils.importESModule(
    "resource://gre/modules/Extension.sys.mjs"
  );
  return Management;
});

const COMPONENT_PREF = "screenshots.browser.component.enabled";
const SCREENSHOTS_PREF = "extensions.screenshots.disabled";
const SCREENSHOT_EXTENSION = "screenshots@mozilla.org";

add_task(async function test_toggling_screenshots_pref() {
  let observerSpy = sinon.spy();
  let notifierSpy = sinon.spy();

  let observerStub = sinon
    .stub(ScreenshotsUtils, "observe")
    .callsFake(observerSpy);
  let notifierStub = sinon
    .stub(ScreenshotsUtils, "notify")
    .callsFake(function () {
      notifierSpy();
      ScreenshotsUtils.notify.wrappedMethod.apply(this, arguments);
    });

  // wait for startup idle tasks to complete
  await new Promise(resolve => ChromeUtils.idleDispatch(resolve));
  ok(Services.prefs.getBoolPref(COMPONENT_PREF), "Component enabled");
  ok(!Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Screenshots enabled");

  let addon = await AddonManager.getAddonByID(SCREENSHOT_EXTENSION);
  await BrowserTestUtils.waitForCondition(
    () => !addon.isActive,
    "The extension is not active when the component is prefd on"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHORT_TEST_PAGE,
    },
    async browser => {
      function extensionEventPromise(eventName, id) {
        return new Promise(resolve => {
          let listener = (_eventName, ...args) => {
            let extension = args[0];
            if (_eventName === eventName && extension.id == id) {
              ExtensionManagement.off(eventName, listener);
              resolve();
            }
          };
          ExtensionManagement.on(eventName, listener);
        });
      }

      let helper = new ScreenshotsHelper(browser);
      ok(
        addon.userDisabled,
        "The extension is disabled when the component is prefd on"
      );
      ok(
        !addon.isActive,
        "The extension is not initially active when the component is prefd on"
      );
      await BrowserTestUtils.waitForCondition(
        () => ScreenshotsUtils.initialized,
        "The component is initialized"
      );
      ok(ScreenshotsUtils.initialized, "The component is initialized");

      ok(observerSpy.notCalled, "Observer not called");
      helper.triggerUIFromToolbar();
      Assert.equal(observerSpy.callCount, 1, "Observer function called once");

      ok(notifierSpy.notCalled, "Notifier not called");
      EventUtils.synthesizeKey("s", { accelKey: true, shiftKey: true });

      await TestUtils.waitForCondition(() => notifierSpy.callCount == 1);
      Assert.equal(notifierSpy.callCount, 1, "Notify function called once");

      await TestUtils.waitForCondition(() => observerSpy.callCount == 2);
      Assert.equal(observerSpy.callCount, 2, "Observer function called twice");

      let menu = document.getElementById("contentAreaContextMenu");
      let popupshown = BrowserTestUtils.waitForPopupEvent(menu, "shown");
      EventUtils.synthesizeMouseAtCenter(document.body, {
        type: "contextmenu",
      });
      await popupshown;
      Assert.equal(menu.state, "open", "Context menu is open");

      let popuphidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
      menu.activateItem(menu.querySelector("#context-take-screenshot"));
      await popuphidden;

      Assert.equal(observerSpy.callCount, 3, "Observer function called thrice");

      let extensionReadyPromise = extensionEventPromise(
        "ready",
        SCREENSHOT_EXTENSION
      );
      Services.prefs.setBoolPref(COMPONENT_PREF, false);
      ok(!Services.prefs.getBoolPref(COMPONENT_PREF), "Extension enabled");

      info("Waiting for the extension ready event");
      await extensionReadyPromise;
      await BrowserTestUtils.waitForCondition(
        () => !addon.userDisabled,
        "The extension gets un-disabled when the component is prefd off"
      );
      ok(addon.isActive, "Extension is active");

      helper.triggerUIFromToolbar();
      Assert.equal(
        observerSpy.callCount,
        3,
        "Observer function still called thrice"
      );

      info("Waiting for the extensions overlay");
      await SpecialPowers.spawn(
        browser,
        ["#firefox-screenshots-preselection-iframe"],
        async function (iframeSelector) {
          info(
            `in waitForUIContent content function, iframeSelector: ${iframeSelector}`
          );
          let iframe;
          await ContentTaskUtils.waitForCondition(() => {
            iframe = content.document.querySelector(iframeSelector);
            if (!iframe || !ContentTaskUtils.isVisible(iframe)) {
              info("in waitForUIContent, no visible iframe yet");
              return false;
            }
            return true;
          });
          // wait a frame for the screenshots UI to finish any init
          await new content.Promise(res => content.requestAnimationFrame(res));
        }
      );

      info("Waiting for the extensions overlay");
      helper.triggerUIFromToolbar();
      await SpecialPowers.spawn(
        browser,
        ["#firefox-screenshots-preselection-iframe"],
        async function (iframeSelector) {
          info(
            `in waitForUIContent content function, iframeSelector: ${iframeSelector}`
          );
          let iframe;
          await ContentTaskUtils.waitForCondition(() => {
            iframe = content.document.querySelector(iframeSelector);
            if (!iframe || !ContentTaskUtils.isVisible(iframe)) {
              info("in waitForUIContent, no visible iframe yet");
              return true;
            }
            return false;
          });
          // wait a frame for the screenshots UI to finish any init
          await new content.Promise(res => content.requestAnimationFrame(res));
        }
      );

      popupshown = BrowserTestUtils.waitForPopupEvent(menu, "shown");
      EventUtils.synthesizeMouseAtCenter(document.body, {
        type: "contextmenu",
      });
      await popupshown;
      Assert.equal(menu.state, "open", "Context menu is open");

      popuphidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
      menu.activateItem(menu.querySelector("#context-take-screenshot"));
      await popuphidden;

      Assert.equal(
        observerSpy.callCount,
        3,
        "Observer function still called thrice"
      );

      await SpecialPowers.spawn(
        browser,
        ["#firefox-screenshots-preselection-iframe"],
        async function (iframeSelector) {
          info(
            `in waitForUIContent content function, iframeSelector: ${iframeSelector}`
          );
          let iframe;
          await ContentTaskUtils.waitForCondition(() => {
            iframe = content.document.querySelector(iframeSelector);
            if (!iframe || !ContentTaskUtils.isVisible(iframe)) {
              info("in waitForUIContent, no visible iframe yet");
              return false;
            }
            return true;
          });
          // wait a frame for the screenshots UI to finish any init
          await new content.Promise(res => content.requestAnimationFrame(res));
        }
      );

      helper.triggerUIFromToolbar();
      await SpecialPowers.spawn(
        browser,
        ["#firefox-screenshots-preselection-iframe"],
        async function (iframeSelector) {
          info(
            `in waitForUIContent content function, iframeSelector: ${iframeSelector}`
          );
          let iframe;
          await ContentTaskUtils.waitForCondition(() => {
            iframe = content.document.querySelector(iframeSelector);
            if (!iframe || !ContentTaskUtils.isVisible(iframe)) {
              return true;
            }
            info("in waitForUIContent, iframe still visible");
            info(iframe);
            return false;
          });
          // wait a frame for the screenshots UI to finish any init
          await new content.Promise(res => content.requestAnimationFrame(res));
        }
      );

      let componentReady = TestUtils.topicObserved(
        "screenshots-component-initialized"
      );

      Services.prefs.setBoolPref(COMPONENT_PREF, true);
      ok(Services.prefs.getBoolPref(COMPONENT_PREF), "Component enabled");
      // Needed for component to initialize
      await componentReady;

      helper.triggerUIFromToolbar();
      Assert.equal(
        observerSpy.callCount,
        4,
        "Observer function called four times"
      );
    }
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHORT_TEST_PAGE,
    },
    async browser => {
      Services.prefs.setBoolPref(SCREENSHOTS_PREF, true);
      Services.prefs.setBoolPref(COMPONENT_PREF, true);

      ok(Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Screenshots disabled");

      ok(
        document.getElementById("screenshot-button").disabled,
        "Toolbar button disabled"
      );

      let menu = document.getElementById("contentAreaContextMenu");
      let popupshown = BrowserTestUtils.waitForPopupEvent(menu, "shown");
      EventUtils.synthesizeMouseAtCenter(document.body, {
        type: "contextmenu",
      });
      await popupshown;
      Assert.equal(menu.state, "open", "Context menu is open");

      ok(
        menu.querySelector("#context-take-screenshot").hidden,
        "Take screenshot is not in context menu"
      );

      let popuphidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
      menu.hidePopup();
      await popuphidden;

      let componentReady = TestUtils.topicObserved(
        "screenshots-component-initialized"
      );

      Services.prefs.setBoolPref(SCREENSHOTS_PREF, false);

      ok(!Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Screenshots enabled");

      await componentReady;

      ok(ScreenshotsUtils.initialized, "The component is initialized");

      ok(
        !document.getElementById("screenshot-button").disabled,
        "Toolbar button is enabled"
      );

      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      Assert.equal(
        observerSpy.callCount,
        5,
        "Observer function called for the fifth time"
      );
    }
  );

  observerStub.restore();
  notifierStub.restore();
});
