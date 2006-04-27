/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Feed Stream Converter.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";

function LOG(str) {
  dump("*** " + str + "\n");
}

var FeedHandler = {
  getPref: function FH_getPref() {
    var ps = 
        Cc["@mozilla.org/preferences-service"].
        getService(Ci.nsIPrefBranch);
    return ps.getIntPref("accessibility.typeaheadfind.flashBar");
  },
  
  init: function FH_init() {
    LOG("window.location.href = " + window.location.href);
    
    var feedService = 
        Cc["@mozilla.org/browser/feeds/result-service;1"].
        getService(Ci.nsIFeedResultService);
        
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    var feedURI = ios.newURI(window.location.href, null, null);
    try {
      var result = feedService.getFeedResult(feedURI);
    }
    catch (e) {
      LOG("feed not available?!");
    }
    
    if (result.bozo) {
      LOG("feed result is bozo?!");
    }
    
    var container = result.doc;
    var introTitle = document.getElementById("introTitle");
    introTitle.value = container.title;
    
    // ...
    
    var e = container.fields.enumerator;
    while (e.hasMoreElements()) {
      var p = e.getNext().QueryInterface(Ci.nsIProperty);
      LOG("P: " + p.name + " = " +p.value);
    }
    
    var parts = container.fields.getProperty("image").QueryInterface(Ci.nsIPropertyBag2);
    var feedLogo = document.getElementById("feedLogo");
    feedLogo.setAttribute("src", parts.getPropertyAsAString("url"));
    
    var feedContent = document.getElementById("feedContent");
    var feed = container.QueryInterface(Ci.nsIFeed);
    for (var i = 0; i < feed.items.length; ++i) {
      var entry = feed.items.queryElementAt(i, Ci.nsIFeedContainer);
      var title = document.createElementNS(XUL_NS, "label");
      //title.value = entry.summary(true, 100);
      title.className = "feedEntryTitle";
      var body = document.createElementNS(XUL_NS, "description");
      //body.appendChild(document.createTextNode(entry.content(true)));
      body.className = "feedEntryContent";
      feedContent.appendChild(title);
      feedContent.appendChild(body);
    }    
  },
  
  openSubscribe: function FH_openSubscribe() {
    var buttonBox = document.getElementById("showSubscribeOptionsButtonBox");
    buttonBox.hidden = true;
    
    var subscribeOptions = document.getElementById("subscribeOptions");
    subscribeOptions.removeAttribute("collapsed");

    // Initialize subscription options
    var popup = document.getElementById("webServiceOptionsPopup");
    this.populateServiceOptions(popup);
    var wso = document.getElementById("webServiceOptions");
    
    var wccr = 
        Cc["@mozilla.org/web-content-handler-registrar;1"].
        getService(Ci.nsIWebContentConverterRegistrar);
    var handlers = wccr.getContentHandlers(TYPE_MAYBE_FEED, { });
    var handler = wccr.getAutoHandler(TYPE_MAYBE_FEED);
    for (var i = 0; i < handlers.length; ++i) {
      if (handler && hander.equals(handlers[i]))
        break;
    }
    wso.selectedIndex = i < handlers.length ? i : 0;
  },
  
  populateServiceOptions: function FH_populateServiceOptions(popup) {
    var wccr = 
        Cc["@mozilla.org/web-content-handler-registrar;1"].
        getService(Ci.nsIWebContentConverterRegistrar);
    var handlers = wccr.getContentHandlers(TYPE_MAYBE_FEED, { });
    
    while (popup.hasChildNodes())
      popup.removeChild(popup.firstChild);
    
    for (var i = 0; i < handlers.length; ++i) {
      var menuitem = document.createElementNS(XUL_NS, "menuitem");
      menuitem.setAttribute("url", handlers[i].url);
      menuitem.setAttribute("label", handlers[i].name);
      popup.appendChild(menuitem);
    }
  },
  
  subscribe: function FH_subscribe() {
    var wccr = 
        Cc["@mozilla.org/web-content-handler-registrar;1"].
        getService(Ci.nsIWebContentConverterRegistrar);
    var autoHandle = document.getElementById("autoHandle");
    
    var wso = document.getElementById("webServiceOptions");
    var handlers = wccr.getContentHandlers(TYPE_MAYBE_FEED, { });
    LOG("Goats: " + wso.selectedIndex);
    NS_ASSERT(wso.selectedIndex < handlers.length,
              "selected index out of bounds?!");
    
    LOG("Handlers: " + handlers);
    var handler = handlers[wso.selectedIndex];
    LOG("Handler: " + handler);
    wccr.setAutoHandler(TYPE_MAYBE_FEED, autoHandle.checked ? handler : null);
    
    var handlerURI = handler.getHandlerURI(window.location.href);
    LOG("Handler URI: " + handlerURI);
    window.location.href = handlerURI;
  },
};

#include ../../../../toolkit/content/debug.js

