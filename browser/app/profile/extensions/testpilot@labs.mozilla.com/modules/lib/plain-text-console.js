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

function stringify(arg) {
  try {
    return String(arg);
  }
  catch(ex) {
    return "<toString() error>";
  }
}

function stringifyArgs(args) {
  return Array.map(args, stringify).join(" ");
}

function message(print, level, args) {
  print(level + ": " + stringifyArgs(args) + "\n");
}

var Console = exports.PlainTextConsole = function PlainTextConsole(print) {
  if (!print)
    print = dump;
  if (print === dump) {
    // If we're just using dump(), auto-enable preferences so
    // that the developer actually sees the console output.
    var prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Ci.nsIPrefBranch);
    prefs.setBoolPref("browser.dom.window.dump.enabled", true);
  }
  this.print = print;
};

Console.prototype = {
  log: function log() {
    message(this.print, "info", arguments);
  },

  info: function info() {
    message(this.print, "info", arguments);
  },

  warn: function warn() {
    message(this.print, "warning", arguments);
  },

  error: function error() {
    message(this.print, "error", arguments);
  },

  debug: function debug() {
    message(this.print, "debug", arguments);
  },

  exception: function exception(e) {
    var fullString = ("An exception occurred.\n" +
                      require("traceback").format(e) + "\n" + e);
    this.error(fullString);
  },

  trace: function trace() {
    var traceback = require("traceback");
    var stack = traceback.get();
    stack.splice(-1, 1);
    message(this.print, "info", [traceback.format(stack)]);
  }
};
