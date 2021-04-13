/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env webextensions */

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PBM_PREF = "privacy.trackingprotection.pbmode.enabled";
const NCB_PREF = "network.cookie.cookieBehavior";
const NCBP_PREF = "network.cookie.cookieBehavior.pbmode";
const CAT_PREF = "browser.contentblocking.category";
const FP_PREF = "privacy.trackingprotection.fingerprinting.enabled";
const CM_PREF = "privacy.trackingprotection.cryptomining.enabled";
const STP_PREF = "privacy.trackingprotection.socialtracking.enabled";
const LEVEL2_PREF = "privacy.annotate_channels.strict_list.enabled";
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

  let defaults = Services.prefs.getDefaultBranch("");
  let originalTP = defaults.getBoolPref(TP_PREF);
  let originalTPPBM = defaults.getBoolPref(TP_PBM_PREF);
  let originalFP = defaults.getBoolPref(FP_PREF);
  let originalCM = defaults.getBoolPref(CM_PREF);
  let originalSTP = defaults.getBoolPref(STP_PREF);
  let originalNCB = defaults.getIntPref(NCB_PREF);
  let originalNCBP = defaults.getIntPref(NCBP_PREF);
  let originalLEVEL2 = defaults.getBoolPref(LEVEL2_PREF);

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
  defaults.setIntPref(NCB_PREF, !originalNCB);
  defaults.setBoolPref(LEVEL2_PREF, !originalLEVEL2);

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

  // cleanup
  defaults.setIntPref(NCB_PREF, originalNCB);
  defaults.setBoolPref(TP_PREF, originalTP);
  defaults.setBoolPref(TP_PBM_PREF, originalTPPBM);
  defaults.setBoolPref(FP_PREF, originalFP);
  defaults.setBoolPref(CM_PREF, originalCM);
  defaults.setBoolPref(STP_PREF, originalSTP);
  defaults.setIntPref(NCB_PREF, originalNCB);
  defaults.setIntPref(NCBP_PREF, originalNCBP);
  defaults.setBoolPref(LEVEL2_PREF, originalLEVEL2);
});

// Tests that the content blocking strict category definition changes the behavior
// of the strict category pref and all prefs it controls.
// Changing the definition does not remove the user from the category.
add_task(async function testContentBlockingStrictDefinition() {
  let defaults = Services.prefs.getDefaultBranch("");
  let originalStrictPref = defaults.getStringPref(STRICT_DEF_PREF);
  defaults.setStringPref(
    STRICT_DEF_PREF,
    "tp,tpPrivate,fp,cm,cookieBehavior0,cookieBehaviorPBM0,stp,lvl2"
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
    "tp,tpPrivate,fp,cm,cookieBehavior0,cookieBehaviorPBM0,stp,lvl2",
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

  defaults.setStringPref(
    STRICT_DEF_PREF,
    "-tpPrivate,-fp,-cm,-tp,cookieBehavior3,cookieBehaviorPBM2,-stp,-lvl2"
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

  // cleanup
  defaults.setStringPref(STRICT_DEF_PREF, originalStrictPref);
  Services.prefs.setStringPref(CAT_PREF, "standard");
});
