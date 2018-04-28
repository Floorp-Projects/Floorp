/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/vtt.jsm");

var WEBVTTPARSERWRAPPER_CID = "{acf6e493-0092-4b26-b172-241e375c57ab}";
var WEBVTTPARSERWRAPPER_CONTRACTID = "@mozilla.org/webvttParserWrapper;1";

function WebVTTParserWrapper()
{
  // Nothing
}

WebVTTParserWrapper.prototype =
{
  loadParser: function(window)
  {
    this.parser = new WebVTT.Parser(window,  new TextDecoder("utf8"));
  },

  parse: function(data)
  {
    // We can safely translate the string data to a Uint8Array as we are
    // guaranteed character codes only from \u0000 => \u00ff
    var buffer = new Uint8Array(data.length);
    for (var i = 0; i < data.length; i++) {
      buffer[i] = data.charCodeAt(i);
    }

    this.parser.parse(buffer);
  },

  flush: function()
  {
    this.parser.flush();
  },

  watch: function(callback)
  {
    this.parser.oncue = callback.onCue;
    this.parser.onregion = callback.onRegion;
    this.parser.onparsingerror = function(e) {
      // Passing the just the error code back is enough for our needs.
      callback.onParsingError(("code" in e) ? e.code : -1);
    };
  },

  convertCueToDOMTree: function(window, cue)
  {
    return WebVTT.convertCueToDOMTree(window, cue.text);
  },

  processCues: function(window, cues, overlay, controls)
  {
    WebVTT.processCues(window, cues, overlay, controls);
  },

  classDescription: "Wrapper for the JS WebVTT implementation (vtt.js)",
  classID: Components.ID(WEBVTTPARSERWRAPPER_CID),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebVTTParserWrapper]),
  classInfo: XPCOMUtils.generateCI({
    classID:    WEBVTTPARSERWRAPPER_CID,
    contractID: WEBVTTPARSERWRAPPER_CONTRACTID,
    interfaces: [Ci.nsIWebVTTParserWrapper]
  })
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebVTTParserWrapper]);
