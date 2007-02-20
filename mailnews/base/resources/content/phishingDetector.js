/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Thunderbird Phishing Dectector
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Scott MacGregor <mscott@mozilla.org>
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
 * ***** END LICENSE BLOCK ****** */

// Dependencies:
// gPrefBranch, gBrandBundle, gMessengerBundle should already be defined
// gatherTextUnder from utilityOverlay.js

const kPhishingNotSuspicious = 0;
const kPhishingWithIPAddress = 1;
const kPhishingWithMismatchedHosts = 2;

//////////////////////////////////////////////////////////////////////////////
// isEmailScam --> examines the message currently loaded in the message pane
//                 and returns true if we think that message is an e-mail scam.
//                 Assumes the message has been completely loaded in the message pane (i.e. OnMsgParsed has fired)
// aUrl: nsIURI object for the msg we want to examine...
//////////////////////////////////////////////////////////////////////////////
function isMsgEmailScam(aUrl)
{
  var isEmailScam = false; 
  if (!aUrl || !gPrefBranch.getBoolPref("mail.phishing.detection.enabled"))
    return isEmailScam;

  // Ignore nntp and RSS messages
  // nsIMsgMailNewsUrl.folder can throw an error, especially if we are opening
  // a .eml message.
  try {
    var serverType = aUrl.folder.server.type;
    if (serverType == 'nntp' || serverType == 'rss')
      return isEmailScam;
  } catch (ex) {}

  // loop through all of the link nodes in the message's DOM, looking for phishing URLs...
  var msgDocument = document.getElementById('messagepane').contentDocument;
  var index;

  // examine all links...
  var linkNodes = msgDocument.links;
  for (index = 0; index < linkNodes.length && !isEmailScam; index++)
    isEmailScam = isPhishingURL(linkNodes[index], true);

  // if an e-mail contains a non-addressbook form element, then assume the message is
  // a phishing attack. Legitimate sites should not be using forms inside of e-mail
  if (!isEmailScam)
  {
    var forms = msgDocument.getElementsByTagName("form");
    for (index = 0; index < forms.length && !isEmailScam; index++)
      isEmailScam = forms[index].action != "" && !/^addbook:/.test(forms[index].action);
  }

  // we'll add more checks here as our detector matures....
  return isEmailScam;
}

//////////////////////////////////////////////////////////////////////////////
// isPhishingURL --> examines the passed in linkNode and returns true if we think
//                   the URL is an email scam.
// aLinkNode: the link node to examine
// aSilentMode: don't prompt the user to confirm
// aHref: optional href for XLinks
//////////////////////////////////////////////////////////////////////////////

function isPhishingURL(aLinkNode, aSilentMode, aHref)
{
  if (!gPrefBranch.getBoolPref("mail.phishing.detection.enabled"))
    return false;

  var phishingType = kPhishingNotSuspicious;
  var href = aHref || aLinkNode.href;
  if (!href)
    return false;

  var linkTextURL = {};
  var unobscuredHostName = {};
  var isPhishingURL = false;

  var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
  var hrefURL = ioService.newURI(href, null, null);
  
  // only check for phishing urls if the url is an http or https link.
  // this prevents us from flagging imap and other internally handled urls
  if (hrefURL.schemeIs('http') || hrefURL.schemeIs('https'))
  {
    unobscuredHostName.value = hrefURL.host;

    if (hostNameIsIPAddress(hrefURL.host, unobscuredHostName) && !isLocalIPAddress(unobscuredHostName))
      phishingType = kPhishingWithIPAddress;
    else if (misMatchedHostWithLinkText(aLinkNode, hrefURL, linkTextURL))
      phishingType = kPhishingWithMismatchedHosts;

    isPhishingURL = phishingType != kPhishingNotSuspicious;

    if (!aSilentMode && isPhishingURL) // allow the user to override the decision
      isPhishingURL = confirmSuspiciousURL(phishingType, unobscuredHostName.value);
  }

  return isPhishingURL;
}

//////////////////////////////////////////////////////////////////////////////
// helper methods in support of isPhishingURL
//////////////////////////////////////////////////////////////////////////////

function misMatchedHostWithLinkText(aLinkNode, aHrefURL, aLinkTextURL)
{
  var linkNodeText = gatherTextUnder(aLinkNode);

  // gatherTextUnder puts a space between each piece of text it gathers,
  // so strip the spaces out (see bug 326082 for details).
  linkNodeText = linkNodeText.replace(/ /g, "");

  // only worry about http and https urls
  if (linkNodeText)
  {
    // does the link text look like a http url?
     if (linkNodeText.search(/(^http:|^https:)/) != -1)
     {
       var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
       var linkTextURL  = ioService.newURI(linkNodeText, null, null);
       aLinkTextURL.value = linkTextURL;
       // compare hosts, but ignore possible www. prefix
       return !(aHrefURL.host.replace(/^www\./, "") == aLinkTextURL.value.host.replace(/^www\./, ""));
     }
  }

  return false;
}

// returns true if the hostName is an IP address
// if the host name is an obscured IP address, returns the unobscured host
function hostNameIsIPAddress(aHostName, aUnobscuredHostName)
{
  // TODO: Add Support for IPv6

  var index;

  // scammers frequently obscure the IP address by encoding each component as octal, hex
  // or in some cases a mix match of each. The IP address could also be represented as a DWORD.

  // break the IP address down into individual components.
  var ipComponents = aHostName.split(".");

  // if we didn't find at least 4 parts to our IP address it either isn't a numerical IP
  // or it is encoded as a dword
  if (ipComponents.length < 4)
  {
    // Convert to a binary to test for possible DWORD.
    var binaryDword = parseInt(aHostName).toString(2);
    if (isNaN(binaryDword))
      return false;

    // convert the dword into its component IP parts.
    ipComponents =
    [
      (aHostName >> 24) & 255,
      (aHostName >> 16) & 255,
      (aHostName >>  8) & 255,
      (aHostName & 255)
    ];
  }
  else
  {
    for (index = 0; index < ipComponents.length; ++index)
    {
      // by leaving the radix parameter blank, we can handle IP addresses
      // where one component is hex, another is octal, etc.
      ipComponents[index] = parseInt(ipComponents[index]);
    }
  }

  // make sure each part of the IP address is in fact a number
  for (index = 0; index < ipComponents.length; ++index)
    if (isNaN(ipComponents[index])) // if any part of the IP address is not a number, then we can safely return
      return false;

  // only set aUnobscuredHostName if we are looking at an IPv4 host name
  var hostName = ipComponents.join(".");
  if (isIPv4HostName(hostName))
  {
    aUnobscuredHostName.value = hostName;
    return true;
  }

  return false;
}

function isIPv4HostName(aHostName)
{
  var ipv4HostRegExp = new RegExp(/\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/);  // IPv4
  // treat 0.0.0.0 as an invalid IP address
  return ipv4HostRegExp.test(aHostName) && aHostName != '0.0.0.0';
}

// returns true if the user confirms the URL is a scam
function confirmSuspiciousURL(aPhishingType, aSuspiciousHostName)
{
  var brandShortName = gBrandBundle.getString("brandShortName");
  var titleMsg = gMessengerBundle.getString("confirmPhishingTitle");
  var dialogMsg;

  switch (aPhishingType)
  {
    case kPhishingWithIPAddress:
    case kPhishingWithMismatchedHosts:
      dialogMsg = gMessengerBundle.getFormattedString("confirmPhishingUrl" + aPhishingType, [brandShortName, aSuspiciousHostName], 2);
      break;
    default:
      return false;
  }

  const nsIPS = Components.interfaces.nsIPromptService;
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPS);
  var buttons = nsIPS.STD_YES_NO_BUTTONS + nsIPS.BUTTON_POS_1_DEFAULT;
  return promptService.confirmEx(window, titleMsg, dialogMsg, buttons, "", "", "", "", {}); /* the yes button is in position 0 */
}

// returns true if the IP address is a local address.
function isLocalIPAddress(unobscuredHostName)
{
  var ipComponents = unobscuredHostName.value.split(".");

  return ipComponents[0] == 10 ||
         (ipComponents[0] == 192 && ipComponents[1] == 168) ||
         (ipComponents[0] == 169 && ipComponents[1] == 254) ||
         (ipComponents[0] == 172 && ipComponents[1] >= 16 && ipComponents[1] < 32);
}
