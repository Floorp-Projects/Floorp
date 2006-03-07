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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
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


// Some small helper classes to deal with Firefox version numbers. Version
// numbers are of the form:
//
//     major.minor.release.build[+]
//
// where the last three are all optional. Each component is an integer or a
// *, where a * means "infinity." See:
// http://www.mozilla.org/projects/firefox/extensions/update.html
//
// A MozVersionNumber encapsulates a number of the form given above and 
// provides the ability to compare it with other version strings. The
// compareToString() method has the usual comparison return value (less than,
// greater than, or 0 for equal to).
//
// ThisFirefoxVersion encapsulates the version of Firefox we're currently
// running (and is-a MozVersionNumber).
//
// Examples:
//
// var ver = new G_ThisFirefoxVersion();
// dump("Running Firefox v" + ver.getVersion());
// if (ver.isVersionOf("1.5"))
//   do something 1.5-specific
// if (ver.compareToString("1.5") < 0)
//   do something for all versions before 1.5
// if (ver.compareToString("1.0") >= 0)
//   do something for all versions of 1.0 and later 


/** 
 * Encapsulates a Mozilla-style version number. These numbers are used to
 * version Firefox, Thunderbird, and extensions. They have four optional
 * numbers separated by dots. 
 *
 * @param version String holding the version number to initialize with
 * 
 * @constructor
 */
function G_MozVersionNumber(version) {
  this.debugZone = "mozversion";
  this.version_ = version;
  this.components_ = this.version_.split(".");
  this.comparator_ = Cc["@mozilla.org/xpcom/version-comparator;1"]
                     .getService(Ci.nsIVersionComparator);
}

/** 
 * @param v String holding a version number against which to compare
 *
 * @returns Integer indicating how v compares. This value will be less
 *          than zero if v is a later version, equal to zero if the
 *          versions are equal, or greater than zero if v is an earlier
 *          version.
 */
G_MozVersionNumber.prototype.compareToString = function(v) {
  return this.comparator_.compare(this.version_, v);
}

/** 
 * Check to see if we are a "sub-version" of a particular string. By 
 * "sub-version" we mean whether our version numbers match a string's
 * up to the number of components the string has. For example, 
 * 1.5.3 is a version of 1 as well as 1.5 and 1.5.3, but not of 
 * 1.5.3.2.
 *
 * @param v String holding a version number against which to check
 *
 * @returns Boolean indicating whether we are a sub-version of the 
 *          argument.
 */
G_MozVersionNumber.prototype.isVersionOf = function(v) {
  // It's not clear what isVersionOf means for + versions, so we 
  // assume it means equality.
  if (this.version_.indexOf("+") != -1 || v.indexOf("+") != -1)
    return this.compareToString(v) == 0;
  
  // Handle the exact case, e.g., 1.0 vs 1.0.0.0
  if (this.compareToString(v) == 0)
    return true;

  var vComponents = v.split(".");
  
  // Handle the case where v is more specific
  if (vComponents.length > this.components_)
    return false;

  // Handle normal case; look for a prefix match
  for (var i = 0; i < vComponents.length; i++) 
    if (vComponents[i] != this.components_[i])
      return false;
  
  return true;
}

/** 
 * @returns String giving our version
 */
G_MozVersionNumber.prototype.getVersion = function() {
  return this.version_;
}


/** 
 * Encapsulates the currently running Firefox's version number. This is 
 * a subclass of MozVersionNumber.
 * 
 * @constructor
 */
function G_ThisFirefoxVersion() {
  
  // Figure out what version we are so we can initialize the superclass
  var version;
  try {

    // This services was introduced in 1.5, so try it and then fall back 
    // to the old-school way if it fails.
    var appInfo = Cc["@mozilla.org/xre/app-info;1"]
                  .getService(Ci.nsIXULAppInfo);
    version = appInfo.version;

  } catch(e) {
    G_Debug(this, "Getting nsIXULAppInfo threw; trying app.version pref");
    version = (new G_Preferences()).getPref("app.version");
  }
    
  if (!version)
    throw new Error("Couldn't get application version!");

  G_MozVersionNumber.call(this, version);
  this.debugZone = "firefoxversion";
}

G_ThisFirefoxVersion.inherits(G_MozVersionNumber);


/**
 * Cheesey unittests
 */
function TEST_G_MozVersionNumber() {
  if (G_GDEBUG) {
    var z = "mozversion UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");

    var v = G_MozVersionNumber;

    G_Assert(z, (new v("1.0")).compareToString("1.0") == 0, 
             "exact equality broken");
    G_Assert(z, (new v("1.0.0.0")).compareToString("1.0") == 0, 
             "mutlidot equality broken");
    
    G_Assert(z, (new v("1.0.2.1")).compareToString("1.1") < 0, 
             "less than broken");
    G_Assert(z, (new v("1.1")).compareToString("1.0.2.1") > 0, 
             "greater than broken");
    G_Assert(z, (new v("1.0+")).compareToString("1.1pre") == 0, 
             "+ broken");

    G_Assert(z, (new v("1.5")).isVersionOf("1.5"), "exact versionof broken");
    G_Assert(z, (new v("1.5.*")).isVersionOf("1.5"), "star versionof broken");
    G_Assert(z, (new v("1.5.1.3")).isVersionOf("1.5"), 
             "long versionof broken");
    G_Assert(z, (new v("1.1pre0")).isVersionOf("1.0+"), "+'s broken");
    G_Assert(z, !((new v("1.0pre0")).isVersionOf("1.1+")), "+'s broken");
    G_Assert(z, !(new v("1.5")).isVersionOf("1.6"), "not versionof broken");
    G_Assert(z, !(new v("1.5")).isVersionOf("1.5.*"), 
             "not versionof star broken");
    
    

    G_Debug(z, "PASSED");  
  }
}

function TEST_G_ThisFirefoxVersion() {
  if (G_GDEBUG) {
    var z = "firefoxversion UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");

    // Just check that it doesn't throw and that it is consistent
    var v = new G_ThisFirefoxVersion();
    G_Assert(z, v.isVersionOf(v.version_), "Firefox version broken");
    
    G_Debug(z, "PASSED");  
  }
}
