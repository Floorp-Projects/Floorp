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
 *   Scott MacGreogr <mscott@netscape.com>
 */

var gSignedUIVisible = false;
var gEncryptionUIVisible = false;

var gSignedUINode = null;
var gEncryptedUINode = null;
var gSMIMEContainer = null;

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

    gSignedUINode.collapsed = false; 
    gSMIMEContainer.collapsed = false; 

    if (nsICMSMessageErrors.SUCCESS == aSignatureStatus)
    {
      gSignedUINode.value = "<signed>";
    }
    else
    {
      // show a broken signature icon....
      gSignedUINode.value = "<invalid signature>";
    }

    gSignedUIVisible = true;
  },

  encryptionStatus: function(aNestingLevel, aEncryptionStatus)
  {
    if (aNestingLevel > 1) {
      // we are not interested
      return;
    }

    gEncryptionStatus = aEncryptionStatus;

    gEncryptedUINode.collapsed = false; 
    gSMIMEContainer.collapsed = false; 

    if (nsICMSMessageErrors.SUCCESS == aEncryptionStatus)
    {
      gEncryptedUINode.value = "<encrypted>";
    }
    else
    {
      // show a broken encryption icon....
      gEncryptedUINode.value = "<invalid encryption>";
    }

    gEncryptionUIVisible = true;
  },

  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIMsgSMIMEHeaderSink) || iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  }
};

function onSMIMEStartHeaders()
{
  gEncryptionStatus = -1;
  gSignatureStatus = -1;
  
  gSignerCert = null;
  
  gSMIMEContainer.collapsed = true;

  if (gEncryptionUIVisible)
  {
    gEncryptedUINode.collapsed = true;
    gEncryptionUIVisible = false;
  }

  if (gSignedUIVisible)
  {
    gSignedUINode.collapsed = true; 
    gSignedUIVisible = false;
  }
}

function onSMIMEEndHeaders()
{}

function msgHdrViewSMIMEOnLoad(event)
{
  // we want to register our security header sink as an opaque nsISupports
  // on the msgHdrSink used by mail.....
  msgWindow.msgHeaderSink.securityInfo = smimeHeaderSink;

  gSignedUINode = document.getElementById('signedText');
  gEncryptedUINode = document.getElementById('encryptedText');
  gSMIMEContainer = document.getElementById('smimeBox');

  // add ourself to the list of message display listeners so we get notified when we are about to display a
  // message.
  var listener = {};
  listener.onStartHeaders = onSMIMEStartHeaders;
  listener.onEndHeaders = onSMIMEEndHeaders;
  gMessageListeners.push(listener);
}

addEventListener('messagepane-loaded', msgHdrViewSMIMEOnLoad, true);
