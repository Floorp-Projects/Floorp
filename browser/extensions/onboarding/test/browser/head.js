/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { Preferences } = Cu.import("resource://gre/modules/Preferences.jsm", {});

const ABOUT_HOME_URL = "about:home";
const ABOUT_NEWTAB_URL = "about:newtab";
const URLs = [ABOUT_HOME_URL, ABOUT_NEWTAB_URL];
const TOUR_IDs = [
  "onboarding-tour-performance",
  "onboarding-tour-private-browsing",
  "onboarding-tour-screenshots",
  "onboarding-tour-addons",
  "onboarding-tour-customize",
  "onboarding-tour-default-browser",
];
const UPDATE_TOUR_IDs = [
  "onboarding-tour-performance",
  "onboarding-tour-library",
  "onboarding-tour-screenshots",
  "onboarding-tour-singlesearch",
  "onboarding-tour-customize",
  "onboarding-tour-sync",
];
const ICON_STATE_WATERMARK = "watermark";
const ICON_STATE_DEFAULT = "default";

registerCleanupFunction(resetOnboardingDefaultState);

function resetOnboardingDefaultState() {
  // All the prefs should be reset to the default states
  // and no need to revert back so we don't use `SpecialPowers.pushPrefEnv` here.
  Preferences.set("browser.onboarding.enabled", true);
  Preferences.set("browser.onboarding.state", ICON_STATE_DEFAULT);
  Preferences.set("browser.onboarding.notification.finished", false);
  Preferences.set("browser.onboarding.notification.mute-duration-on-first-session-ms", 300000);
  Preferences.set("browser.onboarding.notification.max-life-time-per-tour-ms", 432000000);
  Preferences.set("browser.onboarding.notification.max-life-time-all-tours-ms", 1209600000);
  Preferences.set("browser.onboarding.notification.max-prompt-count-per-tour", 8);
  Preferences.reset("browser.onboarding.notification.last-time-of-changing-tour-sec");
  Preferences.reset("browser.onboarding.notification.prompt-count");
  Preferences.reset("browser.onboarding.notification.tour-ids-queue");
  TOUR_IDs.forEach(id => Preferences.reset(`browser.onboarding.tour.${id}.completed`));
  UPDATE_TOUR_IDs.forEach(id => Preferences.reset(`browser.onboarding.tour.${id}.completed`));
}

function setTourCompletedState(tourId, state) {
  Preferences.set(`browser.onboarding.tour.${tourId}.completed`, state);
}

async function openTab(url) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let loadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, url);
  await loadedPromise;
  return tab;
}

function reloadTab(tab) {
  let reloadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  return reloadPromise;
}

function promiseOnboardingOverlayLoaded(browser) {
  function isLoaded() {
    let doc = content && content.document;
    if (doc.querySelector("#onboarding-overlay")) {
      ok(true, "Should load onboarding overlay");
      return Promise.resolve();
    }
    return new Promise(resolve => {
      let observer = new content.MutationObserver(mutations => {
        mutations.forEach(mutation => {
          let overlay = Array.from(mutation.addedNodes)
                             .find(node => node.id == "onboarding-overlay");
          if (overlay) {
            observer.disconnect();
            ok(true, "Should load onboarding overlay");
            resolve();
          }
        });
      });
      observer.observe(doc.body, { childList: true });
    });
  }
  return ContentTask.spawn(browser, {}, isLoaded);
}

function promiseOnboardingOverlayOpened(browser) {
  return BrowserTestUtils.waitForCondition(() =>
    ContentTask.spawn(browser, {}, () =>
      content.document.querySelector("#onboarding-overlay").classList.contains(
        "onboarding-opened")),
    "Should open onboarding overlay",
    100,
    30
  );
}

function promiseOnboardingOverlayClosed(browser) {
  return BrowserTestUtils.waitForCondition(() =>
    ContentTask.spawn(browser, {}, () =>
      !content.document.querySelector("#onboarding-overlay").classList.contains(
        "onboarding-opened")),
    "Should close onboarding overlay",
    100,
    30
  );
}

function promisePrefUpdated(name, expectedValue) {
  return new Promise(resolve => {
    let onUpdate = actualValue => {
      Preferences.ignore(name, onUpdate);
      is(expectedValue, actualValue, `Should update the pref of ${name}`);
      resolve();
    };
    Preferences.observe(name, onUpdate);
  });
}

function promiseTourNotificationOpened(browser) {
  function isOpened() {
    let doc = content && content.document;
    let notification = doc.querySelector("#onboarding-notification-bar");
    if (notification && notification.classList.contains("onboarding-opened")) {
      ok(true, "Should open tour notification");
      return Promise.resolve();
    }
    return new Promise(resolve => {
      let observer = new content.MutationObserver(mutations => {
        mutations.forEach(mutation => {
          let bar = Array.from(mutation.addedNodes)
                         .find(node => node.id == "onboarding-notification-bar");
          if (bar && bar.classList.contains("onboarding-opened")) {
            observer.disconnect();
            ok(true, "Should open tour notification");
            resolve();
          }
        });
      });
      observer.observe(doc.body, { childList: true });
    });
  }
  return ContentTask.spawn(browser, {}, isOpened);
}

function promiseTourNotificationClosed(browser) {
  let condition = () => {
    return ContentTask.spawn(browser, {}, function() {
      return new Promise(resolve => {
        let bar = content.document.querySelector("#onboarding-notification-bar");
        if (bar && !bar.classList.contains("onboarding-opened")) {
          resolve(true);
          return;
        }
        resolve(false);
      });
    });
  };
  return BrowserTestUtils.waitForCondition(
    condition,
    "Should close tour notification",
    100,
    30
  );
}

function getCurrentNotificationTargetTourId(browser) {
  return ContentTask.spawn(browser, {}, function() {
    let bar = content.document.querySelector("#onboarding-notification-bar");
    return bar ? bar.dataset.targetTourId : null;
  });
}

function getCurrentActiveTour(browser) {
  return ContentTask.spawn(browser, {}, function() {
    let list = content.document.querySelector("#onboarding-tour-list");
    let items = list.querySelectorAll(".onboarding-tour-item");
    let activeNavItemId = null;
    for (let item of items) {
      if (item.classList.contains("onboarding-active")) {
        if (!activeNavItemId) {
          activeNavItemId = item.id;
        } else {
          ok(false, "There are more than one item marked as active.");
        }
      }
    }
    let activePageId = null;
    let pages = content.document.querySelectorAll(".onboarding-tour-page");
    for (let page of pages) {
      if (page.style.display != "none") {
        if (!activePageId) {
          activePageId = page.id;
        } else {
          ok(false, "Thre are more than one tour page visible.");
        }
      }
    }
    return { activeNavItemId, activePageId };
  });
}

function waitUntilWindowIdle(browser) {
  return ContentTask.spawn(browser, {}, function() {
    return new Promise(resolve => content.requestIdleCallback(resolve));
  });
}

function skipMuteNotificationOnFirstSession() {
  Preferences.set("browser.onboarding.notification.mute-duration-on-first-session-ms", 0);
}

function assertOverlaySemantics(browser) {
  return ContentTask.spawn(browser, {}, function() {
    let doc = content.document;

    info("Checking dialog");
    let dialog = doc.getElementById("onboarding-overlay-dialog");
    is(dialog.getAttribute("role"), "dialog",
      "Dialog should have a dialog role attribute set");
    is(dialog.tabIndex, "-1", "Dialog should be focusable but not in tab order");
    is(dialog.getAttribute("aria-labelledby"), "onboarding-header",
      "Dialog should be labaled by its header");

    info("Checking the tablist container");
    is(doc.getElementById("onboarding-tour-list").getAttribute("role"), "tablist",
      "Tour list should have a tablist role attribute set");

    info("Checking each tour item that represents the tab");
    let items = [...doc.querySelectorAll(".onboarding-tour-item")];
    items.forEach(item => {
      is(item.parentNode.getAttribute("role"), "presentation",
        "Parent should have no semantic value");
      is(item.getAttribute("aria-selected"),
         item.classList.contains("onboarding-active") ? "true" : "false",
         "Active item should have aria-selected set to true and inactive to false");
      is(item.tabIndex, "0", "Item tab index must be set for keyboard accessibility");
      is(item.getAttribute("role"), "tab", "Item should have a tab role attribute set");
      let tourPanelId = `${item.id}-page`;
      is(item.getAttribute("aria-controls"), tourPanelId,
        "Item should have aria-controls attribute point to its tabpanel");
      let panel = doc.getElementById(tourPanelId);
      is(panel.getAttribute("role"), "tabpanel",
        "Tour panel should have a tabpanel role attribute set");
      is(panel.getAttribute("aria-labelledby"), item.id,
        "Tour panel should have aria-labelledby attribute point to its tab");
    });
  });
}

function assertModalDialog(browser, args) {
  return ContentTask.spawn(browser, args, ({ keyboardFocus, visible, focusedId }) => {
    let doc = content.document;
    let overlayButton = doc.getElementById("onboarding-overlay-button");
    if (visible) {
      [...doc.body.children].forEach(child =>
        child.id !== "onboarding-overlay" &&
        is(child.getAttribute("aria-hidden"), "true",
          "Content should not be visible to screen reader"));
      is(focusedId ? doc.getElementById(focusedId) : doc.body,
        doc.activeElement, `Focus should be on ${focusedId || "body"}`);
      is(keyboardFocus ? "true" : undefined,
        overlayButton.dataset.keyboardFocus,
        "Overlay button focus state is saved correctly");
    } else {
      [...doc.body.children].forEach(
        child => ok(!child.hasAttribute("aria-hidden"),
          "Content should be visible to screen reader"));
      if (keyboardFocus) {
        is(overlayButton, doc.activeElement,
          "Focus should be set on overlay button");
      }
      ok(!overlayButton.dataset.keyboardFocus,
        "Overlay button focus state should be cleared");
    }
  });
}

function assertWatermarkIconDisplayed(browser) {
  return ContentTask.spawn(browser, {}, function() {
    let overlayButton = content.document.getElementById("onboarding-overlay-button");
    ok(overlayButton.classList.contains("onboarding-watermark"), "Should display the watermark onboarding icon");
  });
}
