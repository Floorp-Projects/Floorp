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

var gISMimeCompFields = Components.interfaces.nsIMsgSMIMECompFields;
var gSMimeCompFieldsContractID = "@mozilla.org/messenger-smime/composefields;1";
var gNextSecurityButtonCommand = "";
var gBundle;
var gBrandBundle;
var gSMFields;


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
}

function setSignatureUI()
{
  top.document.getElementById("securityStatus").setAttribute("signing", "ok");
}

function setNoEncryptionUI()
{
  top.document.getElementById("securityStatus").removeAttribute("crypto");
}

function setEncryptionUI()
{
  top.document.getElementById("securityStatus").setAttribute("crypto", "ok");
}

function showMessageComposeSecurityStatus()
{
  Recipients2CompFields(gMsgCompose.compFields);

  var areCertsAvailable = false;
  var encryptionCertName = gCurrentIdentity.getUnicharAttribute("encryption_cert_name");
  var signingCertName = gCurrentIdentity.getUnicharAttribute("signing_cert_name");
  
  if (encryptionCertName.length > 0 && signingCertName.length > 0 )
  {
    areCertsAvailable = true;
  }

  window.openDialog('chrome://messenger-smime/content/msgCompSecurityInfo.xul',
    '',
    'chrome,resizable=1,modal=1,dialog=1', 
    {
      compFields : gMsgCompose.compFields,
      subject : GetMsgSubjectElement().value,
      smFields : gSMFields,
      certsAvailable : areCertsAvailable
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

top.controllers.appendController(SecurityController);
addEventListener('compose-window-close', onComposerClose, true);
addEventListener('compose-window-reopen', onComposerReOpen, true);
