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
 *   Scott MacGreogr <mscott@netscape.com>
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

var gSignedUINode = null;
var gEncryptedUINode = null;
var gSMIMEContainer = null;
var gStatusBar = null;
var gSignedStatusPanel = null;
var gEncryptedStatusPanel = null;

var gEncryptedURIService = null;
var gMyLastEncryptedURI = null;

// manipulates some globals from msgReadSMIMEOverlay.js

const nsICMSMessageErrors = Components.interfaces.nsICMSMessageErrors;

var smimeHeaderSink = 
{ 
  maxWantedNesting: function()
  {
    return 1;
  },

  signedStatus: function(aNestingLevel, aSignatureStatus, aSignerCert)
  {
    if (aNestingLevel > 1) {
      // we are not interested
      return;
    }

    gSignatureStatus = aSignatureStatus;
    gSignerCert = aSignerCert;

    gSMIMEContainer.collapsed = false;
    gSignedUINode.collapsed = false;
    gSignedStatusPanel.collapsed = false;
  
    switch (aSignatureStatus) {
      case nsICMSMessageErrors.SUCCESS:
        gSignedUINode.setAttribute("signed", "ok");
        gStatusBar.setAttribute("signed", "ok");
        break;

      case nsICMSMessageErrors.VERIFY_NOT_YET_ATTEMPTED:
        gSignedUINode.setAttribute("signed", "unknown");
        gStatusBar.setAttribute("signed", "unknown");
        break;

      case nsICMSMessageErrors.VERIFY_CERT_WITHOUT_ADDRESS:
      case nsICMSMessageErrors.VERIFY_HEADER_MISMATCH:
        gSignedUINode.setAttribute("signed", "mismatch");
        gStatusBar.setAttribute("signed", "mismatch");
        break;

      default:
        gSignedUINode.setAttribute("signed", "notok");
        gStatusBar.setAttribute("signed", "notok");
        break;
    }
  },

  encryptionStatus: function(aNestingLevel, aEncryptionStatus, aRecipientCert)
  {
    if (aNestingLevel > 1) {
      // we are not interested
      return;
    }

    gEncryptionStatus = aEncryptionStatus;
    gEncryptionCert = aRecipientCert;

    gSMIMEContainer.collapsed = false; 
    gEncryptedUINode.collapsed = false;
    gEncryptedStatusPanel.collapsed = false; 

    if (nsICMSMessageErrors.SUCCESS == aEncryptionStatus)
    {
      gEncryptedUINode.setAttribute("encrypted", "ok");
      gStatusBar.setAttribute("encrypted", "ok");
    }
    else
    {
      gEncryptedUINode.setAttribute("encrypted", "notok");
      gStatusBar.setAttribute("encrypted", "notok");
    }
    
    if (gEncryptedURIService)
    {
      gMyLastEncryptedURI = GetLoadedMessage();
      gEncryptedURIService.rememberEncrypted(gMyLastEncryptedURI);
    }
  },

  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIMsgSMIMEHeaderSink) || iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  }
};

function forgetEncryptedURI()
{
  if (gMyLastEncryptedURI && gEncryptedURIService)
  {
    gEncryptedURIService.forgetEncrypted(gMyLastEncryptedURI);
    gMyLastEncryptedURI = null;
  }
}

function onSMIMEStartHeaders()
{
  gEncryptionStatus = -1;
  gSignatureStatus = -1;
  
  gSignerCert = null;
  gEncryptionCert = null;
  
  gSMIMEContainer.collapsed = true;

  gSignedUINode.collapsed = true;
  gSignedUINode.removeAttribute("signed");
  gSignedStatusPanel.collapsed = true;
  gStatusBar.removeAttribute("signed");

  gEncryptedUINode.collapsed = true;
  gEncryptedUINode.removeAttribute("encrypted");
  gEncryptedStatusPanel.collapsed = true; 
  gStatusBar.removeAttribute("encrypted");

  forgetEncryptedURI();
}

function onSMIMEEndHeaders()
{}

function msgHdrViewSMIMEOnLoad(event)
{
  // we want to register our security header sink as an opaque nsISupports
  // on the msgHdrSink used by mail.....
  msgWindow.msgHeaderSink.securityInfo = smimeHeaderSink;

  gSignedUINode = document.getElementById('signedHdrIcon');
  gEncryptedUINode = document.getElementById('encryptedHdrIcon');
  gSMIMEContainer = document.getElementById('smimeBox');
  gStatusBar = document.getElementById('status-bar');
  gSignedStatusPanel = document.getElementById('signed-status');
  gEncryptedStatusPanel = document.getElementById('encrypted-status');

  // add ourself to the list of message display listeners so we get notified when we are about to display a
  // message.
  var listener = {};
  listener.onStartHeaders = onSMIMEStartHeaders;
  listener.onEndHeaders = onSMIMEEndHeaders;
  gMessageListeners.push(listener);

  gEncryptedURIService = 
    Components.classes["@mozilla.org/messenger-smime/smime-encrypted-uris-service;1"]
    .getService(Components.interfaces.nsIEncryptedSMIMEURIsService);
}

function msgHdrViewSMIMEOnUnload(event)
{
  forgetEncryptedURI();
}

function msgHdrViewSMIMEOnMessagePaneHide()
{
  gSMIMEContainer.collapsed = true;
  gSignedUINode.collapsed = true;
  gSignedStatusPanel.collapsed = true;
  gEncryptedUINode.collapsed = true;
  gEncryptedStatusPanel.collapsed = true; 
}

function msgHdrViewSMIMEOnMessagePaneUnhide()
{
  if (gEncryptionStatus != -1 || gSignatureStatus != -1)
  {
    gSMIMEContainer.collapsed = false;

    if (gSignatureStatus != -1)
    {
      gSignedUINode.collapsed = false;
      gSignedStatusPanel.collapsed = false;
    }

    if (gEncryptionStatus != -1)
    {
      gEncryptedUINode.collapsed = false;
      gEncryptedStatusPanel.collapsed = false;
    }
  }
}

addEventListener('messagepane-loaded', msgHdrViewSMIMEOnLoad, true);
addEventListener('messagepane-unloaded', msgHdrViewSMIMEOnUnload, true);
addEventListener('messagepane-hide', msgHdrViewSMIMEOnMessagePaneHide, true);
addEventListener('messagepane-unhide', msgHdrViewSMIMEOnMessagePaneUnhide, true);
