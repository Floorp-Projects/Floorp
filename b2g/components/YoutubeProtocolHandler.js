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
  newURI: function yt_phNewURI(aSpec, aOriginCharset, aBaseURI) {
    let uri = Cc["@mozilla.org/network/standard-url;1"]
              .createInstance(Ci.nsIStandardURL);
    let id = aSpec.substring(this.scheme.length + 1);
    id = id.substring(0, id.indexOf('?'));
    uri.init(Ci.nsIStandardURL.URLTYPE_STANDARD, -1, this.scheme + "://dummy_host/" + id, aOriginCharset,
             aBaseURI);
    return uri.QueryInterface(Ci.nsIURI);
  },

  newChannel: function yt_phNewChannel(aURI) {
    // Get a list of streams for this video id.
    let infoURI = "http://www.youtube.com/get_video_info?&video_id=" +
                  aURI.path.substring(1);

    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", infoURI, true);
    xhr.addEventListener("load", function() {
      try {
        let info = parseYoutubeVideoInfo(xhr.responseText);
        cpmm.sendAsyncMessage("content-handler", info);
      }
      catch(e) {
        // If parseYoutubeVideoInfo() can't find a video URL, it
        // throws an Error. We report the error message here and do
        // nothing. This shouldn't happen often. But if it does, the user
        // will find that clicking on a video doesn't do anything.
        log(e.message);
      }
    });
    xhr.send(null);

    function log(msg) {
      msg = "YoutubeProtocolHandler.js: " + (msg.join ? msg.join(" ") : msg);
      Cc["@mozilla.org/consoleservice;1"]
        .getService(Ci.nsIConsoleService)
        .logStringMessage(msg);
    }

    // 
    // Parse the response from a youtube get_video_info query.
    // 
    // If youtube's response is a failure, this function returns an object
    // with status, errorcode, type and reason properties. Otherwise, it returns
    // an object with status, url, and type properties, and optional
    // title, poster, and duration properties.
    // 
    function parseYoutubeVideoInfo(response) {
      // Splits parameters in a query string.
      function extractParameters(q) {
        let params = q.split("&");
        let result = {};
        for(let i = 0, n = params.length; i < n; i++) {
          let param = params[i];
          let pos = param.indexOf('=');
          if (pos === -1) 
            continue;
          let name = param.substring(0, pos);
          let value = param.substring(pos+1);
          result[name] = decodeURIComponent(value);
        }
        return result;
      }

      let params = extractParameters(response);
      
      // If the request failed, return an object with an error code
      // and an error message
      if (params.status === 'fail') {
        // 
        // Hopefully this error message will be properly localized.
        // Do we need to add any parameters to the XMLHttpRequest to 
        // specify the language we want?
        // 
        // Note that we include fake type and url properties in the returned
        // object. This is because we still need to trigger the video app's
        // view activity handler to display the error message from youtube,
        // and those parameters are required.
        //
        return {
          status: params.status,
          errorcode: params.errorcode,
          reason:  (params.reason || '').replace(/\+/g, ' '),
          type: 'video/3gpp',
          url: 'https://m.youtube.com'
        }
      }

      // Otherwise, the query was successful
      let result = {
        status: params.status,
      };

      // Now parse the available streams
      let streamsText = params.url_encoded_fmt_stream_map;
      if (!streamsText)
        throw Error("No url_encoded_fmt_stream_map parameter");
      let streams = streamsText.split(',');
      for(let i = 0, n = streams.length; i < n; i++) {
        streams[i] = extractParameters(streams[i]);
      }

      // This is the list of youtube video formats, ordered from worst
      // (but playable) to best.  These numbers are values used as the
      // itag parameter of each stream description. See
      // https://en.wikipedia.org/wiki/YouTube#Quality_and_codecs
      let formats = [
        "17", // 144p 3GP
        "36", // 240p 3GP
        "43", // 360p WebM
#ifdef MOZ_WIDGET_GONK
        "18", // 360p H.264
#endif
      ];

      // Sort the array of stream descriptions in order of format
      // preference, so that the first item is the most preferred one
      streams.sort(function(a, b) {
        let x = a.itag ? formats.indexOf(a.itag) : -1;
        let y = b.itag ? formats.indexOf(b.itag) : -1;
        return y - x;
      });

      let bestStream = streams[0];

      // If the best stream is a format we don't support just return
      if (formats.indexOf(bestStream.itag) === -1) 
        throw Error("No supported video formats");

      result.url = bestStream.url + '&signature=' + (bestStream.sig || '');
      result.type = bestStream.type;
      // Strip codec information off of the mime type
      if (result.type && result.type.indexOf(';') !== -1) {
        result.type = result.type.split(';',1)[0];
      }

      if (params.title) {
        result.title = params.title.replace(/\+/g, ' ');
      }
      if (params.length_seconds) {
        result.duration = params.length_seconds;
      }
      if (params.thumbnail_url) {
        result.poster = params.thumbnail_url;
      }

      return result;
    }

    throw Components.results.NS_ERROR_ILLEGAL_VALUE;
  },

  allowPort: function yt_phAllowPort(aPort, aScheme) {
    return false;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([YoutubeProtocolHandler]);
