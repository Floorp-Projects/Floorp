/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const STATE_CHANGED_TOPIC = "fxa-migration:state-changed";
const NOTIFICATION_TITLE = "fxa-migration";

var imports = {};
Cu.import("resource://services-sync/FxaMigrator.jsm", imports);

add_task(function* test() {
  // Fake the state where we saw an EOL notification.
  Services.obs.notifyObservers(null, STATE_CHANGED_TOPIC, null);

  let notificationBox = document.getElementById("global-notificationbox");
  Assert.ok(notificationBox.allNotifications.some(n => {
    return n.getAttribute("value") == NOTIFICATION_TITLE;
  }), "Disconnect notification should be present");
});
