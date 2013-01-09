/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"]
           .getService(Ci.nsIMessageSender);
});

function YoutubeProtocolHandler() {
}

YoutubeProtocolHandler.prototype = {
  classID: Components.ID("{c3f1b945-7e71-49c8-95c7-5ae9cc9e2bad}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler]),

  scheme: "vnd.youtube",
  defaultPort: -1,
  protocolFlags: Ci.nsIProtocolHandler.URI_NORELATIVE |
                 Ci.nsIProtocolHandler.URI_NOAUTH |
                 Ci.nsIProtocolHandler.URI_LOADABLE_BY_ANYONE,

  // Sample URL:
  // vnd.youtube:iNuKL2Gy_QM?vndapp=youtube_mobile&vndclient=mv-google&vndel=watch&vnddnc=1
  // Note that there is no hostname, so we use URLTYPE_NO_AUTHORITY
  newURI: function yt_phNewURI(aSpec, aOriginCharset, aBaseURI) {
    let uri = Cc["@mozilla.org/network/standard-url;1"]
              .createInstance(Ci.nsIStandardURL);
    uri.init(Ci.nsIStandardURL.URLTYPE_NO_AUTHORITY, this.defaultPort,
             aSpec, aOriginCharset, aBaseURI);
    return uri.QueryInterface(Ci.nsIURI);
  },

  newChannel: function yt_phNewChannel(aURI) {
    /*
     * This isn't a real protocol handler. Instead of creating a channel
     * we just send a message and throw an exception. This 'content-handler'
     * message is handled in b2g/chrome/content/shell.js where it starts
     * an activity request that will open the Video app. The video app
     * includes code to handle this fake 'video/youtube' mime type
     */
    cpmm.sendAsyncMessage("content-handler", {
      type: 'video/youtube', // A fake MIME type for the activity handler
      url: aURI.spec         // The path component of this URL is the video id
    });

    throw Components.results.NS_ERROR_ILLEGAL_VALUE;
  },

  allowPort: function yt_phAllowPort(aPort, aScheme) {
    return false;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([YoutubeProtocolHandler]);
