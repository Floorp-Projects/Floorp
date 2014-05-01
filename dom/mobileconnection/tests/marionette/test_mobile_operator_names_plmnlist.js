/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_CELL_ID = 268435399; // The largest prime number that is smaller than
                                // 0xFFFFFFF (DEC 268435455). The next one is
                                // 268435459. This doesn't mean anything. ;)
                                // See http://primes.utm.edu/lists/small/millions/

function check(aLongName, aShortName, aMcc, aMnc, aLac, aCid) {
  let network = mobileConnection.voice.network;
  log("  Got longName '" + network.longName + "', shortName '" +
      network.shortName + "'");

  is(network.longName, aLongName, "network.longName");
  is(network.shortName, aShortName, "network.shortName");
  is(network.mcc, aMcc, "network.mcc");
  is(network.mnc, aMnc, "network.mnc");

  let cell = mobileConnection.voice.cell;

  is(cell.gsmLocationAreaCode, aLac, "cell.gsmLocationAreaCode");
  is(cell.gsmCellId, aCid, "cell.gsmCellId");
}

function test(aLongName, aShortName, aMcc, aMnc, aLac, aCid,
              aExpectedLongName, aExpectedShortName) {
  log("Testing mcc = " + aMcc + ", mnc = " + aMnc + ", lac = " + aLac + ":");

  return setEmulatorGsmLocationAndWait(aLac, aCid)
    .then(() => setEmulatorOperatorNamesAndWait("home", aLongName, aShortName,
                                                aMcc, aMnc, true, false))
    // aExpectedLongName, aExpectedShortName could be empty string.
    .then(() => check(aExpectedLongName == null ? aLongName : aExpectedLongName,
                      aExpectedShortName == null ? aShortName : aExpectedShortName,
                      aMcc, aMnc, aLac, aCid));
}

startTestCommon(function() {
  /**
   * In emulator we have pre-defined 4 PNN sets:
   *
   *   PNN 1: Full name: "Test1", Short name: "Test1"
   *   PNN 2: Full name: "Test2", Short name: (none)
   *   PNN 2: Full name: "Test3", Short name: (none)
   *   PNN 2: Full name: "Test4", Short name: (none)
   *
   * Also 4 OPL sets:
   *
   *   MCC = 001, MNC =  01, START=0000, END=FFFE, PNN = 01,
   *   MCC = 001, MNC =  02, START=0001, END=0010, PNN = 02,
   *   MCC = 001, MNC =  03, START=0011, END=0011, PNN = 03,
   *   MCC = 001, MNC = 001, START=0012, END=0012, PNN = 04,
   *
   * See https://github.com/mozilla-b2g/platform_external_qemu/blob/master/telephony/sim_card.c#L725
   */
  return getEmulatorOperatorNames()
    .then(function(aOperators) {
      let {longName: longName, shortName: shortName} = aOperators[0];
      let {mcc: mcc, mnc: mnc} = mobileConnection.voice.network;
      let {gsmLocationAreaCode: lac, gsmCellId: cid} = mobileConnection.voice.cell;

      // Use a cell ID that differs from current cid to ensure voicechange event
      // will be triggered.
      isnot(TEST_CELL_ID, cid, "A different test cell id than used currently.");

      // In following tests, we use different longName/shortName to ensure
      // network name is always re-calculated in RIL worker.
      return Promise.resolve()

        // If MCC/MNC doesn't match any, report given home network name.
        .then(() => test("Foo1", "Bar1", "123", "456", 0x0000, TEST_CELL_ID))
        .then(() => test("Foo2", "Bar2", "123", "456", 0x0001, TEST_CELL_ID))
        .then(() => test("Foo3", "Bar3", "123", "456", 0x0002, TEST_CELL_ID))
        .then(() => test("Foo4", "Bar4", "123", "456", 0x0010, TEST_CELL_ID))
        .then(() => test("Foo5", "Bar5", "123", "456", 0x0011, TEST_CELL_ID))
        .then(() => test("Foo6", "Bar6", "123", "456", 0xFFFE, TEST_CELL_ID))

        // Full ranged network.  Report network name from PNN.
        .then(() => test("Foo1", "Bar1", "001", "01", 0x0000, TEST_CELL_ID,
                         "Test1", "Test1"))
        .then(() => test("Foo2", "Bar2", "001", "01", 0x0001, TEST_CELL_ID,
                         "Test1", "Test1"))
        .then(() => test("Foo3", "Bar3", "001", "01", 0xFFFE, TEST_CELL_ID,
                         "Test1", "Test1"))

        // Ranged network.  Report network name from PNN if lac is inside the
        // inclusive range 0x01..0x10.
        .then(() => test("Foo1", "Bar1", "001", "02", 0x0000, TEST_CELL_ID))
        .then(() => test("Foo2", "Bar2", "001", "02", 0x0001, TEST_CELL_ID,
                         "Test2", ""))
        .then(() => test("Foo3", "Bar3", "001", "02", 0x0002, TEST_CELL_ID,
                         "Test2", ""))
        .then(() => test("Foo4", "Bar4", "001", "02", 0x0010, TEST_CELL_ID,
                         "Test2", ""))
        .then(() => test("Foo5", "Bar5", "001", "02", 0xFFFE, TEST_CELL_ID))

        // Single entry network.  Report network name from PNN if lac matches.
        .then(() => test("Foo1", "Bar1", "001", "03", 0x0000, TEST_CELL_ID))
        .then(() => test("Foo2", "Bar2", "001", "03", 0x0011, TEST_CELL_ID,
                         "Test3", ""))
        .then(() => test("Foo3", "Bar3", "001", "03", 0xFFFE, TEST_CELL_ID))

        // Test if we match MNC "01" and "001" correctly.
        .then(() => test("Foo1", "Bar1", "001", "001", 0x0012, TEST_CELL_ID,
                         "Test4", ""))

        // Reset back to initial values.
        .then(() => test(longName, shortName, mcc, mnc, lac, cid));
    });
});
