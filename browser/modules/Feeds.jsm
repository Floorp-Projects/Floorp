/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [ "Feeds" ];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserWindowTracker",
                               "resource:///modules/BrowserWindowTracker.jsm");

var Feeds = {
  // Listeners are added in nsBrowserGlue.js
  receiveMessage(aMessage) {
    let data = aMessage.data;
    switch (aMessage.name) {
      case "WCCR:registerProtocolHandler": {
        let registrar = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
                          getService(Ci.nsIWebContentHandlerRegistrar);
        registrar.registerProtocolHandler(data.protocol, data.uri, data.title,
                                          aMessage.target);
        break;
      }

      case "WCCR:registerContentHandler": {
        let registrar = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
                          getService(Ci.nsIWebContentHandlerRegistrar);
        registrar.registerContentHandler(data.contentType, data.uri, data.title,
                                         aMessage.target);
        break;
      }

      case "WCCR:setAutoHandler": {
        let registrar = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
                          getService(Ci.nsIWebContentConverterService);
        registrar.setAutoHandler(data.contentType, data.handler);
        break;
      }

      case "FeedConverter:addLiveBookmark": {
        let topWindow = BrowserWindowTracker.getTopWindow();
        topWindow.PlacesCommandHook.addLiveBookmark(data.spec, data.title)
                                   .catch(Cu.reportError);
        break;
      }
    }
  },

  /**
   * isValidFeed: checks whether the given data represents a valid feed.
   *
   * @param  aLink
   *         An object representing a feed with title, href and type.
   * @param  aPrincipal
   *         The principal of the document, used for security check.
   * @param  aIsFeed
   *         Whether this is already a known feed or not, if true only a security
   *         check will be performed.
   */
  isValidFeed(aLink, aPrincipal, aIsFeed) {
    if (!aLink || !aPrincipal)
      return false;

    var type = aLink.type.toLowerCase().replace(/^\s+|\s*(?:;.*)?$/g, "");
    if (!aIsFeed) {
      aIsFeed = (type == "application/rss+xml" ||
                 type == "application/atom+xml");
    }

    if (aIsFeed) {
      try {
        let href = Services.io.newURI(aLink.href, aLink.ownerDocument.characterSet);
        BrowserUtils.urlSecurityCheck(href, aPrincipal,
                                      Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);
        return type || "application/rss+xml";
      } catch (ex) {
      }
    }

    return null;
  },

};
