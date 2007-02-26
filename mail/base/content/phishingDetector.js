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
# The Initial Developer of the Original Code is
# The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
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

var gPhishingDetector = {
  mCheckForIPAddresses: true,
  mCheckForMismatchedHosts: true,
  mPhishingWarden: null,
  /**
   * initialize the phishing warden. 
   * initialize the black and white list url tables. 
   * update the local tables if necessary
   */
  init: function() 
  {
    try {
      // set up the anti phishing service
      var appContext = Components.classes["@mozilla.org/phishingprotection/application;1"]
                         .getService().wrappedJSObject;

      this.mPhishingWarden  = new appContext.PROT_PhishingWarden();

      // Register tables
      // XXX: move table names to a pref that we originally will download
      // from the provider (need to workout protocol details)
      this.mPhishingWarden.registerWhiteTable("goog-white-domain");
      this.mPhishingWarden.registerWhiteTable("goog-white-url");
      this.mPhishingWarden.registerBlackTable("goog-black-url");
      this.mPhishingWarden.registerBlackTable("goog-black-enchash");

      // Download/update lists if we're in non-enhanced mode
      this.mPhishingWarden.maybeToggleUpdateChecking();  
    } catch (ex) { dump('unable to create the phishing warden: ' + ex + '\n');}
    
    this.mCheckForIPAddresses = gPrefBranch.getBoolPref("mail.phishing.detection.ipaddresses");
    this.mCheckForMismatchedHosts = gPrefBranch.getBoolPref("mail.phishing.detection.mismatched_hosts");
  },
  
  /**
   * Analyzes the urls contained in the currently loaded message in the message pane, looking for
   * phishing URLs.
   * Assumes the message has finished loading in the message pane (i.e. OnMsgParsed has fired).
   * 
   * @param aUrl nsIURI for the message being analyzed.
   *
   * @return asynchronously calls gMessageNotificationBar.setPhishingMsg if the message
   *         is identified as a scam.         
   */
  analyzeMsgForPhishingURLs: function (aUrl)
  {
    if (!aUrl || !gPrefBranch.getBoolPref("mail.phishing.detection.enabled"))
      return;  
      
    // Ignore nntp and RSS messages
    var folder;
    try {
      folder = aUrl.folder;
      if (folder.server.type == 'nntp' || folder.server.type == 'rss')
        return;
    } catch (ex) {}

    // extract the link nodes in the message and analyze them, looking for suspicious URLs...
    var linkNodes = document.getElementById('messagepane').contentDocument.links;
    for (var index = 0; index < linkNodes.length; index++)
      this.analyzeUrl(linkNodes[index].href, gatherTextUnder(linkNodes[index]));
      
    // extract the action urls associated with any form elements in the message and analyze them.
    var formNodes = document.getElementById('messagepane').contentDocument.getElementsByTagName("form");
    for (index = 0; index < formNodes.length; index++)
    {
      if (formNodes[index].action)
        this.analyzeUrl(formNodes[index].action);
    }
  },
  
  /** 
   * Analyze the url contained in aLinkNode for phishing attacks. If a phishing URL is found,
   * 
   * @param aHref the url to be analyzed
   * @param aLinkText (optional) user visible link text associated with aHref in case
   *        we are dealing with a link node.
   * @return asynchronously calls gMessageNotificationBar.setPhishingMsg if the link node
   *         conains a phishing URL.  
   */
   analyzeUrl: function (aUrl, aLinkText)
   {
     if (!aUrl)
       return;


     var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
     var hrefURL;
     // make sure relative link urls don't make us bail out
     try {
       hrefURL = ioService.newURI(aUrl, null, null);
     } catch(ex) { return; }
  
     // only check for phishing urls if the url is an http or https link.
     // this prevents us from flagging imap and other internally handled urls
     if (hrefURL.schemeIs('http') || hrefURL.schemeIs('https'))
     {
       var failsStaticTests = false;      
       var linkTextURL = {};
       var unobscuredHostName = {};
       unobscuredHostName.value = hrefURL.host;
       
       if (this.mCheckForIPAddresses && this.hostNameIsIPAddress(hrefURL.host, unobscuredHostName) && !this.isLocalIPAddress(unobscuredHostName))
         failsStaticTests = true;
       else if (this.mCheckForMismatchedHosts && aLinkText && this.misMatchedHostWithLinkText(hrefURL, aLinkText, linkTextURL))
         failsStaticTests = true;
       
       // Lookup the url against our local list. We want to do this even if the url fails our static
       // test checks because the url might be in the white list.
       if (this.mPhishingWarden)
        this.mPhishingWarden.isEvilURL(GetLoadedMessage(), failsStaticTests, aUrl, this.localListCallback);
       else
         this.localListCallback(GetLoadedMessage(), failsStaticTests, aUrl, 2 /* not found */);
    }
  },
  
  /**
    * 
    * @param aMsgURI the uri for the loaded message when the look up was initiated.
    * @param aFailsStaticTests true if our static tests think the url is a phishing scam
    * @param aUrl the url we looked up in the phishing tables
    * @param aLocalListStatus the result of the local lookup (PROT_ListWarden.IN_BLACKLIST,
    *        PROT_ListWarden.IN_WHITELIST or PROT_ListWarden.NOT_FOUND.
    */
  localListCallback: function (aMsgURI, aFailsStaticTests, aUrl, aLocalListStatus)
  {  
    // for urls in the blacklist, notify the phishing bar.
    // for urls in the whitelist, do nothing
    // for all other urls, fall back to the static tests
    if (aMsgURI == GetLoadedMessage())
    {
      if (aLocalListStatus == 0 /* PROT_ListWarden.IN_BLACKLIST */ ||
          (aLocalListStatus == 2 /* PROT_ListWarden.PROT_ListWarden.NOT_FOUND */ && aFailsStaticTests))
        gMessageNotificationBar.setPhishingMsg();
    }
  },
  
  /**
   * Looks up the report phishing url for the current phishing provider, appends aPhishingURL to the url,
   * and loads it in the default browser where the user can submit the url as a phish.
   * @param aPhishingURL the url we want to report back as a phishing attack
   */
   reportPhishingURL: function(aPhishingURL)
   {
     var appContext = Components.classes["@mozilla.org/phishingprotection/application;1"]
                       .getService().wrappedJSObject;
     var reportUrl = appContext.getReportPhishingURL();
     if (reportUrl)
     {
       reportUrl += "&url=" + encodeURIComponent(aPhishingURL);
       // now send the url to the default browser
       
       var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                       .getService(Components.interfaces.nsIIOService);                       
       var uri = ioService.newURI(reportUrl, null, null);
       var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                         .getService(Components.interfaces.nsIExternalProtocolService);
       protocolSvc.loadUrl(uri);
     }
   },   
  
  /**
   * Private helper method to determine if the link node contains a user visible
   * url with a host name that differs from the actual href the user would get taken to.
   * i.e. <a href="http://myevilsite.com">http://mozilla.org</a>
   * 
   * @return true if aHrefURL.host matches the host of the link node text. 
   * @return aLinkTextURL the nsIURI for the link node text
   */
  misMatchedHostWithLinkText: function(aHrefURL, aLinkNodeText, aLinkTextURL)
  {
    // gatherTextUnder puts a space between each piece of text it gathers,
    // so strip the spaces out (see bug 326082 for details).
    aLinkNodeText = aLinkNodeText.replace(/ /g, "");

    // only worry about http and https urls
    if (aLinkNodeText)
    {
      // does the link text look like a http url?
       if (aLinkNodeText.search(/(^http:|^https:)/) != -1)
       {
         var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
         aLinkTextURL.value = ioService.newURI(aLinkNodeText, null, null);
         // compare hosts, but ignore possible www. prefix
         return !(aHrefURL.host.replace(/^www\./, "") == aLinkTextURL.value.host.replace(/^www\./, ""));
       }
    }

    return false;
  },
  
  /**
   * Private helper method to determine if aHostName is an obscured IP address 
   * @return unobscured host name (if there is one)
   * @return true if aHostName is an IP address
   */
  hostNameIsIPAddress: function(aHostName, aUnobscuredHostName)
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
      ipComponents = new Array;
      ipComponents[0] = (aHostName >> 24) & 255;
      ipComponents[1] = (aHostName >> 16) & 255;
      ipComponents[2] = (aHostName >>  8) & 255;
      ipComponents[3] = (aHostName & 255);
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

    var hostName = ipComponents[0] + '.' +  ipComponents[1] + '.' + ipComponents[2] + '.' + ipComponents[3];

    // only set aUnobscuredHostName if we are looking at an IPv4 host name
    if (this.isIPv4HostName(hostName))
    {
      aUnobscuredHostName.value = hostName;
      return true;
    }
    return false;
  },
  
  /**
   * Private helper method.
   * @return true if aHostName is an IPv4 address
   */
  isIPv4HostName: function(aHostName)
  {
    var ipv4HostRegExp = new RegExp(/\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/);  // IPv4
    // treat 0.0.0.0 as an invalid IP address
    return ipv4HostRegExp.test(aHostName) && aHostName != '0.0.0.0';
  },
  
  /** 
   * Private helper method.
   * @return true if unobscuredHostName is a local IP address.
   */
  isLocalIPAddress: function(unobscuredHostName)
  {
    var ipComponents = unobscuredHostName.value.split(".");

    return ipComponents[0] == 10 ||
           (ipComponents[0] == 192 && ipComponents[1] == 168) ||
           (ipComponents[0] == 169 && ipComponents[1] == 254) ||
           (ipComponents[0] == 172 && ipComponents[1] >= 16 && ipComponents[1] < 32);
  },
  
  /** 
   * If the current message has been identified as an email scam, prompts the user with a warning
   * before allowing the link click to be processed. The warning prompt includes the unobscured host name
   * of the http(s) url the user clicked on.
   *
   * @param aUrl the url 
   * @return true if the link should be allowed to load
   */
  warnOnSuspiciousLinkClick: function(aUrl)
  {
    // if the loaded message has been flagged as a phishing scam, 
    if (!gMessageNotificationBar.isFlagSet(kMsgNotificationPhishingBar))
      return true;

    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
    var hrefURL;
    // make sure relative link urls don't make us bail out
    try {
      hrefURL = ioService.newURI(aUrl, null, null);
    } catch(ex) { return false; }
    
    // only prompt for http and https urls
    if (hrefURL.schemeIs('http') || hrefURL.schemeIs('https'))
    {
      // unobscure the host name in case it's an encoded ip address..
      var unobscuredHostName = {};
      unobscuredHostName.value = hrefURL.host;
      this.hostNameIsIPAddress(hrefURL.host, unobscuredHostName);
      
      var brandShortName = gBrandBundle.getString("brandShortName");
      var titleMsg = gMessengerBundle.getString("confirmPhishingTitle");
      var dialogMsg = gMessengerBundle.getFormattedString("confirmPhishingUrl", 
                        [brandShortName, unobscuredHostName.value], 2);

      const nsIPS = Components.interfaces.nsIPromptService;
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPS);
      return !promptService.confirmEx(window, titleMsg, dialogMsg, nsIPS.STD_YES_NO_BUTTONS + nsIPS.BUTTON_POS_1_DEFAULT, 
                                     "", "", "", "", {}); /* the yes button is in position 0 */
    }

    return true; // allow the link to load
  }
};
