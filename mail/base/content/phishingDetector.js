# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Thunderbird Phishing Dectector
#
#
# Contributor(s):
#  Scott MacGregor <mscott@mozilla.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ******

// Dependencies: 
// gPrefBranch, gBrandBundle, gMessengerBundle should already be defined
// gatherTextUnder from utilityOverlay.js

const kPhishingNotSuspicious = 0;
const kPhishingWithIPAddress = 1;
const kPhishingWithMismatchedHosts = 2;

//////////////////////////////////////////////////////////////////////////////
// isPhishingURL --> examines the passed in linkNode and returns true if we think
//                   the URL is an email scam.
// aLinkNode: the link node to examine
// aSilentMode: don't prompt the user to confirm
//////////////////////////////////////////////////////////////////////////////

function isPhishingURL(aLinkNode, aSilentMode)
{
  if (!gPrefBranch.getBoolPref("mail.phishing.detection.enabled"))
    return true;

  var phishingType = kPhishingNotSuspicious;
  var href = aLinkNode.href;
  var linkTextURL = {};
  var hrefURL = Components.classes["@mozilla.org/network/standard-url;1"].
                createInstance(Components.interfaces.nsIURI); 
  
  hrefURL.spec = href;
  
  // (1) if the host name is an IP address then block the url...
  // TODO: add support for IPv6
  var ipv4HostRegExp = new RegExp(/\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/);  // IPv4
  if (ipv4HostRegExp.test(hrefURL.host))
    phishingType = kPhishingWithIPAddress;
  else if (misMatchedHostWithLinkText(aLinkNode, hrefURL, linkTextURL))
    phishingType = kPhishingWithMismatchedHosts;

  var isPhishingURL = phishingType != kPhishingNotSuspicious;

  if (!aSilentMode) // allow the user to over ride the decision
    isPhishingURL = confirmSuspiciousURL(phishingType, hrefURL, linkTextURL.value);

  return isPhishingURL;
}

//////////////////////////////////////////////////////////////////////////////
// helper methods in support of isPhishingURL
//////////////////////////////////////////////////////////////////////////////

function misMatchedHostWithLinkText(aLinkNode, aHrefURL, aLinkTextURL)
{
  var linkNodeText = gatherTextUnder(aLinkNode);    
  // only worry about http and https urls
  if (linkNodeText && (aHrefURL.schemeIs('http') || aHrefURL.schemeIs('https')))
  {
    // does the link text look like a http url?
     if (linkNodeText.search(/(^http:|^https:)/) != -1)
     {
       var linkTextURL = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURI); 
       linkTextURL.spec = linkNodeText;
       aLinkTextURL.value = linkTextURL;
       return aHrefURL.host != linkTextURL.host;
     }
  }

  return false;
}

// returns true if the user confirms the URL is a scam
function confirmSuspiciousURL(phishingType, hrefURL, linkNodeURL)
{
  var brandShortName = gBrandBundle.getString("brandRealShortName");
  var titleMsg = gMessengerBundle.getString("confirmPhishingTitle");
  var dialogMsg;

  switch (phishingType)
  {
    case kPhishingWithIPAddress:
      dialogMsg = gMessengerBundle.getFormattedString("confirmPhishingUrl" + phishingType, [brandShortName, hrefURL.host], 2);
      break;
    case kPhishingWithMismatchedHosts:
      dialogMsg = gMessengerBundle.getFormattedString("confirmPhishingUrl" + phishingType, [brandShortName, hrefURL.host, linkNodeURL.host], 3);
      break;
    default:
      return false;
  }

  const nsIPS = Components.interfaces.nsIPromptService;
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPS);
  var buttons = nsIPS.STD_YES_NO_BUTTONS + nsIPS.BUTTON_POS_1_DEFAULT;
  return !promptService.confirmEx(window, titleMsg, dialogMsg, buttons, "", "", "", "", {}); /* the yes button is in position 0 */
}
