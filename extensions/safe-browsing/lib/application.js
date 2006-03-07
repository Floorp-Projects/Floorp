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


// An instance of our application is a PROT_Application object. It
// basically just populates a few globals and instantiates wardens and
// the listmanager.

var PROT_globalStore;
var PROT_listManager;
var PROT_phishingWarden;

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

  // TODO This is truly lame; we definitely want something better
  function runUnittests() {
    if (G_GDEBUG) {

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

  // We keep names here
  PROT_globalStore = new PROT_GlobalStore("extensions.safebrowsing.");

  // We run long-lived operations on "threads" (user-level
  // time-slices) in order to prevent locking up the UI
  var threadConfig = {
    "interleave": true,
    "runtime": 80,
    "delay": 400,
  };
  var threadQueue = new TH_ThreadQueue(threadConfig);
  
  PROT_listManager = new PROT_ListManager(threadQueue);
  PROT_phishingWarden = new PROT_PhishingWarden(PROT_listManager);

  // Reads keys from profile directory. Note that this object isn't
  // garbage collected because it places a reference to itself on the
  // urlcrypto's prototype.
  new PROT_UrlCryptoKeyManager();

  runUnittests();

  // We need to store stuff (lists we've downloaded, keys, etc.) in disk
  var appDir = new PROT_ApplicationDirectory();
  if (!appDir.exists())
    appDir.create();
  PROT_listManager.setAppDir(appDir.getAppDirFileInterface());
  PROT_listManager.maybeStartManagingUpdates();
}

/**
 * We might be in an environment where the loader might not want to
 * actually start safebrowsing (instantiate the Application). For
 * example, in the SB version in the Google Toolbar we don't want to
 * load if the user has 1.0, or if the user already has the separate
 * SafeBrowsing extension installed. This static method embodies
 * such contstraints. 
 * 
 * Note: this method must only be called AFTER profile-after-change
 * because it might need to look for extensions in the user's profile
 * directory.
 * 
 * @returns Boolean indicating if this verison of the SafeBrowsing
 *          application is compatible with the user's version of
 *          Firefox.
 * @static
 */
PROT_Application.isCompatibleWithThisFirefox = function() {
  var z = "application";

  // At the moment we always try to load. We don't collide with the
  // stand-alone SafeBrowsing extension because it doesn't claim 
  // compatibility with anything other than 1.5. We don't collide
  // with the SafeBrowsing feature in the Google Toolbar because
  // SB will be removed from any Toolbar that claims compatibility
  // greater than 1.5. Note that we need at least Ffox 1.5 because
  // we use the cryptohasher interface.
  return true;

  // As an example of the kind of thing that could go here, here's
  // what SB in the Google Toolbar uses to determine if it should run.

  /***************************************************************
   *
   *  // SafeBrowsing will only work with 1.5 due to dependency on the
   *  // crypto hasher that was added as of that version. 
   *
   *  var compatibleVersion = (new G_ThisFirefoxVersion()).isVersionOf("1.5");
   *  G_Debug(z, "Firefox version is compatible (1.5)? " + compatibleVersion);
   *
   *  // Additionally, we don't want to load if the SafeBrowsing extension
   *  // is installed. We check by querying the extensionmanager for that
   *  // extension's GUID.
   * 
   *  var extensionInstalled = false;
   *  try {
   *    var em = Cc["@mozilla.org/extensions/manager;1"]
   *             .getService(Ci.nsIExtensionManager);
   *    var loc = em.getInstallLocation("safebrowsing@google.com");
   *
   *    if (loc != null)
   *      extensionInstalled = true;
   *
   *  } catch(e) {
   *    dump("Masked in isCompatibleWithThisFirefox: " + e);
   *  }
   *  G_Debug(z, "SafeBrowsing extension already installed? " + 
   *          extensionInstalled);
   *  
   *  return !extensionInstalled && compatibleVersion;
   ***************************************************************/
}

/**
 * A simple helper class that enables us to get or create the
 * directory in which our app will store stuff.
 */
function PROT_ApplicationDirectory() {
  this.debugZone = "appdir";
  this.appDir_ = G_File.getProfileFile();
  this.appDir_.append(PROT_globalStore.getAppDirectoryName());
  G_Debug(this, "Application directory is " + this.appDir_.path);
}

/**
 * @returns Boolean indicating if the directory exists
 */
PROT_ApplicationDirectory.prototype.exists = function() {
  return this.appDir_.exists() && this.appDir_.isDirectory();
}

/**
 * Creates the directory
 */
PROT_ApplicationDirectory.prototype.create = function() {
  G_Debug(this, "Creating app directory: " + this.appDir_.path);
  try {
    this.appDir_.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
  } catch(e) {
    G_Error(this, this.appDir_.path + " couldn't be created.");
  }
}

/**
 * @returns The nsIFile interface of the directory
 */
PROT_ApplicationDirectory.prototype.getAppDirFileInterface = function() {
  return this.appDir_;
}
