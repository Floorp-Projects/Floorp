/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const STATE_CHANGED_TOPIC = "fxa-migration:state-changed";

let imports = {};
Cu.import("resource://services-sync/FxaMigrator.jsm", imports);

add_task(function* test() {
  // Fake the state where we need an FxA user.
  let buttonPromise = promiseButtonMutation();
  Services.obs.notifyObservers(null, "fxa-migration:state-changed",
                               fxaMigrator.STATE_USER_FXA);
  let buttonState = yield buttonPromise;
  assertButtonState(buttonState, "migrate-signup", true);

  // Fake the state where we need a verified FxA user.
  buttonPromise = promiseButtonMutation();
  let email = Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
  email.data = "foo@example.com";
  Services.obs.notifyObservers(email, "fxa-migration:state-changed",
                               fxaMigrator.STATE_USER_FXA_VERIFIED);
  buttonState = yield buttonPromise;
  assertButtonState(buttonState, "migrate-verify", true,
                    "foo@example.com not verified");

  // Fake the state where no migration is needed.
  buttonPromise = promiseButtonMutation();
  Services.obs.notifyObservers(null, "fxa-migration:state-changed", null);
  buttonState = yield buttonPromise;
  // In this case, the front end has called fxAccounts.getSignedInUser() to
  // update the button label and status.  But since there isn't actually a user,
  // the button is left with no fxastatus.
  assertButtonState(buttonState, "", true);
});

function assertButtonState(buttonState, expectedStatus, expectedVisible,
                           expectedLabel=undefined) {
  Assert.equal(buttonState.fxastatus, expectedStatus,
               "Button fxstatus attribute");
  Assert.equal(!buttonState.hidden, expectedVisible, "Button visibility");
  if (expectedLabel !== undefined) {
    Assert.equal(buttonState.label, expectedLabel, "Button label");
  }
}

function promiseButtonMutation() {
  return new Promise((resolve, reject) => {
    let obs = new MutationObserver(mutations => {
      info("Observed mutations for attributes: " +
           mutations.map(m => m.attributeName));
      if (mutations.some(m => m.attributeName == "fxastatus")) {
        obs.disconnect();
        resolve({
          fxastatus: gFxAccounts.button.getAttribute("fxastatus"),
          hidden: gFxAccounts.button.hidden,
          label: gFxAccounts.button.label,
        });
      }
    });
    obs.observe(gFxAccounts.button, { attributes: true });
  });
}
