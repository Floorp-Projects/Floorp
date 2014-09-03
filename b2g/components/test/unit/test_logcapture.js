/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


/**
 * Test that LogCapture successfully reads from the /dev/log devices, returning
 * a Uint8Array of some length, including zero. This tests a few standard
 * log devices
 */
function run_test() {
  Components.utils.import('resource:///modules/LogCapture.jsm');

  function verifyLog(log) {
    // log exists
    notEqual(log, null);
    // log has a length and it is non-negative (is probably array-like)
    ok(log.length >= 0);
  }

  let mainLog = LogCapture.readLogFile('/dev/log/main');
  verifyLog(mainLog);

  let meminfoLog = LogCapture.readLogFile('/proc/meminfo');
  verifyLog(meminfoLog);
}
