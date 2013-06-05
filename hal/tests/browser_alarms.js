XPCOMUtils.defineLazyModuleGetter(this, "AlarmService",
                                  "resource://gre/modules/AlarmService.jsm");

/*
 * Tests for Bug 867868 and related Alarm API bugs.
 *
 * NOTE: These tests pass the alarm time to AlarmService as a number and not as
 * a date. See bug 810973 about Date truncating milliseconds. AlarmService does
 * not officially allow a integer, but nor does it disallow it. Of course this
 * test will break if AlarmService adds a type check, hence this note.
 * FIXME: when bug 810973 is fixed.
 */

function add_alarm_future(cb) {
  let alarmId = undefined;
  AlarmService.add({
    date: Date.now() + 143,
    ignoreTimezone: true
  },
  function onAlarmFired(aAlarm) {
    ok(alarmId === aAlarm.id, "Future alarm fired successfully.");
    cb();
  },
  function onSuccess(aAlarmId) {
    alarmId = aAlarmId;
  },
  function onError(error) {
    ok(false, "Unexpected error adding future alarm " + error);
    cb();
  });
}

function add_alarm_present(cb) {
  let self = this;
  let alarmId = undefined;
  AlarmService.add({
    date: Date.now(),
    ignoreTimezone: true
  },
  function onAlarmFired(aAlarm) {
    ok(alarmId === aAlarm.id, "Present alarm fired successfully.");
    cb();
  },
  function onSuccess(aAlarmId) {
    alarmId = aAlarmId;
  }, function onError(error) {
    ok(false, "Unexpected error adding alarm for current time " + error);
    cb();
  });
}

function add_alarm_past(cb) {
  let self = this;
  let alarmId = undefined;
  AlarmService.add({
    date: Date.now() - 5,
    ignoreTimezone: true
  },
  function onAlarmFired(aAlarm) {
    ok(alarmId === aAlarm.id, "Past alarm fired successfully.");
    cb();
  },
  function onSuccess(aAlarmId) {
    alarmId = aAlarmId;
  },
  function onError(error) {
    ok(false, "Unexpected error adding alarm for time in the past " + error);
    cb();
  });
}

function trigger_all_alarms(cb) {
  let n = 10;
  let counter = 0;
  let date = Date.now() + 57;
  function onAlarmFired() {
    counter++;
    info("trigger_all_alarms count " + counter);
    if (counter == n) {
      ok(true, "All " + n + " alarms set to a particular time fired.");
      cb();
    }
  }

  for (let i = 0; i < n; i++) {
    AlarmService.add(
      {
        date: date,
        ignoreTimezone: true
      },
      onAlarmFired
    );
  }
}

function multiple_handlers(cb) {
  let d = Date.now() + 100;
  let called = 0;

  function done() {
    if (called == 2) {
      ok(true, "Two alarms for the same time fired.");
      cb();
    }
  }

  function handler1() {
    called++;
    done();
  }

  function handler2() {
    called++;
    done();
  }

  AlarmService.add(
    {
      date: d,
      ignoreTimezone: true
    },
    handler1
  );
  AlarmService.add(
    {
      date: d,
      ignoreTimezone: true
    },
    handler2
  );
}

function same_time_alarms(cb) {
  var fired = 0;
  var delay = Date.now() + 100;

  function check() {
    fired++;
    if (fired == 4) {
      ok(true, "All alarms set for the same time fired.");
      cb();
    }
  }

  function addImmediateAlarm() {
    fired++;
    AlarmService.add({
      date: delay,
      ignoreTimezone: true
    }, check);
  }

  AlarmService.add({
    date: delay,
    ignoreTimezone: true
  }, addImmediateAlarm);

  AlarmService.add({
    date: delay,
    ignoreTimezone: true
  }, addImmediateAlarm);
}

function test() {
  var tests = [
    add_alarm_future,
    add_alarm_present,
    add_alarm_past,
    trigger_all_alarms,
    multiple_handlers,
    same_time_alarms
  ]

  var testIndex = -1;
  function nextTest() {
    testIndex++;
    if (testIndex >= tests.length)
      return;

    waitForExplicitFinish();
    tests[testIndex](function() {
      finish();
      nextTest();
    });
  }
  nextTest();
}
