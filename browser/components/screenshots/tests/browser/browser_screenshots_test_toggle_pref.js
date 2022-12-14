/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
});
XPCOMUtils.defineLazyGetter(this, "ExtensionManagement", () => {
  const { Management } = ChromeUtils.import(
    "resource://gre/modules/Extension.jsm"
  );
  return Management;
});

add_task(async function test() {
  let observerSpy = sinon.spy();
  let notifierSpy = sinon.spy();

  let observerStub = sinon
    .stub(ScreenshotsUtils, "observe")
    .callsFake(observerSpy);
  let notifierStub = sinon
    .stub(ScreenshotsUtils, "notify")
    .callsFake(function(window, type) {
      notifierSpy();
      ScreenshotsUtils.notify.wrappedMethod.apply(this, arguments);
    });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      function awaitExtensionEvent(eventName, id) {
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
      const SCREENSHOT_EXTENSION = "screenshots@mozilla.org";

      let helper = new ScreenshotsHelper(browser);

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

      menu.querySelector("#context-take-screenshot").click();
      Assert.equal(observerSpy.callCount, 3, "Observer function called thrice");

      let popuphidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
      menu.hidePopup();
      await popuphidden;

      const COMPONENT_PREF = "screenshots.browser.component.enabled";
      await SpecialPowers.pushPrefEnv({
        set: [[COMPONENT_PREF, false]],
      });
      ok(!Services.prefs.getBoolPref(COMPONENT_PREF), "Extension enabled");
      await awaitExtensionEvent("ready", SCREENSHOT_EXTENSION);

      helper.triggerUIFromToolbar();
      Assert.equal(
        observerSpy.callCount,
        3,
        "Observer function still called thrice"
      );

      await SpecialPowers.spawn(
        browser,
        ["#firefox-screenshots-preselection-iframe"],
        async function(iframeSelector) {
          info(
            `in waitForUIContent content function, iframeSelector: ${iframeSelector}`
          );
          let iframe;
          await ContentTaskUtils.waitForCondition(() => {
            iframe = content.document.querySelector(iframeSelector);
            if (!iframe || !ContentTaskUtils.is_visible(iframe)) {
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
        async function(iframeSelector) {
          info(
            `in waitForUIContent content function, iframeSelector: ${iframeSelector}`
          );
          let iframe;
          await ContentTaskUtils.waitForCondition(() => {
            iframe = content.document.querySelector(iframeSelector);
            if (!iframe || !ContentTaskUtils.is_visible(iframe)) {
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

      menu.querySelector("#context-take-screenshot").click();
      Assert.equal(
        observerSpy.callCount,
        3,
        "Observer function still called thrice"
      );

      popuphidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
      menu.hidePopup();
      await popuphidden;

      await SpecialPowers.spawn(
        browser,
        ["#firefox-screenshots-preselection-iframe"],
        async function(iframeSelector) {
          info(
            `in waitForUIContent content function, iframeSelector: ${iframeSelector}`
          );
          let iframe;
          await ContentTaskUtils.waitForCondition(() => {
            iframe = content.document.querySelector(iframeSelector);
            if (!iframe || !ContentTaskUtils.is_visible(iframe)) {
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
        async function(iframeSelector) {
          info(
            `in waitForUIContent content function, iframeSelector: ${iframeSelector}`
          );
          let iframe;
          await ContentTaskUtils.waitForCondition(() => {
            iframe = content.document.querySelector(iframeSelector);
            if (!iframe || !ContentTaskUtils.is_visible(iframe)) {
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

      await SpecialPowers.pushPrefEnv({
        set: [[COMPONENT_PREF, true]],
      });
      ok(Services.prefs.getBoolPref(COMPONENT_PREF), "Component enabled");
      // Needed for component to initialize
      await componentReady;

      helper.triggerUIFromToolbar();
      Assert.equal(
        observerSpy.callCount,
        4,
        "Observer function called four times"
      );

      const SCREENSHOTS_PREF = "extensions.screenshots.disabled";
      await SpecialPowers.pushPrefEnv({
        set: [[SCREENSHOTS_PREF, true]],
      });
      ok(Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Screenshots disabled");
    }
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      const SCREENSHOTS_PREF = "extensions.screenshots.disabled";
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

      await SpecialPowers.pushPrefEnv({
        set: [[SCREENSHOTS_PREF, false]],
      });
      ok(!Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Screenshots enabled");
    }
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      const SCREENSHOTS_PREF = "extensions.screenshots.disabled";
      ok(!Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Screenshots enabled");

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
