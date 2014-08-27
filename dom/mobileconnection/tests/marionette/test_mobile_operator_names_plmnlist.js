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
   * In emulator we have pre-defined 7 EF_PNN (see 3GPP TS 31.102 clause 4.2.58)
   * sets:
   *
   *   PNN 1: Full name: "Test1", Short name: "Test1"
   *   PNN 2: Full name: "Test2", Short name: (none)
   *   PNN 3: Full name: "Test3", Short name: (none)
   *   PNN 4: Full name: "Test4", Short name: (none)
   *   PNN 5: Full name: "Test5", Short name: (none)
   *   PNN 6: Full name: "Test6", Short name: (none)
   *   PNN 7: Full name: "Test7", Short name: (none)
   *
   * Also 7 EF_OPL (see 3GPP TS 31.102 clause 4.2.59) sets:
   *
   *   MCC = 001, MNC =  01, START=0000, END=FFFE, PNN = 01,
   *   MCC = 001, MNC =  02, START=0001, END=0010, PNN = 02,
   *   MCC = 001, MNC =  03, START=0011, END=0011, PNN = 03,
   *   MCC = 001, MNC = 001, START=0012, END=0012, PNN = 04,
   *   MCC = 001, MNC =  1D, START=0000, END=FFFE, PNN = 05,
   *   MCC = 001, MNC = 2DD, START=0000, END=FFFE, PNN = 06,
   *   MCC = 001, MNC = DDD, START=0000, END=FFFE, PNN = 07,
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

        // Wild char test for MCC = 001, MNC = 1D cases.
        .then(() => test("Foo10", "Bar10", "001", "10", 0x0000, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo11", "Bar11", "001", "11", 0x0001, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo12", "Bar12", "001", "12", 0x0002, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo13", "Bar13", "001", "13", 0x0003, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo14", "Bar14", "001", "14", 0x0004, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo15", "Bar15", "001", "15", 0x0005, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo16", "Bar16", "001", "16", 0x0006, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo17", "Bar17", "001", "17", 0x0007, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo18", "Bar18", "001", "18", 0x0008, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo19", "Bar19", "001", "19", 0x0009, TEST_CELL_ID,
                         "Test5", ""))
        .then(() => test("Foo20", "Bar20", "001", "20", 0x000A, TEST_CELL_ID))

        // Wild chars test for MCC = 001, MNC = 2DD cases.
        .then(() => test("Foo0", "Bar0", "001", "200", 0x00C8, TEST_CELL_ID,
                         "Test6", ""))
        .then(() => test("Foo1", "Bar1", "001", "299", 0x012B, TEST_CELL_ID,
                         "Test6", ""))

        // Wild chars test for MCC = 001, MNC = DDD cases.
        .then(() => test("Foo300", "Bar300", "001", "300", 0x012C, TEST_CELL_ID,
                         "Test7", ""))
        .then(() => test("Foo999", "Bar999", "001", "999", 0x03E7, TEST_CELL_ID,
                         "Test7", ""))

        // Reset back to initial values.
        .then(() => test(longName, shortName, mcc, mnc, lac, cid));
    });
});
