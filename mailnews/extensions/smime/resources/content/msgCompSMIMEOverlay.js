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

var ismimeCompFields = Components.interfaces.nsIMsgSMIMECompFields;
var smimeCompFieldsContractID = "@mozilla.org/messenger-smime/composefields;1";

// InitializeSecurityInfo --> when we want to override the default smime behavior for the current message,
// we need to make sure we have a security info object on the msg compose fields....
function GetSecurityInfo()
{
  var smimeComposefields;
  var msgCompFields = msgCompose.compFields;
  if (msgCompFields)
  {
    if (!msgCompFields.securityInfo)
    {
      smimeComposefields = Components.classes[smimeCompFieldsContractID].createInstance(ismimeCompFields);
      if (smimeComposefields)
      {
        msgCompFields.securityInfo = smimeComposefields;
        // set up the intial security state....
        smimeComposefields.alwaysEncryptMessage = gCurrentIdentity.getBoolAttribute("encrypt_mail_always");
        smimeComposefields.signMessage = gCurrentIdentity.getBoolAttribute("sign_mail");
      }
    } 
    else
    {
      smimeComposefields = msgCompFields.securityInfo;
    }
  } // if we have message compose fields...

  return smimeComposefields
}

function noEncryption()
{
  var smimeCompFields = GetSecurityInfo();
  if (smimeCompFields)
    smimeCompFields.alwaysEncryptMessage = false;
}

function encryptMessage()
{
  var checkedNode = document.getElementById("menu_securityEncryptAlways");
  var noEncryptionNode = document.getElementById("menu_securityNoEncryption");

  var smimeCompFields = GetSecurityInfo();
  
  var encryptionCertName = gCurrentIdentity.getUnicharAttribute("encryption_cert_name");
  if (!encryptionCertName) 
  {
    alert(gComposeMsgsBundle.getString("chooseEncryptionCertMsg"));
    checkedNode.removeAttribute("checked");
    smimeCompFields.signMessage = false;
    noEncryptionNode.setAttribute("checked");
    return;
  }
    
  smimeCompFields.alwaysEncryptMessage = true;
  checkedNode.setAttribute("checked", true);
}

function signMessage()
{ 
  var checkedNode = document.getElementById("menu_securitySign");
  var checked = checkedNode.getAttribute("checked");

  var smimeCompFields = GetSecurityInfo(); 

  if (checked) // if checked, make sure we have a cert name...
  {
    var signingCertName = gCurrentIdentity.getUnicharAttribute("signing_cert_name");
    if (!signingCertName) 
    {
      alert(gComposeMsgsBundle.getString("chooseSigningCertMsg"));
      checkedNode.removeAttribute("checked");
      smimeCompFields.signMessage = false;
      return;
    }

    smimeCompFields.signMessage = true;
    checkedNode.setAttribute("checked", true);  
  }
  else
  {
    smimeCompFields.signMessage = false;
    checkedNode.removeAttribute("checked");  
  }
}

function setSecuritySettings()
{ 
  var smimeCompFields = GetSecurityInfo();
  document.getElementById("menu_securityEncryptAlways").setAttribute("checked", smimeCompFields.alwaysEncryptMessage);
  document.getElementById("menu_securityNoEncryption").setAttribute("checked", !smimeCompFields.alwaysEncryptMessage);
  document.getElementById("menu_securitySign").setAttribute("checked", smimeCompFields.signMessage);
}
