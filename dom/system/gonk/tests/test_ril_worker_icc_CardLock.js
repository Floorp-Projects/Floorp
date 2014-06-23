/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Verify RIL.iccGetCardLockState("fdn")
 */
add_test(function test_icc_get_card_lock_state_fdn() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let buf = context.Buf;

  buf.sendParcel = function() {
    // Request Type.
    do_check_eq(this.readInt32(), REQUEST_QUERY_FACILITY_LOCK)

    // Token : we don't care.
    this.readInt32();

    // String Array Length.
    do_check_eq(this.readInt32(), ril.v5Legacy ? 3 : 4);

    // Facility.
    do_check_eq(this.readString(), ICC_CB_FACILITY_FDN);

    // Password.
    do_check_eq(this.readString(), "");

    // Service class.
    do_check_eq(this.readString(), (ICC_SERVICE_CLASS_VOICE |
                                    ICC_SERVICE_CLASS_DATA  |
                                    ICC_SERVICE_CLASS_FAX).toString());

    if (!ril.v5Legacy) {
      // AID. Ignore because it's from modem.
      this.readInt32();
    }

    run_next_test();
  };

  ril.iccGetCardLockState({lockType: "fdn"});
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
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_DF_GSM);
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPN),
              EF_PATH_MF_SIM + EF_PATH_DF_GSM);

  // Test USIM
  RIL.appType = CARD_APPTYPE_USIM;
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_ADF_USIM);
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_ADF_USIM);
  run_next_test();
});

add_test(function test_personalization_state() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  context.ICCRecordHelper.readICCID = function fakeReadICCID() {};

  function testPersonalization(isCdma, cardPersoState, geckoCardState) {
    let iccStatus = {
      cardState: CARD_STATE_PRESENT,
      gsmUmtsSubscriptionAppIndex: (!isCdma) ? 0 : -1,
      cdmaSubscriptionAppIndex: (isCdma) ? 0 : -1,
      apps: [
        {
          app_state: CARD_APPSTATE_SUBSCRIPTION_PERSO,
          perso_substate: cardPersoState
        }],
    };

    ril._isCdma = isCdma;
    ril._processICCStatus(iccStatus);
    do_check_eq(ril.cardState, geckoCardState);
  }

  // Test GSM personalization state.
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_NETWORK,
                      GECKO_CARDSTATE_NETWORK_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_CORPORATE,
                      GECKO_CARDSTATE_CORPORATE_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER,
                      GECKO_CARDSTATE_SERVICE_PROVIDER_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_NETWORK_PUK,
                      GECKO_CARDSTATE_NETWORK_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_CORPORATE_PUK,
                      GECKO_CARDSTATE_CORPORATE_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK,
                      GECKO_CARDSTATE_SERVICE_PROVIDER_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_READY,
                      GECKO_CARDSTATE_PERSONALIZATION_READY);

  // Test CDMA personalization state.
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK1,
                      GECKO_CARDSTATE_NETWORK1_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK2,
                      GECKO_CARDSTATE_NETWORK2_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_HRPD,
                      GECKO_CARDSTATE_HRPD_NETWORK_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_CORPORATE,
                      GECKO_CARDSTATE_RUIM_CORPORATE_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER,
                      GECKO_CARDSTATE_RUIM_SERVICE_PROVIDER_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_RUIM,
                      GECKO_CARDSTATE_RUIM_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK1_PUK,
                      GECKO_CARDSTATE_NETWORK1_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK2_PUK,
                      GECKO_CARDSTATE_NETWORK2_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_HRPD_PUK,
                      GECKO_CARDSTATE_HRPD_NETWORK_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_CORPORATE_PUK,
                      GECKO_CARDSTATE_RUIM_CORPORATE_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER_PUK,
                      GECKO_CARDSTATE_RUIM_SERVICE_PROVIDER_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_RUIM_PUK,
                      GECKO_CARDSTATE_RUIM_PUK_REQUIRED);

  run_next_test();
});

/**
 * Verify SIM app_state in _processICCStatus
 */
add_test(function test_card_app_state() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  context.ICCRecordHelper.readICCID = function fakeReadICCID() {};

  function testCardAppState(cardAppState, geckoCardState) {
    let iccStatus = {
      cardState: CARD_STATE_PRESENT,
      gsmUmtsSubscriptionAppIndex: 0,
      apps: [
      {
        app_state: cardAppState
      }],
    };

    ril._processICCStatus(iccStatus);
    do_check_eq(ril.cardState, geckoCardState);
  }

  testCardAppState(CARD_APPSTATE_ILLEGAL,
                   GECKO_CARDSTATE_ILLEGAL);
  testCardAppState(CARD_APPSTATE_PIN,
                   GECKO_CARDSTATE_PIN_REQUIRED);
  testCardAppState(CARD_APPSTATE_PUK,
                   GECKO_CARDSTATE_PUK_REQUIRED);
  testCardAppState(CARD_APPSTATE_READY,
                   GECKO_CARDSTATE_READY);
  testCardAppState(CARD_APPSTATE_UNKNOWN,
                   GECKO_CARDSTATE_UNKNOWN);
  testCardAppState(CARD_APPSTATE_DETECTED,
                   GECKO_CARDSTATE_UNKNOWN);

  run_next_test();
});

/**
 * Verify permanent blocked for ICC.
 */
add_test(function test_icc_permanent_blocked() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  context.ICCRecordHelper.readICCID = function fakeReadICCID() {};

  function testPermanentBlocked(pin1_replaced, universalPINState, pin1) {
    let iccStatus = {
      cardState: CARD_STATE_PRESENT,
      gsmUmtsSubscriptionAppIndex: 0,
      universalPINState: universalPINState,
      apps: [
      {
        pin1_replaced: pin1_replaced,
        pin1: pin1
      }]
    };

    ril._processICCStatus(iccStatus);
    do_check_eq(ril.cardState, GECKO_CARDSTATE_PERMANENT_BLOCKED);
  }

  testPermanentBlocked(1,
                       CARD_PINSTATE_ENABLED_PERM_BLOCKED,
                       CARD_PINSTATE_UNKNOWN);
  testPermanentBlocked(1,
                       CARD_PINSTATE_ENABLED_PERM_BLOCKED,
                       CARD_PINSTATE_ENABLED_PERM_BLOCKED);
  testPermanentBlocked(0,
                       CARD_PINSTATE_UNKNOWN,
                       CARD_PINSTATE_ENABLED_PERM_BLOCKED);

  run_next_test();
});

/**
 * Verify iccSetCardLock - Facility Lock.
 */
add_test(function test_set_icc_card_lock_facility_lock() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let aid = "123456789";
  let ril = context.RIL;
  ril.aid = aid;
  ril.v5Legacy = false;
  let buf = context.Buf;

  let GECKO_CARDLOCK_TO_FACILITIY_LOCK = {};
  GECKO_CARDLOCK_TO_FACILITIY_LOCK[GECKO_CARDLOCK_PIN] = ICC_CB_FACILITY_SIM;
  GECKO_CARDLOCK_TO_FACILITIY_LOCK[GECKO_CARDLOCK_FDN] = ICC_CB_FACILITY_FDN;

  let GECKO_CARDLOCK_TO_PASSWORD_TYPE = {};
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_PIN] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_FDN] = "pin2";

  const pin = "1234";
  const pin2 = "4321";
  let GECKO_CARDLOCK_TO_PASSWORD = {};
  GECKO_CARDLOCK_TO_PASSWORD[GECKO_CARDLOCK_PIN] = pin;
  GECKO_CARDLOCK_TO_PASSWORD[GECKO_CARDLOCK_FDN] = pin2;

  const serviceClass = ICC_SERVICE_CLASS_VOICE |
                       ICC_SERVICE_CLASS_DATA  |
                       ICC_SERVICE_CLASS_FAX;

  function do_test(aLock, aPassword, aEnabled) {
    buf.sendParcel = function fakeSendParcel () {
      // Request Type.
      do_check_eq(this.readInt32(), REQUEST_SET_FACILITY_LOCK);

      // Token : we don't care
      this.readInt32();

      let parcel = this.readStringList();
      do_check_eq(parcel.length, 5);
      do_check_eq(parcel[0], GECKO_CARDLOCK_TO_FACILITIY_LOCK[aLock]);
      do_check_eq(parcel[1], aEnabled ? "1" : "0");
      do_check_eq(parcel[2], GECKO_CARDLOCK_TO_PASSWORD[aLock]);
      do_check_eq(parcel[3], serviceClass.toString());
      do_check_eq(parcel[4], aid);
    };

    let lock = {lockType: aLock,
                enabled: aEnabled};
    lock[GECKO_CARDLOCK_TO_PASSWORD_TYPE[aLock]] = aPassword;

    ril.iccSetCardLock(lock);
  }

  do_test(GECKO_CARDLOCK_PIN, pin, true);
  do_test(GECKO_CARDLOCK_PIN, pin, false);
  do_test(GECKO_CARDLOCK_FDN, pin2, true);
  do_test(GECKO_CARDLOCK_FDN, pin2, false);

  run_next_test();
});

/**
 * Verify iccUnlockCardLock.
 */
add_test(function test_unlock_card_lock_corporateLocked() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let buf = context.Buf;
  const pin = "12345678";
  const puk = "12345678";

  let GECKO_CARDLOCK_TO_PASSWORD_TYPE = {};
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK1] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK2] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_HNCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_CCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_SPCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_RCCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_RSPCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK1_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK2_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_HNCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_CCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_SPCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_RCCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_RSPCK_PUK] = "puk";

  function do_test(aLock, aPassword) {
    buf.sendParcel = function fakeSendParcel () {
      // Request Type.
      do_check_eq(this.readInt32(), REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE);

      // Token : we don't care
      this.readInt32();

      let lockType = GECKO_PERSO_LOCK_TO_CARD_PERSO_LOCK[aLock];
      // Lock Type
      do_check_eq(this.readInt32(), lockType);

      // Pin/Puk.
      do_check_eq(this.readString(), aPassword);
    };

    let lock = {lockType: aLock};
    lock[GECKO_CARDLOCK_TO_PASSWORD_TYPE[aLock]] = aPassword;
    ril.iccUnlockCardLock(lock);
  }

  do_test(GECKO_CARDLOCK_NCK, pin);
  do_test(GECKO_CARDLOCK_NCK1, pin);
  do_test(GECKO_CARDLOCK_NCK2, pin);
  do_test(GECKO_CARDLOCK_HNCK, pin);
  do_test(GECKO_CARDLOCK_CCK, pin);
  do_test(GECKO_CARDLOCK_SPCK, pin);
  do_test(GECKO_CARDLOCK_RCCK, pin);
  do_test(GECKO_CARDLOCK_RSPCK, pin);
  do_test(GECKO_CARDLOCK_NCK_PUK, puk);
  do_test(GECKO_CARDLOCK_NCK1_PUK, puk);
  do_test(GECKO_CARDLOCK_NCK2_PUK, puk);
  do_test(GECKO_CARDLOCK_HNCK_PUK, puk);
  do_test(GECKO_CARDLOCK_CCK_PUK, puk);
  do_test(GECKO_CARDLOCK_SPCK_PUK, puk);
  do_test(GECKO_CARDLOCK_RCCK_PUK, puk);
  do_test(GECKO_CARDLOCK_RSPCK_PUK, puk);

  run_next_test();
});
