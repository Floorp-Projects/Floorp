/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

const ESCAPE = "\uffff";
const RESCTL = "\ufffe";
const LF = "\n";
const CR = "\r";
const SP = " ";
const FF = "\u000c";

function run_test() {
  run_next_test();
}

/**
 * Verify validity of the national language tables
 */
add_test(function test_nl_locking_shift_tables_validity() {
  for (let lst = 0; lst < PDU_NL_LOCKING_SHIFT_TABLES.length; lst++) {
    do_print("Verifying PDU_NL_LOCKING_SHIFT_TABLES[" + lst + "]");

    let table = PDU_NL_LOCKING_SHIFT_TABLES[lst];

    // Make sure table length is 128, or it will break table lookup algorithm.
    equal(table.length, 128);

    // Make sure special values are preserved.
    equal(table[PDU_NL_EXTENDED_ESCAPE], ESCAPE);
    equal(table[PDU_NL_LINE_FEED], LF);
    equal(table[PDU_NL_CARRIAGE_RETURN], CR);
    equal(table[PDU_NL_SPACE], SP);
  }

  run_next_test();
});

add_test(function test_nl_single_shift_tables_validity() {
  for (let sst = 0; sst < PDU_NL_SINGLE_SHIFT_TABLES.length; sst++) {
    do_print("Verifying PDU_NL_SINGLE_SHIFT_TABLES[" + sst + "]");

    let table = PDU_NL_SINGLE_SHIFT_TABLES[sst];

    // Make sure table length is 128, or it will break table lookup algorithm.
    equal(table.length, 128);

    // Make sure special values are preserved.
    equal(table[PDU_NL_EXTENDED_ESCAPE], ESCAPE);
    equal(table[PDU_NL_PAGE_BREAK], FF);
    equal(table[PDU_NL_RESERVED_CONTROL], RESCTL);
  }

  run_next_test();
});

add_test(function test_gsm_sms_strict_7bit_charmap_validity() {
  let defaultTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  let defaultShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  for (let from in GSM_SMS_STRICT_7BIT_CHARMAP) {
    let to = GSM_SMS_STRICT_7BIT_CHARMAP[from];
    do_print("Verifying GSM_SMS_STRICT_7BIT_CHARMAP[\"\\u0x"
             + from.charCodeAt(0).toString(16) + "\"] => \"\\u"
             + to.charCodeAt(0).toString(16) + "\"");

    // Make sure "from" is not in default table
    equal(defaultTable.indexOf(from), -1);
    equal(defaultShiftTable.indexOf(from), -1);
    // Make sure "to" is in default table
    if ((defaultTable.indexOf(to) < 0)
        && (defaultShiftTable.indexOf(to) < 0)) {
      equal(false, true);
    }
  }

  run_next_test();
});
