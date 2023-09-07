/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env webextensions */

ChromeUtils.defineESModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
});

const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PBM_PREF = "privacy.trackingprotection.pbmode.enabled";
const NCB_PREF = "network.cookie.cookieBehavior";
const NCBP_PREF = "network.cookie.cookieBehavior.pbmode";
const CAT_PREF = "browser.contentblocking.category";
const FP_PREF = "privacy.trackingprotection.fingerprinting.enabled";
const CM_PREF = "privacy.trackingprotection.cryptomining.enabled";
const STP_PREF = "privacy.trackingprotection.socialtracking.enabled";
const EMAIL_TP_PREF = "privacy.trackingprotection.emailtracking.enabled";
const EMAIL_TP_PBM_PREF =
  "privacy.trackingprotection.emailtracking.pbmode.enabled";
const LEVEL2_PREF = "privacy.annotate_channels.strict_list.enabled";
const REFERRER_PREF = "network.http.referer.disallowCrossSiteRelaxingDefault";
const REFERRER_TOP_PREF =
  "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation";
const OCSP_PREF = "privacy.partition.network_state.ocsp_cache";
const QUERY_PARAM_STRIP_PREF = "privacy.query_stripping.enabled";
const QUERY_PARAM_STRIP_PBM_PREF = "privacy.query_stripping.enabled.pbmode";
const FPP_PREF = "privacy.fingerprintingProtection";
const FPP_PBM_PREF = "privacy.fingerprintingProtection.pbmode";
const STRICT_DEF_PREF = "browser.contentblocking.features.strict";

// Tests that the content blocking standard category definition is based on the default settings of
// the content blocking prefs.
// Changing the definition does not remove the user from the category.
add_task(async function testContentBlockingStandardDefinition() {
  Services.prefs.setStringPref(CAT_PREF, "strict");
  Services.prefs.setStringPref(CAT_PREF, "standard");
  is(
    Services.prefs.getStringPref(CAT_PREF),
    "standard",
    `${CAT_PREF} starts on standard`
  );

  ok(
    !Services.prefs.prefHasUserValue(TP_PREF),
    `${TP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(TP_PBM_PREF),
    `${TP_PBM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(FP_PREF),
    `${FP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(CM_PREF),
    `${CM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(STP_PREF),
    `${STP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(EMAIL_TP_PREF),
    `${EMAIL_TP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(EMAIL_TP_PBM_PREF),
    `${EMAIL_TP_PBM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(NCB_PREF),
    `${NCB_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(NCBP_PREF),
    `${NCBP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(LEVEL2_PREF),
    `${LEVEL2_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(REFERRER_PREF),
    `${REFERRER_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(REFERRER_TOP_PREF),
    `${REFERRER_TOP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(OCSP_PREF),
    `${OCSP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(QUERY_PARAM_STRIP_PREF),
    `${QUERY_PARAM_STRIP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(QUERY_PARAM_STRIP_PBM_PREF),
    `${QUERY_PARAM_STRIP_PBM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(FPP_PREF),
    `${FPP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(FPP_PBM_PREF),
    `${FPP_PBM_PREF} pref has the default value`
  );

  let defaults = Services.prefs.getDefaultBranch("");
  let originalTP = defaults.getBoolPref(TP_PREF);
  let originalTPPBM = defaults.getBoolPref(TP_PBM_PREF);
  let originalFP = defaults.getBoolPref(FP_PREF);
  let originalCM = defaults.getBoolPref(CM_PREF);
  let originalSTP = defaults.getBoolPref(STP_PREF);
  let originalEmailTP = defaults.getBoolPref(EMAIL_TP_PREF);
  let originalEmailTPPBM = defaults.getBoolPref(EMAIL_TP_PBM_PREF);
  let originalNCB = defaults.getIntPref(NCB_PREF);
  let originalNCBP = defaults.getIntPref(NCBP_PREF);
  let originalLEVEL2 = defaults.getBoolPref(LEVEL2_PREF);
  let originalREFERRER = defaults.getBoolPref(REFERRER_PREF);
  let originalREFERRERTOP = defaults.getBoolPref(REFERRER_TOP_PREF);
  let originalOCSP = defaults.getBoolPref(OCSP_PREF);
  let originalQueryParamStrip = defaults.getBoolPref(QUERY_PARAM_STRIP_PREF);
  let originalQueryParamStripPBM = defaults.getBoolPref(
    QUERY_PARAM_STRIP_PBM_PREF
  );
  let originalFPP = defaults.getBoolPref(FPP_PREF);
  let originalFPPPBM = defaults.getBoolPref(FPP_PBM_PREF);

  let nonDefaultNCB;
  switch (originalNCB) {
    case Ci.nsICookieService.BEHAVIOR_ACCEPT:
      nonDefaultNCB = Ci.nsICookieService.BEHAVIOR_REJECT;
      break;
    default:
      nonDefaultNCB = Ci.nsICookieService.BEHAVIOR_ACCEPT;
      break;
  }
  let nonDefaultNCBP;
  switch (originalNCBP) {
    case Ci.nsICookieService.BEHAVIOR_ACCEPT:
      nonDefaultNCBP = Ci.nsICookieService.BEHAVIOR_REJECT;
      break;
    default:
      nonDefaultNCBP = Ci.nsICookieService.BEHAVIOR_ACCEPT;
      break;
  }
  defaults.setIntPref(NCB_PREF, nonDefaultNCB);
  defaults.setIntPref(NCBP_PREF, nonDefaultNCBP);
  defaults.setBoolPref(TP_PREF, !originalTP);
  defaults.setBoolPref(TP_PBM_PREF, !originalTPPBM);
  defaults.setBoolPref(FP_PREF, !originalFP);
  defaults.setBoolPref(CM_PREF, !originalCM);
  defaults.setBoolPref(CM_PREF, !originalSTP);
  defaults.setBoolPref(EMAIL_TP_PREF, !originalEmailTP);
  defaults.setBoolPref(EMAIL_TP_PBM_PREF, !originalEmailTPPBM);
  defaults.setIntPref(NCB_PREF, !originalNCB);
  defaults.setBoolPref(LEVEL2_PREF, !originalLEVEL2);
  defaults.setBoolPref(REFERRER_PREF, !originalREFERRER);
  defaults.setBoolPref(REFERRER_TOP_PREF, !originalREFERRERTOP);
  defaults.setBoolPref(OCSP_PREF, !originalOCSP);
  defaults.setBoolPref(QUERY_PARAM_STRIP_PREF, !originalQueryParamStrip);
  defaults.setBoolPref(QUERY_PARAM_STRIP_PBM_PREF, !originalQueryParamStripPBM);
  defaults.setBoolPref(FPP_PREF, !originalFPP);
  defaults.setBoolPref(FPP_PBM_PREF, !originalFPPPBM);

  ok(
    !Services.prefs.prefHasUserValue(TP_PREF),
    `${TP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(TP_PBM_PREF),
    `${TP_PBM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(FP_PREF),
    `${FP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(CM_PREF),
    `${CM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(STP_PREF),
    `${STP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(EMAIL_TP_PREF),
    `${EMAIL_TP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(EMAIL_TP_PBM_PREF),
    `${EMAIL_TP_PBM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(NCB_PREF),
    `${NCB_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(NCBP_PREF),
    `${NCBP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(LEVEL2_PREF),
    `${LEVEL2_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(REFERRER_PREF),
    `${REFERRER_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(REFERRER_TOP_PREF),
    `${REFERRER_TOP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(OCSP_PREF),
    `${OCSP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(QUERY_PARAM_STRIP_PREF),
    `${QUERY_PARAM_STRIP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(QUERY_PARAM_STRIP_PBM_PREF),
    `${QUERY_PARAM_STRIP_PBM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(FPP_PREF),
    `${FPP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(FPP_PBM_PREF),
    `${FPP_PBM_PREF} pref has the default value`
  );

  // cleanup
  defaults.setIntPref(NCB_PREF, originalNCB);
  defaults.setBoolPref(TP_PREF, originalTP);
  defaults.setBoolPref(TP_PBM_PREF, originalTPPBM);
  defaults.setBoolPref(FP_PREF, originalFP);
  defaults.setBoolPref(CM_PREF, originalCM);
  defaults.setBoolPref(STP_PREF, originalSTP);
  defaults.setBoolPref(EMAIL_TP_PREF, originalEmailTP);
  defaults.setBoolPref(EMAIL_TP_PBM_PREF, originalEmailTPPBM);
  defaults.setIntPref(NCB_PREF, originalNCB);
  defaults.setIntPref(NCBP_PREF, originalNCBP);
  defaults.setBoolPref(LEVEL2_PREF, originalLEVEL2);
  defaults.setBoolPref(REFERRER_PREF, originalREFERRER);
  defaults.setBoolPref(REFERRER_TOP_PREF, originalREFERRERTOP);
  defaults.setBoolPref(OCSP_PREF, originalOCSP);
  defaults.setBoolPref(QUERY_PARAM_STRIP_PREF, originalQueryParamStrip);
  defaults.setBoolPref(QUERY_PARAM_STRIP_PBM_PREF, originalQueryParamStripPBM);
  defaults.setBoolPref(FPP_PREF, originalFPP);
  defaults.setBoolPref(FPP_PBM_PREF, originalFPPPBM);
});

// Tests that the content blocking strict category definition changes the behavior
// of the strict category pref and all prefs it controls.
// Changing the definition does not remove the user from the category.
add_task(async function testContentBlockingStrictDefinition() {
  let defaults = Services.prefs.getDefaultBranch("");
  let originalStrictPref = defaults.getStringPref(STRICT_DEF_PREF);
  defaults.setStringPref(
    STRICT_DEF_PREF,
    "tp,tpPrivate,fp,cm,cookieBehavior0,cookieBehaviorPBM0,stp,emailTP,emailTPPrivate,lvl2,rp,rpTop,ocsp,qps,qpsPBM,fpp,fppPrivate"
  );
  Services.prefs.setStringPref(CAT_PREF, "strict");
  is(
    Services.prefs.getStringPref(CAT_PREF),
    "strict",
    `${CAT_PREF} has changed to strict`
  );

  ok(
    !Services.prefs.prefHasUserValue(STRICT_DEF_PREF),
    `We changed the default value of ${STRICT_DEF_PREF}`
  );
  is(
    Services.prefs.getStringPref(STRICT_DEF_PREF),
    "tp,tpPrivate,fp,cm,cookieBehavior0,cookieBehaviorPBM0,stp,emailTP,emailTPPrivate,lvl2,rp,rpTop,ocsp,qps,qpsPBM,fpp,fppPrivate",
    `${STRICT_DEF_PREF} changed to what we set.`
  );

  is(
    Services.prefs.getBoolPref(TP_PREF),
    true,
    `${TP_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(TP_PBM_PREF),
    true,
    `${TP_PBM_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(FP_PREF),
    true,
    `${CM_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(CM_PREF),
    true,
    `${CM_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(STP_PREF),
    true,
    `${STP_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(EMAIL_TP_PREF),
    true,
    `${EMAIL_TP_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(EMAIL_TP_PBM_PREF),
    true,
    `${EMAIL_TP_PBM_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getIntPref(NCB_PREF),
    Ci.nsICookieService.BEHAVIOR_ACCEPT,
    `${NCB_PREF} has been set to BEHAVIOR_ACCEPT`
  );
  is(
    Services.prefs.getIntPref(NCBP_PREF),
    Ci.nsICookieService.BEHAVIOR_ACCEPT,
    `${NCBP_PREF} has been set to BEHAVIOR_ACCEPT`
  );
  is(
    Services.prefs.getBoolPref(LEVEL2_PREF),
    true,
    `${LEVEL2_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(REFERRER_PREF),
    true,
    `${REFERRER_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(REFERRER_TOP_PREF),
    true,
    `${REFERRER_TOP_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(OCSP_PREF),
    true,
    `${OCSP_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PREF),
    true,
    `${QUERY_PARAM_STRIP_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PBM_PREF),
    true,
    `${QUERY_PARAM_STRIP_PBM_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(FPP_PREF),
    true,
    `${FPP_PREF} pref has been set to true`
  );
  is(
    Services.prefs.getBoolPref(FPP_PBM_PREF),
    true,
    `${FPP_PBM_PREF} pref has been set to true`
  );

  // Note, if a pref is not listed it will use the default value, however this is only meant as a
  // backup if a mistake is made. The UI will not respond correctly.
  defaults.setStringPref(STRICT_DEF_PREF, "");
  ok(
    !Services.prefs.prefHasUserValue(TP_PREF),
    `${TP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(TP_PBM_PREF),
    `${TP_PBM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(FP_PREF),
    `${FP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(CM_PREF),
    `${CM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(STP_PREF),
    `${STP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(EMAIL_TP_PREF),
    `${EMAIL_TP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(EMAIL_TP_PBM_PREF),
    `${EMAIL_TP_PBM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(NCB_PREF),
    `${NCB_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(NCBP_PREF),
    `${NCBP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(LEVEL2_PREF),
    `${LEVEL2_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(REFERRER_PREF),
    `${REFERRER_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(REFERRER_TOP_PREF),
    `${REFERRER_TOP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(OCSP_PREF),
    `${OCSP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(QUERY_PARAM_STRIP_PREF),
    `${QUERY_PARAM_STRIP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(QUERY_PARAM_STRIP_PBM_PREF),
    `${QUERY_PARAM_STRIP_PBM_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(FPP_PREF),
    `${FPP_PREF} pref has the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(FPP_PBM_PREF),
    `${FPP_PBM_PREF} pref has the default value`
  );

  defaults.setStringPref(
    STRICT_DEF_PREF,
    "-tpPrivate,-fp,-cm,-tp,cookieBehavior3,cookieBehaviorPBM2,-stp,-emailTP,-emailTPPrivate,-lvl2,-rp,-ocsp,-qps,-qpsPBM,-fpp,-fppPrivate"
  );
  is(
    Services.prefs.getBoolPref(TP_PREF),
    false,
    `${TP_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(TP_PBM_PREF),
    false,
    `${TP_PBM_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(FP_PREF),
    false,
    `${FP_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(CM_PREF),
    false,
    `${CM_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(STP_PREF),
    false,
    `${STP_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(EMAIL_TP_PREF),
    false,
    `${EMAIL_TP_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(EMAIL_TP_PBM_PREF),
    false,
    `${EMAIL_TP_PBM_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getIntPref(NCB_PREF),
    Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN,
    `${NCB_PREF} has been set to BEHAVIOR_REJECT_TRACKER`
  );
  is(
    Services.prefs.getIntPref(NCBP_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT,
    `${NCBP_PREF} has been set to BEHAVIOR_REJECT`
  );
  is(
    Services.prefs.getBoolPref(LEVEL2_PREF),
    false,
    `${LEVEL2_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(REFERRER_PREF),
    false,
    `${REFERRER_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(REFERRER_TOP_PREF),
    false,
    `${REFERRER_TOP_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(OCSP_PREF),
    false,
    `${OCSP_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PREF),
    false,
    `${QUERY_PARAM_STRIP_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PBM_PREF),
    false,
    `${QUERY_PARAM_STRIP_PBM_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(FPP_PREF),
    false,
    `${FPP_PREF} pref has been set to false`
  );
  is(
    Services.prefs.getBoolPref(FPP_PBM_PREF),
    false,
    `${FPP_PBM_PREF} pref has been set to false`
  );

  // cleanup
  defaults.setStringPref(STRICT_DEF_PREF, originalStrictPref);
  Services.prefs.setStringPref(CAT_PREF, "standard");
});
