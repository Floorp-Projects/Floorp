/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Netscape Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var gEncryptionStatus = -1;
var gSignatureStatus = -1;
var gSignerCert = null;
var gEncryptionCert = null;
var gBundle;
var gBrandBundle;

const nsPKIParamBlock    = "@mozilla.org/security/pkiparamblock;1";
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;

function setupBundles()
{
  if (gBundle && gBrandBundle)
    return;
  
  if (!gBundle) {
    gBundle = document.getElementById("bundle_read_smime");
    gBrandBundle = document.getElementById("bundle_brand");
  }
}

function showImapSignatureUnknown()
{
  var ifps = Components.interfaces.nsIPromptService;

  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
  promptService = promptService.QueryInterface(ifps);
  setupBundles();

  if (promptService && gBundle && gBrandBundle) {
    if (promptService.confirm(window,
          gBrandBundle.getString("brandShortName"),
          gBundle.getString("ImapOnDemand"))) {
      ReloadWithAllParts();
    }
  }
}

function showMessageReadSecurityInfo()
{
  var gSignedUINode = document.getElementById('signedHdrIcon');
  if (gSignedUINode) {
    if (gSignedUINode.getAttribute("signed") == "unknown") {
      showImapSignatureUnknown();
      return;
    }
  }
  
  var pkiParams = Components.classes[nsPKIParamBlock].createInstance(nsIPKIParamBlock);

  // isupport array starts with index 1
  pkiParams.setISupportAtIndex(1, gSignerCert);
  pkiParams.setISupportAtIndex(2, gEncryptionCert);
  
  var params = pkiParams.QueryInterface(Components.interfaces.nsIDialogParamBlock);

  // int array starts with index 0, but that is used for window exit status
  params.SetInt(1, gSignatureStatus);
  params.SetInt(2, gEncryptionStatus);
  
  window.openDialog('chrome://messenger-smime/content/msgReadSecurityInfo.xul',
    '', 'chrome,resizable=1,modal=1,dialog=1', pkiParams );
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
        if (document.firstChild.getAttribute('windowtype') == "mail:messageWindow")
        {
          return ( gCurrentMessageUri != null);
        }
        else
        {
          if (GetNumSelectedMessages() > 0 && gDBView)
          {
            var enabled = new Object();
            enabled.value = false;
            var checkStatus = new Object();
            gDBView.getCommandStatus(nsMsgViewCommandType.cmdRequiringMsgBody, enabled, checkStatus);
            return enabled.value;
          }
        }
      
        return false;
      }

      default:
        return false;
    }
    return false;
  }
};

top.controllers.appendController(SecurityController);
