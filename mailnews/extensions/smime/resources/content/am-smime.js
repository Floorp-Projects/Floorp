/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   ddrinan@netscape.com
 *   Scott MacGregor <mscott@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsX509CertDBContractID = "@mozilla.org/security/x509certdb;1";
const nsIX509Cert = Components.interfaces.nsIX509Cert;

const email_recipient_cert_usage = 5;
const email_signing_cert_usage = 4;

var gIdentity;
var gPref = null;
var gEncryptionCertName = null;
var gHiddenEncryptionPolicy = null;
var gEncryptionChoices = null;
var gSignCertName  = null;
var gSignMessages  = null;
var gEncryptAlways = null;
var gNeverEncrypt = null;
var gBundle = null;
var gBrandBundle;
var gSmimePrefbranch;
var gEncryptionChoicesLocked;
var gSigningChoicesLocked;
const kEncryptionCertPref = "identity.encryption_cert_name";
const kSigningCertPref = "identity.signing_cert_name";

function onInit() 
{
  // initialize all of our elements based on the current identity values....
  gEncryptionCertName = document.getElementById(kEncryptionCertPref);
  gHiddenEncryptionPolicy = document.getElementById("identity.encryptionpolicy");
  gEncryptionChoices = document.getElementById("encryptionChoices");
  gSignCertName       = document.getElementById(kSigningCertPref);
  gSignMessages       = document.getElementById("identity.sign_mail");
  gEncryptAlways      = document.getElementById("encrypt_mail_always");
  gNeverEncrypt       = document.getElementById("encrypt_mail_never");
  gBundle             = document.getElementById("bundle_smime");
  gBrandBundle        = document.getElementById("bundle_brand");

  gEncryptionChoicesLocked = false;
  gSigningChoicesLocked = false;

  gEncryptionCertName.value = gIdentity.getUnicharAttribute("encryption_cert_name");

  gEncryptionChoices.value = gIdentity.getIntAttribute("encryptionpolicy");
    
  if (!gEncryptionCertName.value)
  {
    gEncryptAlways.setAttribute("disabled", true);
    gNeverEncrypt.setAttribute("disabled", true);
  }
  else {
    enableEncryptionControls(true);
  }

  gSignCertName.value = gIdentity.getUnicharAttribute("signing_cert_name");
  gSignMessages.checked = gIdentity.getBoolAttribute("sign_mail");
  if (!gSignCertName.value)
  {
    gSignMessages.setAttribute("disabled", true);
  }
  else {
    enableSigningControls(true);
  }

  // Always start with enabling signing and encryption cert select buttons.
  // This will keep the visibility of buttons in a sane state as user
  // jumps from security panel of one account to another.
  enableCertSelectButtons();

  // Disable all locked elements on the panel
  onLockPreference();
}

function onPreInit(account, accountValues)
{
  gIdentity = account.defaultIdentity;
}

function onSave()
{
  // find out which radio for the encryption radio group is selected and set that on our hidden encryptionChoice pref....
  var newValue = gEncryptionChoices.value;
  gHiddenEncryptionPolicy.setAttribute('value', newValue);
  gIdentity.setIntAttribute("encryptionpolicy", newValue);
  gIdentity.setUnicharAttribute("encryption_cert_name", gEncryptionCertName.value);

  gIdentity.setBoolAttribute("sign_mail", gSignMessages.checked);
  gIdentity.setUnicharAttribute("signing_cert_name", gSignCertName.value);
}

function onLockPreference()
{
  var initPrefString = "mail.identity"; 
  var finalPrefString; 

  var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);

  var allPrefElements = [
    { prefstring:"signingCertSelectButton", id:"signingCertSelectButton"},
    { prefstring:"encryptionCertSelectButton", id:"encryptionCertSelectButton"},
    { prefstring:"sign_mail", id:"identity.sign_mail"},
    { prefstring:"encryptionpolicy", id:"encryptionChoices"}
  ];

  finalPrefString = initPrefString + "." + gIdentity.key + ".";
  gSmimePrefbranch = prefService.getBranch(finalPrefString);

  disableIfLocked( allPrefElements );
}


// Does the work of disabling an element given the array which contains xul id/prefstring pairs.
// Also saves the id/locked state in an array so that other areas of the code can avoid
// stomping on the disabled state indiscriminately.
function disableIfLocked( prefstrArray )
{
  var i;
  for (i=0; i<prefstrArray.length; i++) {
    var id = prefstrArray[i].id;
    var element = document.getElementById(id);
    if (gSmimePrefbranch.prefIsLocked(prefstrArray[i].prefstring)) {
      // If encryption choices radio group is locked, make sure the individual 
      // choices in the group are locked. Set a global (gEncryptionChoicesLocked) 
      // indicating the status so that locking can be maintained further.
      if (id == "encryptionChoices") {
        document.getElementById("encrypt_mail_never").setAttribute("disabled", "true");
        document.getElementById("encrypt_mail_always").setAttribute("disabled", "true");
        gEncryptionChoicesLocked = true;
      }
      // If option to sign mail is locked (with true/false set in config file), disable
      // the corresponding checkbox and set a global (gSigningChoicesLocked) in order to
      // honor the locking as user changes other elements on the panel. 
      if (id == "identity.sign_mail") {
        document.getElementById("identity.sign_mail").setAttribute("disabled", "true");
        gSigningChoicesLocked = true;
      }
      else {
        element.setAttribute("disabled", "true");
        if (id == "signingCertSelectButton") {
          document.getElementById("signingCertClearButton").setAttribute("disabled", "true");
        }
        else if (id == "encryptionCertSelectButton") {
          document.getElementById("encryptionCertClearButton").setAttribute("disabled", "true");
        }
      }
    }
  }
}

function getPromptService()
{
  var ifps = Components.interfaces.nsIPromptService;
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
  if (promptService) {
    promptService = promptService.QueryInterface(ifps);
  }
  return promptService;
}

function alertUser(message)
{
  var ps = getPromptService();
  if (ps) {
    ps.alert(
      window,
      gBrandBundle.getString("brandShortName"), 
      message);
  }
}

function askUser(message)
{
  var ps = getPromptService();
  if (!ps)
    return false;
  
  return ps.confirm(
    window,
    gBrandBundle.getString("brandShortName"), 
    message);
}

function checkOtherCert(nickname, pref, usage, msgNeedCertWantSame, msgWantSame, msgNeedCertWantToSelect, enabler)
{
  var otherCertInfo = document.getElementById(pref);
  if (!otherCertInfo)
    return;

  if (otherCertInfo.value == nickname)
    // all is fine, same cert is now selected for both purposes
    return;

  var certdb = Components.classes[nsX509CertDBContractID].getService(nsIX509CertDB);
  if (!certdb)
    return;
  
  if (email_recipient_cert_usage == usage) {
    matchingOtherCert = certdb.findEmailEncryptionCert(nickname);
  }
  else if (email_signing_cert_usage == usage) {
    matchingOtherCert = certdb.findEmailSigningCert(nickname);
  }
  else
    return;

  var userWantsSameCert = false;

  if (!otherCertInfo.value.length) {
    if (matchingOtherCert) {
      userWantsSameCert = askUser(gBundle.getString(msgNeedCertWantSame));
    }
    else {
      if (askUser(gBundle.getString(msgNeedCertWantToSelect))) {
        smimeSelectCert(pref);
      }
    }
  }
  else {
    if (matchingOtherCert) {
      userWantsSameCert = askUser(gBundle.getString(msgWantSame));
    }
  }

  if (userWantsSameCert) {
    otherCertInfo.value = nickname;
    enabler(true);
  }
}

function smimeSelectCert(smime_cert)
{
  var certInfo = document.getElementById(smime_cert);
  if (!certInfo)
    return;

  var picker = Components.classes["@mozilla.org/user_cert_picker;1"]
               .createInstance(Components.interfaces.nsIUserCertPicker);
  var canceled = new Object;
  var x509cert = 0;
  var certUsage;
  var selectEncryptionCert;

  if (smime_cert == kEncryptionCertPref) {
    selectEncryptionCert = true;
    certUsage = email_recipient_cert_usage;
  } else if (smime_cert == kSigningCertPref) {
    selectEncryptionCert = false;
    certUsage = email_signing_cert_usage;
  }

  try {
    x509cert = picker.pickByUsage(window,
      certInfo.value,
      certUsage, // this is from enum SECCertUsage
      false, false, canceled);
  } catch(e) {
    canceled.value = false;
    x509cert = null;
  }

  if (!canceled.value) {
    if (!x509cert) {
      var errorString;
      if (selectEncryptionCert) {
        errorString = "NoEncryptionCert";
      }
      else {
        errorString = "NoSigningCert";
      }
      alertUser(gBundle.getString(errorString));
    }
    else {
      certInfo.removeAttribute("disabled");
      certInfo.value = x509cert.nickname;

      if (selectEncryptionCert) {
        enableEncryptionControls(true);

        checkOtherCert(certInfo.value,
          kSigningCertPref, email_signing_cert_usage, 
          "signing_needCertWantSame", 
          "signing_wantSame", 
          "signing_needCertWantToSelect",
          enableSigningControls);
      } else {
        enableSigningControls(true);

        checkOtherCert(certInfo.value,
          kEncryptionCertPref, email_recipient_cert_usage, 
          "encryption_needCertWantSame", 
          "encryption_wantSame", 
          "encryption_needCertWantToSelect",
          enableEncryptionControls);
      }
    }
  }

  enableCertSelectButtons();
}

function enableEncryptionControls(do_enable)
{
  if (gEncryptionChoicesLocked)
    return;

  if (do_enable) {
    gEncryptAlways.removeAttribute("disabled");
    gNeverEncrypt.removeAttribute("disabled");
  }
  else {
    gEncryptAlways.setAttribute("disabled", "true");
    gNeverEncrypt.setAttribute("disabled", "true");
    gEncryptionChoices.value = 0;
  }
}

function enableSigningControls(do_enable)
{
  if (gSigningChoicesLocked)
    return;

  if (do_enable) {
    gSignMessages.removeAttribute("disabled");
  }
  else {
    gSignMessages.setAttribute("disabled", "true");
    gSignMessages.checked = false;
  }
}

function enableCertSelectButtons()
{
  document.getElementById("signingCertSelectButton").removeAttribute("disabled");

  if (document.getElementById('identity.signing_cert_name').value.length)
    document.getElementById("signingCertClearButton").removeAttribute("disabled");
  else
    document.getElementById("signingCertClearButton").setAttribute("disabled", "true");

  document.getElementById("encryptionCertSelectButton").removeAttribute("disabled");

  if (document.getElementById('identity.encryption_cert_name').value.length)
    document.getElementById("encryptionCertClearButton").removeAttribute("disabled");
  else
    document.getElementById("encryptionCertClearButton").setAttribute("disabled", "true");
}

function smimeClearCert(smime_cert)
{
  var certInfo = document.getElementById(smime_cert);
  if (!certInfo)
    return;

  certInfo.setAttribute("disabled", "true");
  certInfo.value = "";

  if (smime_cert == kEncryptionCertPref) {
    enableEncryptionControls(false);
  } else if (smime_cert == kSigningCertPref) {
    enableSigningControls(false);
  }
  
  enableCertSelectButtons();
}

