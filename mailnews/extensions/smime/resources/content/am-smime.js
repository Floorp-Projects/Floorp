/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributors:
 *   ddrinan@netscape.com
 *   Scott MacGregor <mscott@netscape.com>
 */

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

function onInit() 
{
  // initialize all of our elements based on the current identity values....
  gEncryptionCertName = document.getElementById("identity.encryption_cert_name");
  gHiddenEncryptionPolicy = document.getElementById("identity.encryptionpolicy");
  gEncryptionChoices = document.getElementById("encryptionChoices");
  gSignCertName       = document.getElementById("identity.signing_cert_name");
  gSignMessages       = document.getElementById("identity.sign_mail");
  gEncryptAlways      = document.getElementById("encrypt_mail_always");
  gNeverEncrypt       = document.getElementById("encrypt_mail_never");
  gBundle             = document.getElementById("bundle_smime");
  gBrandBundle        = document.getElementById("bundle_brand");

  gEncryptionCertName.value = gIdentity.getUnicharAttribute("encryption_cert_name");

  var selectedItemId = null;
  var encryptionPolicy = gIdentity.getIntAttribute("encryptionpolicy");
  switch (encryptionPolicy)
  {
    case 2:
      selectedItemId = 'encrypt_mail_always';
      break;
    default:
      selectedItemId = 'encrypt_mail_never';
      break;
  }

  gEncryptionChoices.selectedItem = document.getElementById(selectedItemId);
    
  if (!gEncryptionCertName.value)
  {
    gEncryptAlways.setAttribute("disabled", true);
    gNeverEncrypt.setAttribute("disabled", true);
  }

  gSignCertName.value = gIdentity.getUnicharAttribute("signing_cert_name");
  gSignMessages.checked = gIdentity.getBoolAttribute("sign_mail");
  if (!gSignCertName.value)
  {
    gSignMessages.setAttribute("disabled", true);
  }
  onLockPreference();
}

function onPreInit(account, accountValues)
{
  gIdentity = account.defaultIdentity;
}

function onSave()
{
  // find out which radio for the encryption radio group is selected and set that on our hidden encryptionChoice pref....
  var newValue = gEncryptionChoices.selectedItem.value;
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
    { prefstring:"encryptionCertSelectButton", id:"encryptionCertSelectButton"}
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
  for (i=0; i<prefstrArray.length; i++) {
    var id = prefstrArray[i].id;
    var element = document.getElementById(id);
    if (gSmimePrefbranch.prefIsLocked(prefstrArray[i].prefstring)) {
        element.setAttribute("disabled", "true");
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
    return null;
  
  if (email_recipient_cert_usage == usage) {
    matchingOtherCert = certdb.getEmailEncryptionCert(nickname);
  }
  else if (email_signing_cert_usage == usage) {
    matchingOtherCert = certdb.getEmailSigningCert(nickname);
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
    enabler();
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

  var encryptionCertPrefName = "identity.encryption_cert_name";
  var signingCertPrefName = "identity.signing_cert_name";

  if (smime_cert == encryptionCertPrefName) {
    selectEncryptionCert = true;
    certUsage = email_recipient_cert_usage;
  } else if (smime_cert == signingCertPrefName) {
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
        enableEncryptionControls();

        checkOtherCert(certInfo.value,
          signingCertPrefName, email_signing_cert_usage, 
          "signing_needCertWantSame", 
          "signing_wantSame", 
          "signing_needCertWantToSelect",
          enableSigningControls);
      } else {
        enableSigningControls();

        checkOtherCert(certInfo.value,
          encryptionCertPrefName, email_recipient_cert_usage, 
          "encryption_needCertWantSame", 
          "encryption_wantSame", 
          "encryption_needCertWantToSelect",
          enableEncryptionControls);
      }
    }
  }
}

function enableEncryptionControls()
{
  gEncryptAlways.removeAttribute("disabled");
  gNeverEncrypt.removeAttribute("disabled");
}

function enableSigningControls()
{
  gSignMessages.removeAttribute("disabled");
}
