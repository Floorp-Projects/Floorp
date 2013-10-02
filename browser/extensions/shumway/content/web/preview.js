/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/*
 * Copyright 2013 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// Extenstion communication object... as it used in pdf.js
var FirefoxCom = (function FirefoxComClosure() {
  return {
    /**
     * Creates an event that the extension is listening for and will
     * synchronously respond to.
     * NOTE: It is reccomended to use request() instead since one day we may not
     * be able to synchronously reply.
     * @param {String} action The action to trigger.
     * @param {String} data Optional data to send.
     * @return {*} The response.
     */
    requestSync: function(action, data) {
      var request = document.createTextNode('');
      document.documentElement.appendChild(request);

      var sender = document.createEvent('CustomEvent');
      sender.initCustomEvent('shumway.message', true, false,
          {action: action, data: data, sync: true});
      request.dispatchEvent(sender);
      var response = sender.detail.response;
      document.documentElement.removeChild(request);
      return response;
    },
    /**
     * Creates an event that the extension is listening for and will
     * asynchronously respond by calling the callback.
     * @param {String} action The action to trigger.
     * @param {String} data Optional data to send.
     * @param {Function} callback Optional response callback that will be called
     * with one data argument.
     */
    request: function(action, data, callback) {
      var request = document.createTextNode('');
      request.setUserData('action', action, null);
      request.setUserData('data', data, null);
      request.setUserData('sync', false, null);
      if (callback) {
        request.setUserData('callback', callback, null);

        document.addEventListener('shumway.response', function listener(event) {
          var node = event.target,
              response = event.detail.response;

          document.documentElement.removeChild(node);

          document.removeEventListener('shumway.response', listener, false);
          return callback(response);
        }, false);
      }
      document.documentElement.appendChild(request);

      var sender = document.createEvent('CustomEvent');
      sender.initCustomEvent('shumway.message', true, false,
          {action: action, data: data, sync: false});
      return request.dispatchEvent(sender);
    }
  };
})();

function fallback() {
  FirefoxCom.requestSync('fallback', null)
}

var BYTES_TO_LOAD = 32768;
var BYTES_TO_PARSE = 32768;

function runSniffer() {
  var flashParams = JSON.parse(FirefoxCom.requestSync('getPluginParams', null));
  document.head.getElementsByTagName('base')[0].href = flashParams.baseUrl;
  movieUrl = flashParams.url;
  document.getElementById('playbutton').addEventListener('click', function() {
    switchToFullMode();
  });
  document.getElementById('fullmode').addEventListener('click', function() {
    switchToFullMode();
    return false;
  });
  document.getElementById('fallback').addEventListener('click', function() {
    fallback();
    return false;
  });
  FirefoxCom.requestSync('loadFile', {url: movieUrl, sessionId: 0, limit: BYTES_TO_LOAD});
}

var subscription, movieUrl, buffers = [];;

addEventListener("message", function handlerMessage(e) {
  var args = e.data;
  switch (args.callback) {
    case "loadFile":
      if (args.sessionId != 0) {
        return;
      }
      switch (args.topic) {
        case "progress":
          buffers.push(args.array);
          break;
        case "error":
          console.error('Unable to download ' + movieUrl + ': ' + args.error);
          break;
        case "close":
          parseSwf();
          break;
      }
      break;
  }
}, true);

function inflateData(bytes, outputLength) {
  verifyDeflateHeader(bytes);
  var stream = new Stream(bytes, 2);
  var output = {
    data: new Uint8Array(outputLength),
    available: 0,
    completed: false
  };
  var state = {};
  // inflate while we can
  try {
    do {
      inflateBlock(stream, output, state);
    } while (!output.completed && stream.pos < stream.end
        && output.available < outputLength);
  } catch (e) {
    console.log('inflate aborted: ' + e);
  }
  return new Stream(output.data, 0, Math.min(output.available, outputLength));
}

function parseSwf() {
  var sum = 0;
  for (var i = 0; i < buffers.length; i++)
    sum += buffers[i].length;
  var data = new Uint8Array(sum), j = 0;
  for (var i = 0; i < buffers.length; i++) {
    data.set(buffers[i], j); j += buffers[i].length;
  }

  var backgroundColor;
  try {
    var magic1 = data[0];
    var magic2 = data[1];
    var magic3 = data[2];
    if ((magic1 !== 70 && magic1 !== 67) || magic2 !== 87 || magic3 !== 83)
      throw new Error('unsupported file format');

    var compressed = magic1 === 67;
    var stream = compressed ? inflateData(data.subarray(8), BYTES_TO_PARSE) :
        new Stream(data, 8, data.length - 8);
    var bytes = stream.bytes;

    var SWF_TAG_CODE_SET_BACKGROUND_COLOR = 9;
    var PREFETCH_SIZE = 17 /* RECT */ +
        4  /* Frames rate and count */;;
    stream.ensure(PREFETCH_SIZE);
    var rectFieldSize = bytes[stream.pos] >> 3;
    stream.pos += ((5 + 4 * rectFieldSize + 7) >> 3) + 4; // skipping other header fields

    // for now just sniffing background color
    while (stream.pos < stream.end &&
        !backgroundColor) {
      stream.ensure(2);
      var tagCodeAndLength = stream.getUint16(stream.pos, true);
      stream.pos += 2;
      var tagCode = tagCodeAndLength >> 6;
      var length = tagCodeAndLength & 0x3F;
      if (length == 0x3F) {
        stream.ensure(4);
        length = stream.getInt32(stream.pos, true);
        stream.pos += 4;
        if (length < 0) throw new Error('invalid length');
      }
      stream.ensure(length);
      switch (tagCode) {
        case SWF_TAG_CODE_SET_BACKGROUND_COLOR:
          backgroundColor = 'rgb(' + bytes[stream.pos] + ', ' +
              bytes[stream.pos + 1] + ', ' +
              bytes[stream.pos + 2] + ')';
          break;
      }
      stream.pos += length;
    }
  } catch (e) {
    console.log('parsing aborted: ' + e);
  }
  if (backgroundColor) {
    document.body.style.backgroundColor = backgroundColor;
  }
}

document.addEventListener('keydown', function (e) {
  if (e.keyCode == 119 && e.ctrlKey) { // Ctrl+F8
    window.location.replace("data:application/x-moz-playpreview;,application/x-shockwave-flash,full,paused=true");
  }
}, false);

function switchToFullMode() {
  window.location.replace("data:application/x-moz-playpreview;,application/x-shockwave-flash,full");
}
