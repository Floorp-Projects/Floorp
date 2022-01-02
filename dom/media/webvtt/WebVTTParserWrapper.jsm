/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { WebVTT } = ChromeUtils.import("resource://gre/modules/vtt.jsm");

function WebVTTParserWrapper() {
  // Nothing
}

WebVTTParserWrapper.prototype = {
  loadParser(window) {
    this.parser = new WebVTT.Parser(window, new TextDecoder("utf8"));
  },

  parse(data) {
    // We can safely translate the string data to a Uint8Array as we are
    // guaranteed character codes only from \u0000 => \u00ff
    var buffer = new Uint8Array(data.length);
    for (var i = 0; i < data.length; i++) {
      buffer[i] = data.charCodeAt(i);
    }

    this.parser.parse(buffer);
  },

  flush() {
    this.parser.flush();
  },

  watch(callback) {
    this.parser.oncue = callback.onCue;
    this.parser.onregion = callback.onRegion;
    this.parser.onparsingerror = function(e) {
      // Passing the just the error code back is enough for our needs.
      callback.onParsingError("code" in e ? e.code : -1);
    };
  },

  cancel() {
    this.parser.oncue = null;
    this.parser.onregion = null;
    this.parser.onparsingerror = null;
  },

  convertCueToDOMTree(window, cue) {
    return WebVTT.convertCueToDOMTree(window, cue.text);
  },

  processCues(window, cues, overlay, controls) {
    WebVTT.processCues(window, cues, overlay, controls);
  },

  classDescription: "Wrapper for the JS WebVTT implementation (vtt.js)",
  QueryInterface: ChromeUtils.generateQI(["nsIWebVTTParserWrapper"]),
};

var EXPORTED_SYMBOLS = ["WebVTTParserWrapper"];
