/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

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
    equal(ril.cardState, geckoCardState);
  }

  // Test GSM personalization state.
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_NETWORK,
                      Ci.nsIIcc.CARD_STATE_NETWORK_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET,
                      Ci.nsIIcc.CARD_STATE_NETWORK_SUBSET_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_CORPORATE,
                      Ci.nsIIcc.CARD_STATE_CORPORATE_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER,
                      Ci.nsIIcc.CARD_STATE_SERVICE_PROVIDER_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_SIM,
                      Ci.nsIIcc.CARD_STATE_SIM_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_NETWORK_PUK,
                      Ci.nsIIcc.CARD_STATE_NETWORK_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET_PUK,
                      Ci.nsIIcc.CARD_STATE_NETWORK_SUBSET_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_CORPORATE_PUK,
                      Ci.nsIIcc.CARD_STATE_CORPORATE_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK,
                      Ci.nsIIcc.CARD_STATE_SERVICE_PROVIDER_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_SIM_PUK,
                      Ci.nsIIcc.CARD_STATE_SIM_PUK_REQUIRED);

  testPersonalization(false, CARD_PERSOSUBSTATE_UNKNOWN,
                      Ci.nsIIcc.CARD_STATE_UNKNOWN);
  testPersonalization(false, CARD_PERSOSUBSTATE_IN_PROGRESS,
                      Ci.nsIIcc.CARD_STATE_PERSONALIZATION_IN_PROGRESS);
  testPersonalization(false, CARD_PERSOSUBSTATE_READY,
                      Ci.nsIIcc.CARD_STATE_PERSONALIZATION_READY);

  // Test CDMA personalization state.
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK1,
                      Ci.nsIIcc.CARD_STATE_NETWORK1_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK2,
                      Ci.nsIIcc.CARD_STATE_NETWORK2_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_HRPD,
                      Ci.nsIIcc.CARD_STATE_HRPD_NETWORK_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_CORPORATE,
                      Ci.nsIIcc.CARD_STATE_RUIM_CORPORATE_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER,
                      Ci.nsIIcc.CARD_STATE_RUIM_SERVICE_PROVIDER_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_RUIM,
                      Ci.nsIIcc.CARD_STATE_RUIM_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK1_PUK,
                      Ci.nsIIcc.CARD_STATE_NETWORK1_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK2_PUK,
                      Ci.nsIIcc.CARD_STATE_NETWORK2_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_HRPD_PUK,
                      Ci.nsIIcc.CARD_STATE_HRPD_NETWORK_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_CORPORATE_PUK,
                      Ci.nsIIcc.CARD_STATE_RUIM_CORPORATE_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER_PUK,
                      Ci.nsIIcc.CARD_STATE_RUIM_SERVICE_PROVIDER_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_RUIM_PUK,
                      Ci.nsIIcc.CARD_STATE_RUIM_PUK_REQUIRED);

  testPersonalization(true, CARD_PERSOSUBSTATE_UNKNOWN,
                      Ci.nsIIcc.CARD_STATE_UNKNOWN);
  testPersonalization(true, CARD_PERSOSUBSTATE_IN_PROGRESS,
                      Ci.nsIIcc.CARD_STATE_PERSONALIZATION_IN_PROGRESS);
  testPersonalization(true, CARD_PERSOSUBSTATE_READY,
                      Ci.nsIIcc.CARD_STATE_PERSONALIZATION_READY);

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
    equal(ril.cardState, geckoCardState);
  }

  testCardAppState(CARD_APPSTATE_ILLEGAL,
                   Ci.nsIIcc.CARD_STATE_ILLEGAL);
  testCardAppState(CARD_APPSTATE_PIN,
                   Ci.nsIIcc.CARD_STATE_PIN_REQUIRED);
  testCardAppState(CARD_APPSTATE_PUK,
                   Ci.nsIIcc.CARD_STATE_PUK_REQUIRED);
  testCardAppState(CARD_APPSTATE_READY,
                   Ci.nsIIcc.CARD_STATE_READY);
  testCardAppState(CARD_APPSTATE_UNKNOWN,
                   Ci.nsIIcc.CARD_STATE_UNKNOWN);
  testCardAppState(CARD_APPSTATE_DETECTED,
                   Ci.nsIIcc.CARD_STATE_UNKNOWN);

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
    equal(ril.cardState, Ci.nsIIcc.CARD_STATE_PERMANENT_BLOCKED);
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
 * Verify ICC without app index.
 */
add_test(function test_icc_without_app_index() {
  const ICCID = "123456789";

  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  let iccStatus = {
    cardState: CARD_STATE_PRESENT,
    gsmUmtsSubscriptionAppIndex: -1,
    universalPINState: CARD_PINSTATE_DISABLED,
    apps: [
    {
      app_state: CARD_APPSTATE_READY
    }]
  };

  context.ICCRecordHelper.readICCID = function fakeReadICCID() {
    ril.iccInfo.iccid = ICCID;
  };

  ril._processICCStatus(iccStatus);

  // Should read icc id event if the app index is -1.
  equal(ril.iccInfo.iccid, ICCID);
  // cardState is "unknown" if the app index is -1.
  equal(ril.cardState, GECKO_CARDSTATE_UNKNOWN);

  run_next_test();
});
