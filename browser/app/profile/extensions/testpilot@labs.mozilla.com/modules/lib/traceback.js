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
 * The Original Code is Jetpack.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Atul Varma <atul@mozilla.com>
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
