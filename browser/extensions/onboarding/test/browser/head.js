/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Preferences.jsm");

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
