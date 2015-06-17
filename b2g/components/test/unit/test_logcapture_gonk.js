/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


/**
 * Test that LogCapture successfully reads from the /dev/log devices, returning
 * a Uint8Array of some length, including zero. This tests a few standard
 * log devices
 */
function run_test() {
  Components.utils.import("resource:///modules/LogCapture.jsm");
  run_next_test();
}

function verifyLog(log) {
  // log exists
  notEqual(log, null);
  // log has a length and it is non-negative (is probably array-like)
  ok(log.length >= 0);
}

add_test(function test_readLogFile() {
  let mainLog = LogCapture.readLogFile("/dev/log/main");
  verifyLog(mainLog);

  let meminfoLog = LogCapture.readLogFile("/proc/meminfo");
  verifyLog(meminfoLog);

  run_next_test();
});

add_test(function test_readProperties() {
  let propertiesLog = LogCapture.readProperties();
  notEqual(propertiesLog, null, "Properties should not be null");
  notEqual(propertiesLog, undefined, "Properties should not be undefined");

  for (let propertyName in propertiesLog) {
    equal(typeof(propertiesLog[propertyName]), "string",
          "Property " + propertyName + " should be a string");
  }

  equal(propertiesLog["ro.product.locale.language"], "en",
        "Locale language should be read correctly. See bug 1171577.");

  equal(propertiesLog["ro.product.locale.region"], "US",
        "Locale region should be read correctly. See bug 1171577.");

  run_next_test();
});

add_test(function test_readAppIni() {
  let appIni = LogCapture.readLogFile("/system/b2g/application.ini");
  verifyLog(appIni);

  run_next_test();
});

add_test(function test_get_about_memory() {
  let memLog = LogCapture.readAboutMemory();

  ok(memLog, "Should have returned a valid Promise object");

  memLog.then(file => {
    ok(file, "Should have returned a filename");
    run_next_test();
  }, error => {
    ok(false, "Dumping about:memory promise rejected: " + error);
    run_next_test();
  });
});
