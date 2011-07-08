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
 * The Original Code is Component Scope File Constructor test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011.
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Ci = Components.interfaces;

function do_check_true(cond, text) {
  // we don't have the test harness' utilities in this scope, so we need this
  // little helper. In the failure case, the exception is propagated to the
  // caller in the main run_test() function, and the test fails.
  if (!cond)
    throw "Failed check: " + text;
}

function FileComponent() {
  this.wrappedJSObject = this;
}
FileComponent.prototype =
{
  doTest: function() {
    // throw if anything goes wrong

    // find the current directory path
    var file = Components.classes["@mozilla.org/file/directory_service;1"]
               .getService(Ci.nsIProperties)
               .get("CurWorkD", Ci.nsIFile);
    file.append("xpcshell.ini");

    // should be able to construct a file
    var f1 = File(file.path);
    // with either constructor syntax
    var f2 = new File(file.path);
    // and with nsIFiles
    var f3 = File(file);
    var f4 = new File(file);
    // and extra args are ignored
    var f5 = File(file.path, "foopy");
    var f6 = new File(file, Date(123123154));

    // do some tests
    do_check_true(f1 instanceof Ci.nsIDOMFile, "Should be a DOM File");
    do_check_true(f2 instanceof Ci.nsIDOMFile, "Should be a DOM File");
    do_check_true(f3 instanceof Ci.nsIDOMFile, "Should be a DOM File");
    do_check_true(f4 instanceof Ci.nsIDOMFile, "Should be a DOM File");
    do_check_true(f5 instanceof Ci.nsIDOMFile, "Should be a DOM File");
    do_check_true(f6 instanceof Ci.nsIDOMFile, "Should be a DOM File");

    do_check_true(f1.name == "xpcshell.ini", "Should be the right file");
    do_check_true(f2.name == "xpcshell.ini", "Should be the right file");
    do_check_true(f3.name == "xpcshell.ini", "Should be the right file");
    do_check_true(f4.name == "xpcshell.ini", "Should be the right file");
    do_check_true(f5.name == "xpcshell.ini", "Should be the right file");
    do_check_true(f6.name == "xpcshell.ini", "Should be the right file");

    do_check_true(f1.type = "text/plain", "Should be the right type");
    do_check_true(f2.type = "text/plain", "Should be the right type");
    do_check_true(f3.type = "text/plain", "Should be the right type");
    do_check_true(f4.type = "text/plain", "Should be the right type");
    do_check_true(f5.type = "text/plain", "Should be the right type");
    do_check_true(f6.type = "text/plain", "Should be the right type");

    var threw = false;
    try {
      // Needs a ctor argument
      var f7 = File();
    } catch (e) {
      threw = true;
    }
    do_check_true(threw, "No ctor arguments should throw");

    var threw = false;
    try {
      // Needs a valid ctor argument
      var f7 = File(Date(132131532));
    } catch (e) {
      threw = true;
    }
    do_check_true(threw, "Passing a random object should fail");

    var threw = false
    try {
      // Directories fail
      var dir = Components.classes["@mozilla.org/file/directory_service;1"]
                          .getService(Ci.nsIProperties)
                          .get("CurWorkD", Ci.nsIFile);
      var f7 = File(dir)
    } catch (e) {
      threw = true;
    }
    do_check_true(threw, "Can't create a File object for a directory");

    return true;
  },

  // nsIClassInfo + information for XPCOM registration code in XPCOMUtils.jsm
  classDescription: "File in components scope code",
  classID: Components.ID("{da332370-91d4-464f-a730-018e14769cab}"),
  contractID: "@mozilla.org/tests/component-file;1",

  // nsIClassInfo
  implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
  flags: 0,

  getInterfaces: function getInterfaces(aCount) {
    var interfaces = [Components.interfaces.nsIClassInfo];
    aCount.value = interfaces.length;
    return interfaces;
  },

  getHelperForLanguage: function getHelperForLanguage(aLanguage) {
    return null;
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsIClassInfo])
};

var gComponentsArray = [FileComponent];
var NSGetFactory = XPCOMUtils.generateNSGetFactory(gComponentsArray);
