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

var gIdentity;
var gPref = null;
var gEncryptionCertName = null;
var gHiddenEncryptionPolicy = null;
var gEncryptionChoices = null;
var gSignCertName  = null;
var gSignMessages  = null;
var gEncryptAlways = null;
var gNeverEncrypt = null;

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
  dump("XXX on lock\n");
}


// Does the work of disabling an element given the array which contains xul id/prefstring pairs.
// Also saves the id/locked state in an array so that other areas of the code can avoid
// stomping on the disabled state indiscriminately.
function disableIfLocked( prefstrArray )
{
    if (!gLockedPref)
      gLockedPref = new Array;

    for (i=0; i<prefstrArray.length; i++) {
        var id = prefstrArray[i].id;
        var element = document.getElementById(id);
        if (gPref.prefIsLocked(prefstrArray[i].prefstring)) {
            element.disabled = true;
            gLockedPref[id] = true;
        } else {
            element.removeAttribute("disabled");
            gLockedPref[id] = false;
        }
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

  if (smime_cert == "identity.encryption_cert_name") {
    certUsage = 5;
  } else if (smime_cert == "identity.signing_cert_name") {
    certUsage = 4;
  }

  try {
    x509cert = picker.pickByUsage(window,
      certInfo.value,
      certUsage, // this is from enum SECCertUsage
      false, false, canceled);
  } catch(e) {
    // XXX display error message in the future
  }

  if (!canceled.value && x509cert) {
    certInfo.setAttribute("disabled", "false");
    certInfo.value = x509cert.nickname;

    if (smime_cert == "identity.encryption_cert_name") {
      gEncryptAlways.removeAttribute("disabled");
      gNeverEncrypt.removeAttribute("disabled");
    } else {
      gSignMessages.removeAttribute("disabled");
    }
  }
}
