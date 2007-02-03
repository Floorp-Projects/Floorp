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

// We instantiate this variable when we create the application.
var gDataProvider = null;

// An instance of our application is a PROT_Application object. It
// basically just populates a few globals and instantiates wardens and
// the listmanager.

/**
 * An instance of our application. There should be exactly one of these.
 * 
 * Note: This object should instantiated only at profile-after-change
 * or later because the listmanager and the cryptokeymanager need to
 * read and write data files. Additionally, NSS isn't loaded until
 * some time around then (Moz bug #321024).
 *
 * @constructor
 */
function PROT_Application() {
  this.debugZone= "application";

#ifdef DEBUG
  // TODO This is truly lame; we definitely want something better
  function runUnittests() {
    if (false) {

      G_DebugL("UNITTESTS", "STARTING UNITTESTS");
      TEST_G_Protocol4Parser();
      TEST_G_Base64();
      TEST_G_CryptoHasher();
      TEST_PROT_EnchashDecrypter();
      TEST_PROT_TRTable();
      TEST_PROT_ListManager();
      TEST_PROT_PhishingWarden();
      TEST_PROT_TRFetcher();
      TEST_G_ObjectSafeMap();
      TEST_PROT_URLCanonicalizer();
      TEST_G_Preferences();
      TEST_G_Observer();
      TEST_G_File();
      TEST_PROT_WireFormat();
      // UrlCrypto's test should come before the key manager's
      TEST_PROT_UrlCrypto();
      TEST_PROT_UrlCryptoKeyManager();
      TEST_G_MozVersionNumber();
      TEST_G_ThisFirefoxVersion();
      G_DebugL("UNITTESTS", "END UNITTESTS");
    }
  };

  runUnittests();
#endif
  
  // expose some classes
  this.G_TabbedBrowserWatcher = G_TabbedBrowserWatcher;
  this.PROT_Controller = PROT_Controller;
  this.PROT_PhishingWarden = PROT_PhishingWarden;

  // Load data provider pref values
  gDataProvider = new PROT_DataProvider();

  // expose the object
  this.wrappedJSObject = this;
}

/**
 * @param name String The type of url to get (either Phish or Error).
 * @return String the report phishing URL (localized).
 */
PROT_Application.prototype.getReportURL = function(name) {
  return gDataProvider["getReport" + name + "URL"]();
}
