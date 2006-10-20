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
 * The Original Code is Composer.
 *
 * The Initial Developer of the Original Code is
 * Disruptive Innovations SARL.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <daniel.glazman@disruptive-innovations.com>, Original author
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

var L10NUtils = {

  /********** CONSTANTS **********/

  kCOMPOSER_PROPERTIES: "chrome://composer/locale/composer.properties",

  /********** ATTRIBUTES **********/

  mStringBundleService: null,
  mStringBundle: null,

  /********** PRIVATE **********/

  _getStringBundleService: function _getStringBundleService()
  {
    if (!this.mStringBundleService)
    {
      try {
        this.mStringBundleService =
            Components.classes["@mozilla.org/intl/stringbundle;1"]
                      .getService(Components.interfaces.nsIStringBundleService); 
      } catch (e) { dump (e)+"\n"; }
    }
    return this.mStringBundleService;
  },

  _getStringFromBundle: function _getStringFromBundle(aBundle, aName)
  {
    if (aBundle)
    {
      try {
        return aBundle.GetStringFromName(aName);
      } catch (e) { dump (e)+"\n"; }
    }
    return null;
  },

  _getBundleFromURL: function _getBundleFromURL(aProperties)
  {
    var stringBundle;
    if (this._getStringBundleService())
      try {
        stringBundle = this.mStringBundleService.createBundle(aProperties);
      } catch (e) { dump (e)+"\n"; }
    return stringBundle;
  },

  /********** PUBLIC **********/

  getString: function getString(aName)
  {
    return this._getStringFromBundle(this.getBundle(), aName);
  },

  getStringFromURL: function getStringFromURL(aName, aProperties)
  {
    var stringBundle;
    try {
      stringBundle = this._getBundleFromURL(aProperties); 
    } catch (e) { dump (e)+"\n"; }

    return this._getStringFromBundle(stringBundle, aName);
  },

  getBundle: function getBundle()
  {
    if (!this.mStringBundle)
      try {
        this.mStringBundle = this._getBundleFromURL(this.kCOMPOSER_PROPERTIES); 
      } catch (e) { dump (e)+"\n"; }

    return this.mStringBundle;
  }
};
