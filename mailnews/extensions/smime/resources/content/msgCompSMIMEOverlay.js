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
 *   Scott MacGreogr <mscott@netscape.com>
 */

const gISMimeCompFields = Components.interfaces.nsIMsgSMIMECompFields;
const gSMimeCompFieldsContractID = "@mozilla.org/messenger-smime/composefields;1";
const gSMimeContractID = "@mozilla.org/messenger-smime/smimejshelper;1";
const gISMimeJSHelper = Components.interfaces.nsISMimeJSHelper;
var gNextSecurityButtonCommand = "";
var gBundle;
var gBrandBundle;
var gSMFields;
var gEncryptedURIService = null;


function onComposerClose()
{
  gSMFields = null;
  setNoEncryptionUI();
  setNoSignatureUI();

  if (!gMsgCompose)
    return;

  if (!gMsgCompose.compFields)
    return;

  gMsgCompose.compFields.securityInfo = null;
}

function onComposerReOpen()
{
  // are we already set up?
  if (gSMFields)
    return;

  if (!gMsgCompose)
    return;

  if (!gMsgCompose.compFields)
    return;

  gMsgCompose.compFields.securityInfo = null;

  gSMFields = Components.classes[gSMimeCompFieldsContractID].createInstance(gISMimeCompFields);
  if (gSMFields)
  {
    gMsgCompose.compFields.securityInfo = gSMFields;
    // set up the intial security state....
    var encryptionPolicy = gCurrentIdentity.getIntAttribute("encryptionpolicy");
    // 0 == never, 1 == if possible, 2 == always Encrypt.
    gSMFields.requireEncryptMessage = encryptionPolicy == 2;

    gSMFields.signMessage = gCurrentIdentity.getBoolAttribute("sign_mail");

    if (gEncryptedURIService && !gSMFields.requireEncryptMessage)
    {
      if (gEncryptedURIService.isEncrypted(gMsgCompose.originalMsgURI))
      {
        // Override encryption setting if original is known as encrypted.
        gSMFields.requireEncryptMessage = true;
      }
    }

    if (gSMFields.requireEncryptMessage)
    {
      setEncryptionUI();
    }
    else
    {
      setNoEncryptionUI();
    }

    if (gSMFields.signMessage)
    {
      setSignatureUI();
    }
    else
    {
      setNoSignatureUI();
    }
  }
}


// this function gets called multiple times,
// but only on first open, not on composer recycling
function smimeComposeOnLoad()
{
  if (!gEncryptedURIService)
  {
    gEncryptedURIService = 
      Components.classes["@mozilla.org/messenger-smime/smime-encrypted-uris-service;1"]
      .getService(Components.interfaces.nsIEncryptedSMIMEURIsService);
  }

  onComposerReOpen();
}

function setupBundles()
{
  if (gBundle && gBrandBundle)
    return;
  
  if (!gBundle) {
    gBundle = document.getElementById("bundle_comp_smime");
    gBrandBundle = document.getElementById("bundle_brand");
  }
}

function showNeedSetupInfo()
{
  var ifps = Components.interfaces.nsIPromptService;

  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
  promptService = promptService.QueryInterface(ifps);
  setupBundles();

  if (promptService && gBundle && gBrandBundle) {
    var dummy = new Object;
    var buttonPressed =
    promptService.confirmEx(window,
      gBrandBundle.getString("brandShortName"),
      gBundle.getString("NeedSetup"), 
      (ifps.BUTTON_POS_0 * ifps.BUTTON_TITLE_YES
       + ifps.BUTTON_POS_1 * ifps.BUTTON_TITLE_NO),
      0,
      0,
      0,
      null,
      dummy);
    
    if (0 == buttonPressed) {
      openHelp("sign-encrypt");
    }
  }
}

function noEncryption()
{
  if (!gSMFields)
    return;

  gSMFields.requireEncryptMessage = false;
  setNoEncryptionUI();
}

function encryptMessage()
{
  if (!gSMFields)
    return;
  
  var encryptionCertName = gCurrentIdentity.getUnicharAttribute("encryption_cert_name");
  if (!encryptionCertName) 
  {
    gSMFields.requireEncryptMessage = false;
    setNoEncryptionUI();
    showNeedSetupInfo();
    return;
  }

  gSMFields.requireEncryptMessage = true;
  setEncryptionUI();
}

function signMessage()
{ 
  if (!gSMFields)
    return;

  // toggle
  gSMFields.signMessage = !gSMFields.signMessage;

  if (gSMFields.signMessage) // make sure we have a cert name...
  {
    var signingCertName = gCurrentIdentity.getUnicharAttribute("signing_cert_name");
    if (!signingCertName)
    {
      gSMFields.signMessage = false;
      showNeedSetupInfo();
      return;
    }

    setSignatureUI();
  }
  else
  {
    setNoSignatureUI();
  }
}

function setSecuritySettings(menu_id)
{ 
  if (!gSMFields)
    return;

  document.getElementById("menu_securityEncryptRequire" + menu_id).setAttribute("checked", gSMFields.requireEncryptMessage);
  document.getElementById("menu_securityNoEncryption" + menu_id).setAttribute("checked", !gSMFields.requireEncryptMessage);
  document.getElementById("menu_securitySign" + menu_id).setAttribute("checked", gSMFields.signMessage);
}

function setNextCommand(what)
{
  gNextSecurityButtonCommand = what;
}

function doSecurityButton()
{
  var what = gNextSecurityButtonCommand;
  gNextSecurityButtonCommand = "";

  switch (what)
  {
    case "noEncryption":
      noEncryption();
      break;
    
    case "encryptMessage":
      encryptMessage();
      break;
    
    case "signMessage":
      signMessage();
      break;
    
    case "show":
    default:
      showMessageComposeSecurityStatus();
      break;
  }
}

function setNoSignatureUI()
{
  top.document.getElementById("securityStatus").removeAttribute("signing");
  top.document.getElementById("signing-status").collapsed = true;
}

function setSignatureUI()
{
  top.document.getElementById("securityStatus").setAttribute("signing", "ok");
  top.document.getElementById("signing-status").collapsed = false;
}

function setNoEncryptionUI()
{
  top.document.getElementById("securityStatus").removeAttribute("crypto");
  top.document.getElementById("encryption-status").collapsed = true;
}

function setEncryptionUI()
{
  top.document.getElementById("securityStatus").setAttribute("crypto", "ok");
  top.document.getElementById("encryption-status").collapsed = false;
}

function showMessageComposeSecurityStatus()
{
  Recipients2CompFields(gMsgCompose.compFields);

  var encryptionCertName = gCurrentIdentity.getUnicharAttribute("encryption_cert_name");
  var signingCertName = gCurrentIdentity.getUnicharAttribute("signing_cert_name");
  
  window.openDialog('chrome://messenger-smime/content/msgCompSecurityInfo.xul',
    '',
    'chrome,resizable=1,modal=1,dialog=1', 
    {
      compFields : gMsgCompose.compFields,
      subject : GetMsgSubjectElement().value,
      smFields : gSMFields,
      isSigningCertAvailable : (signingCertName.length > 0),
      isEncryptionCertAvailable : (encryptionCertName.length > 0),
      currentIdentity : gCurrentIdentity
    }
  );
}

var SecurityController =
{
  supportsCommand: function(command)
  {
    switch ( command )
    {
      case "cmd_viewSecurityStatus":
        return true;
      
      default:
        return false;
     }
  },

  isCommandEnabled: function(command)
  {
    switch ( command )
    {
      case "cmd_viewSecurityStatus":
      {
        return true;
      }

      default:
        return false;
    }
    return false;
  }
};

function onComposerSendMessage()
{
  try {
    if (!gMsgCompose.compFields.securityInfo.requireEncryptMessage) {
      return;
    }

    var helper = Components.classes[gSMimeContractID].createInstance(gISMimeJSHelper);

    var emailAddresses = new Object();
    var missingCount = new Object();

    helper.getNoCertAddresses(
      gMsgCompose.compFields,
      missingCount,
      emailAddresses);
  }
  catch (e)
  {
    return;
  }

  if (missingCount.value > 0)
  {
    var prefService =
      Components.classes["@mozilla.org/preferences-service;1"]
        .getService(Components.interfaces.nsIPrefService);
    var prefs = prefService.getBranch(null);

    var autocompleteLdap = false;
    autocompleteLdap = prefs.getBoolPref("ldap_2.autoComplete.useDirectory");

    if (autocompleteLdap)
    {
      var autocompleteDirectory = null;
      autocompleteDirectory = prefs.getCharPref(
        "ldap_2.autoComplete.directoryServer");

      if(gCurrentIdentity.overrideGlobalPref) {
        autocompleteDirectory = gCurrentIdentity.directoryServer;
      }

      if (autocompleteDirectory)
      {
        window.openDialog('chrome://messenger-smime/content/certFetchingStatus.xul',
          '',
          'chrome,resizable=1,modal=1,dialog=1', 
          autocompleteDirectory,
          emailAddresses.value
        );
      }
    }
  }
}

function onComposerFromChanged()
{
  if (!gSMFields)
    return;

  // In order to provide maximum protection to the user:
  // - If encryption is already enabled, we will not turn it off automatically.
  // - If encryption is not enabled, but the new account defaults to encryption, we will turn it on.
  // - If signing is disabled, we will not turn it on automatically.
  // - If signing is enabled, but the new account defaults to not sign, we will turn signing off.

  if (!gSMFields.requireEncryptMessage)
  {
    var encryptionPolicy = gCurrentIdentity.getIntAttribute("encryptionpolicy");
    // 0 == never, 1 == if possible, 2 == always Encrypt.

    if (encryptionPolicy == 2)
    {
      gSMFields.requireEncryptMessage = true;
      setEncryptionUI();
    }
  }

  if (gSMFields.signMessage)
  {
    var signMessage = gCurrentIdentity.getBoolAttribute("sign_mail");
    
    if (!signMessage)
    {
      gSMFields.signMessage = false;
      setNoSignatureUI();
    }
  }
}

top.controllers.appendController(SecurityController);
addEventListener('compose-window-close', onComposerClose, true);
addEventListener('compose-window-reopen', onComposerReOpen, true);
addEventListener('compose-send-message', onComposerSendMessage, true);
addEventListener('compose-from-changed', onComposerFromChanged, true);
