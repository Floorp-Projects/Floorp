/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const STATE_CHANGED_TOPIC = "fxa-migration:state-changed";
const NOTIFICATION_TITLE = "fxa-migration";

let imports = {};
Cu.import("resource://services-sync/FxaMigrator.jsm", imports);

add_task(function* test() {
  // Fake the state where we need an FxA user.
  let buttonPromise = promiseButtonMutation();
  Services.obs.notifyObservers(null, STATE_CHANGED_TOPIC,
                               imports.fxaMigrator.STATE_USER_FXA);
  let buttonState = yield buttonPromise;
  assertButtonState(buttonState, "migrate-signup", true);
  Assert.ok(Weave.Notifications.notifications.some(n => {
    return n.title == NOTIFICATION_TITLE;
  }), "Needs-user notification should be present");

  // Fake the state where we need a verified FxA user.
  buttonPromise = promiseButtonMutation();
  let email = Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
  email.data = "foo@example.com";
  Services.obs.notifyObservers(email, STATE_CHANGED_TOPIC,
                               imports.fxaMigrator.STATE_USER_FXA_VERIFIED);
  buttonState = yield buttonPromise;
  assertButtonState(buttonState, "migrate-verify", true,
                    "foo@example.com not verified");
  let note = Weave.Notifications.notifications.find(n => {
    return n.title == NOTIFICATION_TITLE;
  });
  Assert.ok(!!note, "Needs-verification notification should be present");
  Assert.ok(note.description.includes(email.data),
            "Needs-verification notification should include email");

  // Fake the state where no migration is needed.
  buttonPromise = promiseButtonMutation();
  Services.obs.notifyObservers(null, STATE_CHANGED_TOPIC, null);
  buttonState = yield buttonPromise;
  // In this case, the front end has called fxAccounts.getSignedInUser() to
  // update the button label and status.  But since there isn't actually a user,
  // the button is left with no fxastatus.
  assertButtonState(buttonState, "", true);
  Assert.ok(!Weave.Notifications.notifications.some(n => {
    return n.title == NOTIFICATION_TITLE;
  }), "Migration notifications should no longer be present");
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
