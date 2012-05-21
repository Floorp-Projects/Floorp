/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Undo the auto-parentification of URLs done in bug 418356.
function deParentifyURL(url) {
  return url ? url.split(" -> ").slice(-1)[0] : url;
}

// TODO: We might want to move this function to url or some similar
// module.
function getLocalFile(path) {
  var ios = Cc['@mozilla.org/network/io-service;1']
            .getService(Ci.nsIIOService);
  var channel = ios.newChannel(path, null, null);
  var iStream = channel.open();
  var siStream = Cc['@mozilla.org/scriptableinputstream;1']
                 .createInstance(Ci.nsIScriptableInputStream);
  siStream.init(iStream);
  var data = new String();
  data += siStream.read(-1);
  siStream.close();
  iStream.close();
  return data;
}

function safeGetFileLine(path, line) {
  try {
    var scheme = require("url").parse(path).scheme;
    // TODO: There should be an easier, more accurate way to figure out
    // what's the case here.
    if (!(scheme == "http" || scheme == "https"))
      return getLocalFile(path).split("\n")[line - 1];
  } catch (e) {}
  return null;
}

function errorStackToJSON(stack) {
  var lines = stack.split("\n");

  var frames = [];
  lines.forEach(
    function(line) {
      if (!line)
        return;
      var atIndex = line.indexOf("@");
      var colonIndex = line.lastIndexOf(":");
      var filename = deParentifyURL(line.slice(atIndex + 1, colonIndex));
      var lineNo = parseInt(line.slice(colonIndex + 1));
      var funcSig = line.slice(0, atIndex);
      var funcName = funcSig.slice(0, funcSig.indexOf("("));
      frames.unshift({filename: filename,
                      funcName: funcName,
                      lineNo: lineNo});
    });

  return frames;
};

function nsIStackFramesToJSON(frame) {
  var stack = [];

  while (frame) {
    var filename = deParentifyURL(frame.filename);
    stack.splice(0, 0, {filename: filename,
                        lineNo: frame.lineNumber,
                        funcName: frame.name});
    frame = frame.caller;
  }

  return stack;
};

var fromException = exports.fromException = function fromException(e) {
  if (e instanceof Ci.nsIException)
    return nsIStackFramesToJSON(e.location);
  return errorStackToJSON(e.stack);
};

var get = exports.get = function get() {
  return nsIStackFramesToJSON(Components.stack.caller);
};

var format = exports.format = function format(tbOrException) {
  if (tbOrException === undefined) {
    tbOrException = get();
    tbOrException.splice(-1, 1);
  }

  var tb;
  if (tbOrException.length === undefined)
    tb = fromException(tbOrException);
  else
    tb = tbOrException;

  var lines = ["Traceback (most recent call last):"];

  tb.forEach(
    function(frame) {
      lines.push('  File "' + frame.filename + '", line ' +
                 frame.lineNo + ', in ' + frame.funcName);
      var sourceLine = safeGetFileLine(frame.filename, frame.lineNo);
      if (sourceLine)
        lines.push('    ' + sourceLine.trim());
    });

  return lines.join("\n");
};
