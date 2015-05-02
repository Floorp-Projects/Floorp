/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

const SMSC_ATT = '+13123149810';
const SMSC_ATT_TYPO = '+++1312@@@314$$$9,8,1,0';
const SMSC_ATT_TEXT = '"+13123149810",145';
const SMSC_ATT_PDU = '07913121139418F0';
const SMSC_O2 = '+447802000332';
const SMSC_O2_TEXT = '"+447802000332",145';
const SMSC_O2_PDU = '0791448720003023';
const SMSC_TON_UNKNOWN = '0407485455'
const SMSC_TON_UNKNOWN_TEXT = '"0407485455",129';
const SMSC_TON_UNKNOWN_PDU = '06814070844555';

function run_test() {
  run_next_test();
}

function setSmsc(context, smsc, ton, npi, expected) {
  context.Buf.sendParcel = function() {
    equal(this.readString(), expected);
  };

  context.RIL.setSmscAddress({
      smscAddress: smsc,
      typeOfNumber: ton,
      numberPlanIdentification: npi
  });
}

add_test(function test_setSmscAddress() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let parcelTypes = [];
  context.Buf.newParcel = (type, options) => parcelTypes.push(type);

  // Test text mode.
  worker.RILQUIRKS_SMSC_ADDRESS_FORMAT = "text";

  setSmsc(context, SMSC_ATT, 1, 1, SMSC_ATT_TEXT);
  equal(parcelTypes.pop(), REQUEST_SET_SMSC_ADDRESS);

  setSmsc(context, SMSC_O2, 1, 1, SMSC_O2_TEXT);
  equal(parcelTypes.pop(), REQUEST_SET_SMSC_ADDRESS);

  setSmsc(context, SMSC_ATT_TYPO, 1, 1, SMSC_ATT_TEXT);
  equal(parcelTypes.pop(), REQUEST_SET_SMSC_ADDRESS);

  setSmsc(context, SMSC_TON_UNKNOWN, 0, 1, SMSC_TON_UNKNOWN_TEXT);
  equal(parcelTypes.pop(), REQUEST_SET_SMSC_ADDRESS);

  // Test pdu mode.
  worker.RILQUIRKS_SMSC_ADDRESS_FORMAT = "pdu";

  setSmsc(context, SMSC_ATT, 1, 1, SMSC_ATT_PDU);
  equal(parcelTypes.pop(), REQUEST_SET_SMSC_ADDRESS);

  setSmsc(context, SMSC_O2, 1, 1, SMSC_O2_PDU);
  equal(parcelTypes.pop(), REQUEST_SET_SMSC_ADDRESS);

  setSmsc(context, SMSC_ATT_TYPO, 1, 1, SMSC_ATT_PDU);
  equal(parcelTypes.pop(), REQUEST_SET_SMSC_ADDRESS);

  setSmsc(context, SMSC_TON_UNKNOWN, 0, 1, SMSC_TON_UNKNOWN_PDU);
  equal(parcelTypes.pop(), REQUEST_SET_SMSC_ADDRESS);

  run_next_test();
});

