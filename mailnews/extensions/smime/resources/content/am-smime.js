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
var gEncryptIfPossible  = null;
var gEncryptAlways  = null;
var gSignCertName  = null;
var gSignMessages  = null;

function onInit() 
{
  // initialize all of our elements based on the current identity values....
  gEncryptionCertName = document.getElementById("identity.encryption_cert_name");
  gEncryptIfPossible  =  document.getElementById("identity.encrypt_mail_if_possible");
  gEncryptAlways      = document.getElementById("identity.encrypt_mail_always");
  gSignCertName       = document.getElementById("identity.signing_cert_name");
  gSignMessages       = document.getElementById("identity.sign_mail");

  gEncryptionCertName.value = gIdentity.getUnicharAttribute("encryption_cert_name");
  gEncryptIfPossible.checked = gIdentity.getBoolAttribute("encrypt_mail_if_possible");
  gEncryptAlways.checked = gIdentity.getBoolAttribute("encrypt_mail_always");
  if (!gEncryptionCertName.value)
  {
    gEncryptAlways.setAttribute("disabled", true);
  }

  // we currently don't support encrypt if possible so keep it disabled for now...
  gEncryptIfPossible.setAttribute("disabled", true);

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
  var picker = Components.classes["@mozilla.org/user_cert_picker;1"]
               .getService(Components.interfaces.nsIUserCertPicker);
  var canceled = new Object;
  var x509cert = 0;
  var certUsage;

  if (smime_cert == "identity.encryption_cert_name") {
    certUsage = 5;
  } else if (smime_cert == "identity.signing_cert_name") {
    certUsage = 4;
  }

  try {
    var smimeBundle = document.getElementById("bundle_smime");
    x509cert = picker.pickByUsage(window,
      smimeBundle.getString("prefPanel-smime"),
      smimeBundle.getString("smimeCertPrompt"),
      certUsage, // this is from enum SECCertUsage
      false, false, canceled);
  } catch(e) {
    // XXX display error message in the future
  }

  if (!canceled.value && x509cert) {
    var certInfo = document.getElementById(smime_cert);
    if (certInfo) {
      certInfo.setAttribute("disabled", "false");
      certInfo.value = x509cert.nickname;

      if (smime_cert == "identity.encryption_cert_name") {
        gEncryptAlways.removeAttribute("disabled");
      } else {
        gSignMessages.removeAttribute("disabled");
      }
	}
  }
}
