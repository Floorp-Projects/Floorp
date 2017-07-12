/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { Preferences } = Cu.import("resource://gre/modules/Preferences.jsm", {});

const ABOUT_HOME_URL = "about:home";
const ABOUT_NEWTAB_URL = "about:newtab";
const URLs = [ABOUT_HOME_URL, ABOUT_NEWTAB_URL];
const TOUR_IDs = [
  "onboarding-tour-private-browsing",
  "onboarding-tour-addons",
  "onboarding-tour-customize",
  "onboarding-tour-search",
  "onboarding-tour-default-browser",
  "onboarding-tour-sync",
];
const UPDATE_TOUR_IDs = [];

registerCleanupFunction(resetOnboardingDefaultState);

function resetOnboardingDefaultState() {
  // All the prefs should be reset to the default states
  // and no need to revert back so we don't use `SpecialPowers.pushPrefEnv` here.
  Preferences.set("browser.onboarding.enabled", true);
  Preferences.set("browser.onboarding.hidden", false);
  Preferences.set("browser.onboarding.notification.finished", false);
  Preferences.set("browser.onboarding.notification.mute-duration-on-first-session-ms", 300000);
  Preferences.set("browser.onboarding.notification.max-life-time-per-tour-ms", 432000000);
  Preferences.set("browser.onboarding.notification.max-prompt-count-per-tour", 8);
  Preferences.reset("browser.onboarding.notification.last-time-of-changing-tour-sec");
  Preferences.reset("browser.onboarding.notification.prompt-count");
  Preferences.reset("browser.onboarding.notification.tour-ids-queue");
  TOUR_IDs.forEach(id => Preferences.reset(`browser.onboarding.tour.${id}.completed`));
}

function setTourCompletedState(tourId, state) {
  Preferences.set(`browser.onboarding.tour.${tourId}.completed`, state);
}

function promiseOnboardingOverlayLoaded(browser) {
  // The onboarding overlay is init inside window.requestIdleCallback, not immediately,
  // so we use check conditions here.
  let condition = () => {
    return ContentTask.spawn(browser, {}, function() {
      return new Promise(resolve => {
        let doc = content && content.document;
        if (doc && doc.querySelector("#onboarding-overlay")) {
          resolve(true);
          return;
        }
        resolve(false);
      });
    })
  };
  return BrowserTestUtils.waitForCondition(
    condition,
    "Should load onboarding overlay",
    100,
    30
  );
}

function promiseOnboardingOverlayOpened(browser) {
  let condition = () => {
    return ContentTask.spawn(browser, {}, function() {
      return new Promise(resolve => {
        let overlay = content.document.querySelector("#onboarding-overlay");
        if (overlay.classList.contains("onboarding-opened")) {
          resolve(true);
          return;
        }
        resolve(false);
      });
    })
  };
  return BrowserTestUtils.waitForCondition(
    condition,
    "Should open onboarding overlay",
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
  let condition = () => {
    return ContentTask.spawn(browser, {}, function() {
      return new Promise(resolve => {
        let bar = content.document.querySelector("#onboarding-notification-bar");
        if (bar && bar.classList.contains("onboarding-opened")) {
          resolve(true);
          return;
        }
        resolve(false);
      });
    })
  };
  return BrowserTestUtils.waitForCondition(
    condition,
    "Should open tour notification",
    100,
    30
  );
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
    })
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
        activeNavItemId = item.id;
        break;
      }
    }
    let activePageId = null;
    let pages = content.document.querySelectorAll(".onboarding-tour-page");
    for (let page of pages) {
      if (page.style.display != "none") {
        activePageId = page.id;
        break;
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
