/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

function Startup() {
  DoEnableService();
  DoEnableSettings();
}

function DoEnableService() {
  var enable_setting = document.getElementById( "P3PServiceEnabled" );
  var warn0_setting  = document.getElementById( "P3PWarningNotPrivate" );
  var warn1_setting  = document.getElementById( "P3PWarningPartialPrivacy" );
  var warn2_setting  = document.getElementById( "P3PWarningPostToNotPrivate" );
  var warn3_setting  = document.getElementById( "P3PWarningPostToBrokenPolicy" );
  var warn4_setting  = document.getElementById( "P3PWarningPostToNoPolicy" );

  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefBranch);

  if (enable_setting.checked) {
    warn0_setting.removeAttribute( "disabled" );
    warn1_setting.removeAttribute( "disabled" );
    warn2_setting.removeAttribute( "disabled" );
    warn3_setting.removeAttribute( "disabled" );
    warn4_setting.removeAttribute( "disabled" );

    // Look for the warning settings.  If not defined then an exception will occur.
    // If exception occurs then set the checkbox.
    try {
      pref.getBoolPref( "P3P.warningnotprivate" );
    }
    catch (ex) {
      warn0_setting.setAttribute("checked", "true");
    }

    try {
      pref.getBoolPref( "P3P.warningpartialprivacy" );
    }
    catch (ex) {
      warn1_setting.setAttribute("checked", "true");
    }

    try {
      pref.getBoolPref( "P3P.warningposttonotprivate" );
    }
    catch (ex) {
      warn2_setting.setAttribute("checked", "true");
    }

    try {
      pref.getBoolPref( "P3P.warningposttobrokenpolicy" );
    }
    catch (ex) {
      warn3_setting.setAttribute("checked", "true");
    }

    try {
      pref.getBoolPref( "P3P.warningposttonopolicy" );
    }
    catch (ex) {
      warn4_setting.setAttribute("checked", "true");
    }

  }
  else {
    warn0_setting.setAttribute( "disabled", "true" );
    warn1_setting.setAttribute( "disabled", "true" );
    warn2_setting.setAttribute( "disabled", "true" );
    warn3_setting.setAttribute( "disabled", "true" );
    warn4_setting.setAttribute( "disabled", "true" );
  }

  // Allow other settings to be updated
  DoEnableSettings();
}

function DoEnableSettings() {
  var enable_service = document.getElementById( "P3PServiceEnabled" );
  var level_setting  = document.getElementById( "P3PLevel" );

  if (enable_service.checked) {
    // Service is enabled...
    EnableTree(level_setting);

    try {
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);

      var level = pref.getIntPref( "P3P.level" );
    }
    catch (ex) {
      level_setting.setAttribute( "data", "0" );
    }

    DoMapLevel();
  }
  else {
    // Service is not enabled... disable all controls
    DisableTree(level_setting);
  }
}

function EnableTree(item) {
  item.removeAttribute( "disabled" );

  for( var i = 0; i < item.childNodes.length; i++ ) {
    EnableTree(item.childNodes[i]);
  }
}

function DisableTree(item) {
  item.setAttribute( "disabled", "true" );

  for( var i = 0; i < item.childNodes.length; i++ ) {
    DisableTree(item.childNodes[i]);
  }
}

var settingIDs    = ["P3PPhysicalAdmin",                  "P3PPhysicalHistorical",
                     "P3PPhysicalDevelop",                "P3PPhysicalPseudo-analysis",
                     "P3PPhysicalIndividual-analysis",    "P3PPhysicalCustomization",
                     "P3PPhysicalTailoring",              "P3PPhysicalPseudo-decision",
                     "P3PPhysicalIndividual-decision",    "P3PPhysicalCurrent",
                     "P3PPhysicalContact",                "P3PPhysicalTelemarketing",
                     "P3PPhysicalOther-purpose",          "P3PPhysicalSharing",

                     "P3PLocationAdmin",                  "P3PLocationHistorical",
                     "P3PLocationDevelop",                "P3PLocationPseudo-analysis",
                     "P3PLocationIndividual-analysis",    "P3PLocationCustomization",
                     "P3PLocationTailoring",              "P3PLocationPseudo-decision",
                     "P3PLocationIndividual-decision",    "P3PLocationCurrent",
                     "P3PLocationContact",                "P3PLocationTelemarketing",
                     "P3PLocationOther-purpose",          "P3PLocationSharing",

                     "P3POnlineAdmin",                    "P3POnlineHistorical",
                     "P3POnlineDevelop",                  "P3POnlinePseudo-analysis",
                     "P3POnlineIndividual-analysis",      "P3POnlineCustomization",
                     "P3POnlineTailoring",                "P3POnlinePseudo-decision",
                     "P3POnlineIndividual-decision",      "P3POnlineCurrent",
                     "P3POnlineContact",                  "P3POnlineTelemarketing",
                     "P3POnlineOther-purpose",            "P3POnlineSharing",

                     "P3PComputerAdmin",                  "P3PComputerHistorical",
                     "P3PComputerDevelop",                "P3PComputerPseudo-analysis",
                     "P3PComputerIndividual-analysis",    "P3PComputerCustomization",
                     "P3PComputerTailoring",              "P3PComputerPseudo-decision",
                     "P3PComputerIndividual-decision",    "P3PComputerCurrent",
                     "P3PComputerContact",                "P3PComputerTelemarketing",
                     "P3PComputerOther-purpose",          "P3PComputerSharing",

                     "P3PStateAdmin",                     "P3PStateHistorical",
                     "P3PStateDevelop",                   "P3PStatePseudo-analysis",
                     "P3PStateIndividual-analysis",       "P3PStateCustomization",
                     "P3PStateTailoring",                 "P3PStatePseudo-decision",
                     "P3PStateIndividual-decision",       "P3PStateCurrent",
                     "P3PStateContact",                   "P3PStateTelemarketing",
                     "P3PStateOther-purpose",             "P3PStateSharing",

                     "P3PUniqueIDAdmin",                  "P3PUniqueIDHistorical",
                     "P3PUniqueIDDevelop",                "P3PUniqueIDPseudo-analysis",
                     "P3PUniqueIDIndividual-analysis",    "P3PUniqueIDCustomization",
                     "P3PUniqueIDTailoring",              "P3PUniqueIDPseudo-decision",
                     "P3PUniqueIDIndividual-decision",    "P3PUniqueIDCurrent",
                     "P3PUniqueIDContact",                "P3PUniqueIDTelemarketing",
                     "P3PUniqueIDOther-purpose",          "P3PUniqueIDSharing",

                     "P3PGovernmentAdmin",                "P3PGovernmentHistorical",
                     "P3PGovernmentDevelop",              "P3PGovernmentPseudo-analysis",
                     "P3PGovernmentIndividual-analysis",  "P3PGovernmentCustomization",
                     "P3PGovernmentTailoring",            "P3PGovernmentPseudo-decision",
                     "P3PGovernmentIndividual-decision",  "P3PGovernmentCurrent",
                     "P3PGovernmentContact",              "P3PGovernmentTelemarketing",
                     "P3PGovernmentOther-purpose",        "P3PGovernmentSharing",

                     "P3PPurchaseAdmin",                  "P3PPurchaseHistorical",
                     "P3PPurchaseDevelop",                "P3PPurchasePseudo-analysis",
                     "P3PPurchaseIndividual-analysis",    "P3PPurchaseCustomization",
                     "P3PPurchaseTailoring",              "P3PPurchasePseudo-decision",
                     "P3PPurchaseIndividual-decision",    "P3PPurchaseCurrent",
                     "P3PPurchaseContact",                "P3PPurchaseTelemarketing",
                     "P3PPurchaseOther-purpose",          "P3PPurchaseSharing",

                     "P3PFinancialAdmin",                 "P3PFinancialHistorical",
                     "P3PFinancialDevelop",               "P3PFinancialPseudo-analysis",
                     "P3PFinancialIndividual-analysis",   "P3PFinancialCustomization",
                     "P3PFinancialTailoring",             "P3PFinancialPseudo-decision",
                     "P3PFinancialIndividual-decision",   "P3PFinancialCurrent",
                     "P3PFinancialContact",               "P3PFinancialTelemarketing",
                     "P3PFinancialOther-purpose",         "P3PFinancialSharing",

                     "P3PHealthAdmin",                    "P3PHealthHistorical",
                     "P3PHealthDevelop",                  "P3PHealthPseudo-analysis",
                     "P3PHealthIndividual-analysis",      "P3PHealthCustomization",
                     "P3PHealthTailoring",                "P3PHealthPseudo-decision",
                     "P3PHealthIndividual-decision",      "P3PHealthCurrent",
                     "P3PHealthContact",                  "P3PHealthTelemarketing",
                     "P3PHealthOther-purpose",            "P3PHealthSharing",

                     "P3PPoliticalAdmin",                 "P3PPoliticalHistorical",
                     "P3PPoliticalDevelop",               "P3PPoliticalPseudo-analysis",
                     "P3PPoliticalIndividual-analysis",   "P3PPoliticalCustomization",
                     "P3PPoliticalTailoring",             "P3PPoliticalPseudo-decision",
                     "P3PPoliticalIndividual-decision",   "P3PPoliticalCurrent",
                     "P3PPoliticalContact",               "P3PPoliticalTelemarketing",
                     "P3PPoliticalOther-purpose",         "P3PPoliticalSharing",

                     "P3PNavigationAdmin",                "P3PNavigationHistorical",
                     "P3PNavigationDevelop",              "P3PNavigationPseudo-analysis",
                     "P3PNavigationIndividual-analysis",  "P3PNavigationCustomization",
                     "P3PNavigationTailoring",            "P3PNavigationPseudo-decision",
                     "P3PNavigationIndividual-decision",  "P3PNavigationCurrent",
                     "P3PNavigationContact",              "P3PNavigationTelemarketing",
                     "P3PNavigationOther-purpose",        "P3PNavigationSharing",

                     "P3PInteractiveAdmin",               "P3PInteractiveHistorical",
                     "P3PInteractiveDevelop",             "P3PInteractivePseudo-analysis",
                     "P3PInteractiveIndividual-analysis", "P3PInteractiveCustomization",
                     "P3PInteractiveTailoring",           "P3PInteractivePseudo-decision",
                     "P3PInteractiveIndividual-decision", "P3PInteractiveCurrent",
                     "P3PInteractiveContact",             "P3PInteractiveTelemarketing",
                     "P3PInteractiveOther-purpose",       "P3PInteractiveSharing",

                     "P3PDemographicAdmin",               "P3PDemographicHistorical",
                     "P3PDemographicDevelop",             "P3PDemographicPseudo-analysis",
                     "P3PDemographicIndividual-analysis", "P3PDemographicCustomization",
                     "P3PDemographicTailoring",           "P3PDemographicPseudo-decision",
                     "P3PDemographicIndividual-decision", "P3PDemographicCurrent",
                     "P3PDemographicContact",             "P3PDemographicTelemarketing",
                     "P3PDemographicOther-purpose",       "P3PDemographicSharing",

                     "P3PPreferenceAdmin",                "P3PPreferenceHistorical",
                     "P3PPreferenceDevelop",              "P3PPreferencePseudo-analysis",
                     "P3PPreferenceIndividual-analysis",  "P3PPreferenceCustomization",
                     "P3PPreferenceTailoring",            "P3PPreferencePseudo-decision",
                     "P3PPreferenceIndividual-decision",  "P3PPreferenceCurrent",
                     "P3PPreferenceContact",              "P3PPreferenceTelemarketing",
                     "P3PPreferenceOther-purpose",        "P3PPreferenceSharing",

                     "P3PContentAdmin",                   "P3PContentHistorical",
                     "P3PContentDevelop",                 "P3PContentPseudo-analysis",
                     "P3PContentIndividual-analysis",     "P3PContentCustomization",
                     "P3PContentTailoring",               "P3PContentPseudo-decision",
                     "P3PContentIndividual-decision",     "P3PContentCurrent",
                     "P3PContentContact",                 "P3PContentTelemarketing",
                     "P3PContentOther-purpose",           "P3PContentSharing",

                     "P3POtherAdmin",                     "P3POtherHistorical",
                     "P3POtherDevelop",                   "P3POtherPseudo-analysis",
                     "P3POtherIndividual-analysis",       "P3POtherCustomization",
                     "P3POtherTailoring",                 "P3POtherPseudo-decision",
                     "P3POtherIndividual-decision",       "P3POtherCurrent",
                     "P3POtherContact",                   "P3POtherTelemarketing",
                     "P3POtherOther-purpose",             "P3POtherSharing"];

// It is imperitive that the order in the following matrix arrays be maintained with the order in the settingIDs array above.

//                    admin historical develop pseudo-  individual-  customization tailoring pseudo-  individual- current contact telemarketing other-  Sharing
//                                             analysis analysis                             decision decision                                  purpose

var low_matrix    = [  true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Physical
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Location
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Online
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Computer
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // State
                      false,     false, false,    false,      false,         false,    false,   false,      false,   true,  false,        false,  false,  false, // UniqueID
                      false,     false, false,    false,      false,         false,    false,   false,      false,   true,  false,        false,  false,  false, // Government
                      false,     false, false,    false,      false,         false,    false,   false,      false,   true,  false,        false,  false,  false, // Purchase
                      false,     false, false,    false,      false,         false,    false,   false,      false,   true,  false,        false,  false,  false, // Financial
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Health
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Political
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Navigation
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Interactive
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Demographic
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Preference
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,   true, // Content
                      false,     false, false,    false,      false,         false,    false,   false,      false,   true,  false,        false,  false,  false  // Other
                    ];

//                    admin historical develop pseudo-  individual-  customization tailoring pseudo-  individual- current contact telemarketing other-  Sharing
//                                             analysis analysis                             decision decision                                  purpose

var medium_matrix = [  true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Physical
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Location
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Online
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Computer
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // State
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // UniqueID
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Government
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Purchase
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Financial
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Health
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Political
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Navigation
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Interactive
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Demographic
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Preference
                       true,      true,  true,     true,       true,          true,     true,    true,       true,   true,   true,         true,  false,  false, // Content
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false  // Other
                    ];

//                    admin historical develop pseudo-  individual-  customization tailoring pseudo-  individual- current contact telemarketing other-  Sharing
//                                             analysis analysis                             decision decision                                  purpose

var high_matrix   = [ false,     false, false,    false,      false,         false,    false,   false,      false,   true,  false,        false,  false,  false, // Physical
                      false,     false, false,    false,      false,         false,    false,   false,      false,   true,  false,        false,  false,  false, // Location
                      false,     false, false,    false,      false,         false,    false,   false,      false,   true,  false,        false,  false,  false, // Online
                       true,      true,  true,    false,      false,          true,     true,    true,       true,   true,  false,        false,  false,  false, // Computer
                       true,      true,  true,    false,      false,          true,     true,    true,       true,   true,  false,        false,  false,  false, // State
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // UniqueID
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Government
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Purchase
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Financial
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Health
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Political
                       true,      true,  true,    false,      false,          true,     true,    true,       true,   true,  false,        false,  false,  false, // Navigation
                       true,      true,  true,    false,      false,          true,     true,    true,       true,   true,  false,        false,  false,  false, // Interactive
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Demographic
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Preference
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false, // Content
                      false,     false, false,    false,      false,         false,    false,   false,      false,  false,  false,        false,  false,  false  // Other
                    ];

function DoMapLevel() {
  var level = document.getElementById( "P3PLevel" );

  if (level.data != "3") {

    var custradio = document.getElementById( "custradio" );
    custradio.setAttribute("disabled", "true");

    for (var i = 0; i < settingIDs.length; i++) {
      var element    = document.getElementById( settingIDs[i] );

      if (level.data == "0") {
        element.checked = low_matrix[i];
      }
      else if (level.data == "1") {
        element.checked = medium_matrix[i];
      }
      else if (level.data == "2") {
        element.checked = high_matrix[i];
      }
    }
  }
}

function doCustomize() {
  // Save the current settings so that we can check if the customize window changed
  // any of them.
  var savedSettings = [false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false,
                       false, false, false, false, false, false, false, false, false, false, false, false, false, false];

  for (var i = 0; i < settingIDs.length; i++) {
    savedSettings[i] = document.getElementById( settingIDs[i] ).checked;
  }

  // Open the customize window.

  window.openDialog("chrome://communicator/content/pref/pref-P3Pcust.xul","P3Pcust","modal=yes,chrome,resizable=yes",
                    "custombox");

  // Check if any settings were changed.
  var settingChanged = false;
  for (var i = 0; (i < settingIDs.length) && !settingChanged; i++) {
    settingChanged = savedSettings[i] != document.getElementById( settingIDs[i] ).checked;
  }

  // If any of the settings changed, enable and set the "Custom" radio button
  // (uncheck the current radio button first).
  if (settingChanged) {

    // Rather than try to figure out which radio button is currently checked in
    // order to uncheck it, we just get all the radio buttons in the group and uncheck them all.
    var level = document.getElementById( "P3PLevel" );
    var groupElements = level.getElementsByTagName("radio");
    for( var i = 0; i < groupElements.length; i++ ) {
      if( groupElements[i] != level && groupElements[i].checked ) {
        groupElements[i].checked = false;
        groupElements[i].removeAttribute( "checked" );
      }
    }

    // Enable the "Custom" radio button and check it.
    var custradio = document.getElementById( "custradio" );
    custradio.removeAttribute("disabled");
    custradio.setAttribute("checked", "true");

    // Set the radio group's data index to the one for the custom button.
    level.setAttribute( "data", "3" );
  }
}

function initCustomize() {
  var tree = document.getElementById( 'categoryTree' );
  var defaultItem = document.getElementById( 'defaultItem' );
  tree.selectItem( defaultItem );
}
