/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function testBody() {

  setWatchdogEnabled(true);

  // It's unlikely that we've ever hibernated at this point, but the timestamps
  // default to 0, so this should always be true.
  var now = Date.now() * 1000;
  var startHibernation = Cu.getWatchdogTimestamp("WatchdogHibernateStart");
  var stopHibernation = Cu.getWatchdogTimestamp("WatchdogHibernateStop");
  do_log_info("Pre-hibernation statistics:");
  do_log_info("now: " + now / 1000000);
  do_log_info("startHibernation: " + startHibernation / 1000000);
  do_log_info("stopHibernation: " + stopHibernation / 1000000);
  Assert.ok(startHibernation < now, "startHibernation ok");
  Assert.ok(stopHibernation < now, "stopHibernation ok");

  // When the watchdog runs, it hibernates if there's been no activity for the
  // last 2 seconds, otherwise it sleeps for 1 second. As such, given perfect
  // scheduling, we should never have more than 3 seconds of inactivity without
  // hibernating. To add some padding for automation, we mandate that hibernation
  // must begin between 2 and 5 seconds from now.
  await new Promise(resolve => {
    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(resolve, 10000, Ci.nsITimer.TYPE_ONE_SHOT);
    simulateActivityCallback(false);
  });

  simulateActivityCallback(true);
  busyWait(1000); // Give the watchdog time to wake up on the condvar.
  var stateChange = Cu.getWatchdogTimestamp("ContextStateChange");
  startHibernation = Cu.getWatchdogTimestamp("WatchdogHibernateStart");
  stopHibernation = Cu.getWatchdogTimestamp("WatchdogHibernateStop");
  do_log_info("Post-hibernation statistics:");
  do_log_info("stateChange: " + stateChange / 1000000);
  do_log_info("startHibernation: " + startHibernation / 1000000);
  do_log_info("stopHibernation: " + stopHibernation / 1000000);
  // XPCOM timers, JS times, and PR_Now() are apparently not directly
  // comparable, as evidenced by certain seemingly-impossible timing values
  // that occasionally get logged in windows automation. We're really just
  // making sure this behavior is roughly as expected on the macro scale,
  // so we add a 1 second fuzz factor here.
  const FUZZ_FACTOR = 1 * 1000 * 1000;
  Assert.ok(stateChange > now + 10*1000*1000 - FUZZ_FACTOR, "stateChange ok");
  Assert.ok(startHibernation > now + 2*1000*1000 - FUZZ_FACTOR, "startHibernation ok");
  Assert.ok(startHibernation < now + 5*1000*1000 + FUZZ_FACTOR, "startHibernation ok");
  Assert.ok(stopHibernation > now + 10*1000*1000 - FUZZ_FACTOR, "stopHibernation ok");
}
