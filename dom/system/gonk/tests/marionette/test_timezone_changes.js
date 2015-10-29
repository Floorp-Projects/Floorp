/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function init() {
  let promises = [];

  /*
   * The initial timezone of the emulator could be anywhere, depends the host
   * machine. Ensure resetting it to UTC before testing.
   */
  promises.push(runEmulatorCmdSafe('gsm timezone 0'));
  promises.push(new Promise((aResolve, aReject) => {
    waitFor(aResolve, () => {
      return new Date().getTimezoneOffset() === 0;
    });
  }));

  return Promise.all(promises);
}

function paddingZeros(aNumber, aLength) {
  let str = '' + aNumber;
  while (str.length < aLength) {
      str = '0' + str;
  }

  return str;
}

function verifyDate(aTestDate, aUTCOffsetDate) {
  // Verify basic properties.
  is(aUTCOffsetDate.getUTCFullYear(), aTestDate.getFullYear(), 'year');
  is(aUTCOffsetDate.getUTCMonth(), aTestDate.getMonth(), 'month');
  is(aUTCOffsetDate.getUTCDate(), aTestDate.getDate(), 'date');
  is(aUTCOffsetDate.getUTCHours(), aTestDate.getHours(), 'hours');
  is(aUTCOffsetDate.getUTCMinutes(), aTestDate.getMinutes(), 'minutes');
  is(aUTCOffsetDate.getUTCMilliseconds(), aTestDate.getMilliseconds(), 'milliseconds');

  // Ensure toLocaleString also uses correct timezone.
  // It uses ICU's timezone instead of the offset calculated from gecko prtime.
  let expectedDateString =
    paddingZeros(aUTCOffsetDate.getUTCMonth() + 1, 2) + '/' +
    paddingZeros(aUTCOffsetDate.getUTCDate(), 2);
  let dateString = aTestDate.toLocaleString('en-US', {
    month: '2-digit',
    day: '2-digit',
  });
  let expectedTimeString =
    paddingZeros(aUTCOffsetDate.getUTCHours(), 2) + ':' +
    paddingZeros(aUTCOffsetDate.getUTCMinutes(), 2);
  let timeString = aTestDate.toLocaleString('en-US', {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit'
  });

  is(expectedDateString, dateString, 'dateString');
  is(expectedTimeString, timeString, 'timeString');
}

function waitForTimezoneUpdate(aTzOffset,
  aTestDateInMillis = 86400000, // Use 'UTC 00:00:00, 2nd of Jan, 1970' by default.
  aTransTzOffset, aTransTestDateInMillis) {
  return new Promise(function(aResolve, aReject) {
    window.addEventListener('moztimechange', function onevent(aEvent) {
      // Since there could be multiple duplicate moztimechange event, wait until
      // timezone is actually changed to expected value before removing the
      // listener.
      let testDate = new Date(aTestDateInMillis);
      if (testDate.getTimezoneOffset() === aTzOffset) {
        window.removeEventListener('moztimechange', onevent);

        // The UTC time of offsetDate is the same as the expected local time of
        // testDate. We'll use it to verify the values.
        let offsetDate = new Date(aTestDateInMillis - aTzOffset * 60 * 1000);
        verifyDate(testDate, offsetDate);

        // Verify transition time if given.
        if (aTransTzOffset !== undefined) {
          testDate = new Date(aTransTestDateInMillis);
          is(testDate.getTimezoneOffset(), aTransTzOffset);

          // Verify transition date.
          offsetDate = new Date(aTransTestDateInMillis - aTransTzOffset * 60 * 1000);
          verifyDate(testDate, offsetDate);
        }

        aResolve(aEvent);
      }
    });
  });
}

function testChangeNitzTimezone(aTzDiff) {
  let promises = [];

  // aTzOffset should be the expected value for getTimezoneOffset().
  // Note that getTimezoneOffset() is not so straightforward,
  // it values (UTC - localtime), so UTC+08:00 returns -480.
  promises.push(waitForTimezoneUpdate(-aTzDiff * 15));
  promises.push(runEmulatorCmdSafe('gsm timezone ' + aTzDiff));

  return Promise.all(promises);
}

function testChangeOlsonTimezone(aOlsonTz, aTzOffset, aTestDateInMillis,
  aTransTzOffset, aTransTestDateInMillis) {
  let promises = [];

  promises.push(waitForTimezoneUpdate(aTzOffset, aTestDateInMillis,
      aTransTzOffset, aTransTestDateInMillis));
  promises.push(setSettings('time.timezone', aOlsonTz));

  return Promise.all(promises);
}

// Start test
startTestBase(function() {
  return init()
    .then(() => testChangeNitzTimezone(36))  // UTC+09:00
    .then(() => testChangeOlsonTimezone('America/New_York',
      300, 1446357600000,   // 2015/11/01 02:00 UTC-04:00 => 01:00 UTC-05:00 (EST)
      240, 1425798000000))  // 2015/03/08 02:00 UTC-05:00 => 03:00 UTC-04:00 (EDT)
    .then(() => testChangeNitzTimezone(-22)) // UTC-05:30
    .then(() => testChangeNitzTimezone(51))  // UTC+12:45
    .then(() => testChangeOlsonTimezone('Australia/Adelaide',
      -570, 1428165000000,  // 2015/04/05 03:00 UTC+10:30 => 02:00 UTC+09:30 (ACST)
      -630, 1443889800000)) // 2015/10/04 02:00 UTC+09:30 => 03:00 UTC+10:30 (ACDT)
    .then(() => testChangeNitzTimezone(-38)) // UTC-09:30
    .then(() => testChangeNitzTimezone(0))   // UTC
    .then(() => runEmulatorCmdSafe('gsm timezone auto'));
});
