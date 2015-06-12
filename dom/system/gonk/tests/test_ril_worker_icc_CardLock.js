/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Verify RIL.iccGetCardLockEnabled
 */
add_test(function test_icc_get_card_lock_enabled() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let ril = context.RIL;
  ril.aid = "123456789";

  function do_test(aLock) {
    const serviceClass = ICC_SERVICE_CLASS_VOICE |
                         ICC_SERVICE_CLASS_DATA  |
                         ICC_SERVICE_CLASS_FAX;

    buf.sendParcel = function fakeSendParcel() {
      // Request Type.
      equal(this.readInt32(), REQUEST_QUERY_FACILITY_LOCK)

      // Token : we don't care.
      this.readInt32();

      // Data
      let parcel = this.readStringList();
      equal(parcel.length, 4);
      equal(parcel[0], GECKO_CARDLOCK_TO_FACILITY[aLock]);
      equal(parcel[1], "");
      equal(parcel[2], serviceClass.toString());
      equal(parcel[3], ril.aid);
    };

    ril.iccGetCardLockEnabled({lockType: aLock});
  }

  do_test(GECKO_CARDLOCK_PIN)
  do_test(GECKO_CARDLOCK_FDN)

  run_next_test();
});

add_test(function test_path_id_for_spid_and_spn() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }});
  let context = worker.ContextPool._contexts[0];
  let RIL = context.RIL;
  let ICCFileHelper = context.ICCFileHelper;

  // Test SIM
  RIL.appType = CARD_APPTYPE_SIM;
  equal(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_DF_GSM);
  equal(ICCFileHelper.getEFPath(ICC_EF_SPN),
              EF_PATH_MF_SIM + EF_PATH_DF_GSM);

  // Test USIM
  RIL.appType = CARD_APPTYPE_USIM;
  equal(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_ADF_USIM);
  equal(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_ADF_USIM);
  run_next_test();
});

/**
 * Verify RIL.iccSetCardLockEnabled
 */
add_test(function test_icc_set_card_lock_enabled() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let ril = context.RIL;
  ril.aid = "123456789";

  function do_test(aLock, aPassword, aEnabled) {
    const serviceClass = ICC_SERVICE_CLASS_VOICE |
                         ICC_SERVICE_CLASS_DATA  |
                         ICC_SERVICE_CLASS_FAX;

    buf.sendParcel = function fakeSendParcel() {
      // Request Type.
      equal(this.readInt32(), REQUEST_SET_FACILITY_LOCK);

      // Token : we don't care
      this.readInt32();

      // Data
      let parcel = this.readStringList();
      equal(parcel.length, 5);
      equal(parcel[0], GECKO_CARDLOCK_TO_FACILITY[aLock]);
      equal(parcel[1], aEnabled ? "1" : "0");
      equal(parcel[2], aPassword);
      equal(parcel[3], serviceClass.toString());
      equal(parcel[4], ril.aid);
    };

    ril.iccSetCardLockEnabled({
      lockType: aLock,
      enabled: aEnabled,
      password: aPassword});
  }

  do_test(GECKO_CARDLOCK_PIN, "1234", true);
  do_test(GECKO_CARDLOCK_PIN, "1234", false);
  do_test(GECKO_CARDLOCK_FDN, "4321", true);
  do_test(GECKO_CARDLOCK_FDN, "4321", false);

  run_next_test();
});

/**
 * Verify RIL.iccChangeCardLockPassword
 */
add_test(function test_icc_change_card_lock_password() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let ril = context.RIL;


  function do_test(aLock, aPassword, aNewPassword) {
    let GECKO_CARDLOCK_TO_REQUEST = {};
    GECKO_CARDLOCK_TO_REQUEST[GECKO_CARDLOCK_PIN] = REQUEST_CHANGE_SIM_PIN;
    GECKO_CARDLOCK_TO_REQUEST[GECKO_CARDLOCK_PIN2] = REQUEST_CHANGE_SIM_PIN2;

    buf.sendParcel = function fakeSendParcel() {
      // Request Type.
      equal(this.readInt32(), GECKO_CARDLOCK_TO_REQUEST[aLock]);

      // Token : we don't care
      this.readInt32();

      // Data
      let parcel = this.readStringList();
      equal(parcel.length, 3);
      equal(parcel[0], aPassword);
      equal(parcel[1], aNewPassword);
      equal(parcel[2], ril.aid);
    };

    ril.iccChangeCardLockPassword({
      lockType: aLock,
      password: aPassword,
      newPassword: aNewPassword});
  }

  do_test(GECKO_CARDLOCK_PIN, "1234", "4321");
  do_test(GECKO_CARDLOCK_PIN2, "1234", "4321");

  run_next_test();
});

/**
 * Verify RIL.iccUnlockCardLock - PIN
 */
add_test(function test_icc_unlock_card_lock_pin() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let buf = context.Buf;
  ril.aid = "123456789";

  function do_test(aLock, aPassword) {
    let GECKO_CARDLOCK_TO_REQUEST = {};
    GECKO_CARDLOCK_TO_REQUEST[GECKO_CARDLOCK_PIN] = REQUEST_ENTER_SIM_PIN;
    GECKO_CARDLOCK_TO_REQUEST[GECKO_CARDLOCK_PIN2] = REQUEST_ENTER_SIM_PIN2;

    buf.sendParcel = function fakeSendParcel() {
      // Request Type.
      equal(this.readInt32(), GECKO_CARDLOCK_TO_REQUEST[aLock]);

      // Token : we don't care
      this.readInt32();

      // Data
      let parcel = this.readStringList();
      equal(parcel.length, 2);
      equal(parcel[0], aPassword);
      equal(parcel[1], ril.aid);
    };

    ril.iccUnlockCardLock({
      lockType: aLock,
      password: aPassword
    });
  }

  do_test(GECKO_CARDLOCK_PIN, "1234");
  do_test(GECKO_CARDLOCK_PIN2, "1234");

  run_next_test();
});

/**
 * Verify RIL.iccUnlockCardLock - PUK
 */
add_test(function test_icc_unlock_card_lock_puk() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let buf = context.Buf;
  ril.aid = "123456789";

  function do_test(aLock, aPassword, aNewPin) {
    let GECKO_CARDLOCK_TO_REQUEST = {};
    GECKO_CARDLOCK_TO_REQUEST[GECKO_CARDLOCK_PUK] = REQUEST_ENTER_SIM_PUK;
    GECKO_CARDLOCK_TO_REQUEST[GECKO_CARDLOCK_PUK2] = REQUEST_ENTER_SIM_PUK2;

    buf.sendParcel = function fakeSendParcel() {
      // Request Type.
      equal(this.readInt32(), GECKO_CARDLOCK_TO_REQUEST[aLock]);

      // Token : we don't care
      this.readInt32();

      // Data
      let parcel = this.readStringList();
      equal(parcel.length, 3);
      equal(parcel[0], aPassword);
      equal(parcel[1], aNewPin);
      equal(parcel[2], ril.aid);
    };

    ril.iccUnlockCardLock({
      lockType: aLock,
      password: aPassword,
      newPin: aNewPin
    });
  }

  do_test(GECKO_CARDLOCK_PUK, "12345678", "1234");
  do_test(GECKO_CARDLOCK_PUK2, "12345678", "1234");

  run_next_test();
});

/**
 * Verify RIL.iccUnlockCardLock - Depersonalization
 */
add_test(function test_icc_unlock_card_lock_depersonalization() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let buf = context.Buf;

  function do_test(aPassword) {
    buf.sendParcel = function fakeSendParcel() {
      // Request Type.
      equal(this.readInt32(), REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE);

      // Token : we don't care
      this.readInt32();

      // Data
      let parcel = this.readStringList();
      equal(parcel.length, 1);
      equal(parcel[0], aPassword);
    };

    ril.iccUnlockCardLock({
      lockType: GECKO_CARDLOCK_NCK,
      password: aPassword
    });
  }

  do_test("12345678");

  run_next_test();
});
