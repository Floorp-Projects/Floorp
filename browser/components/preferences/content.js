# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Asaf Romano <mozilla.mano@sent.com>
#   Jeff Walden <jwalden+code@mit.edu>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****


var gContentPane = {

  // UTILITY FUNCTIONS

  /**
   * Utility function to enable/disable the button specified by aButtonID based
   * on the value of the Boolean preference specified by aPreferenceID.
   */  
  updateButtons: function (aButtonID, aPreferenceID)
  {
    var button = document.getElementById(aButtonID);
    var preference = document.getElementById(aPreferenceID);
    button.disabled = preference.value != true;
    return undefined;
  },

  /**
   * The exceptions types which may be passed to this._showExceptions().
   */
  _exceptionsParams: {
    popup:   { blockVisible: false, sessionVisible: false, allowVisible: true, prefilledHost: "", permissionType: "popup"   },
    image:   { blockVisible: true,  sessionVisible: false, allowVisible: true, prefilledHost: "", permissionType: "image"   }
  },

  /**
   * Displays the exceptions dialog of the given type, where types map onto the
   * the fields in this._exceptionsParams.
   */  
  _showExceptions: function (aPermissionType)
  {
    var bundlePreferences = document.getElementById("bundlePreferences");
    var params = this._exceptionsParams[aPermissionType];
    params.windowTitle = bundlePreferences.getString(aPermissionType + "permissionstitle");
    params.introText = bundlePreferences.getString(aPermissionType + "permissionstext");
    document.documentElement.openWindow("Browser:Permissions",
                                        "chrome://browser/content/preferences/permissions.xul",
                                        "", params);
  },

  // BEGIN UI CODE

  /*
   * Preferences:
   *
   * dom.disable_open_during_load
   * - true if popups are blocked by default, false otherwise
   * permissions.default.image
   * - an integer:
   *     1   all images should be loaded,
   *     2   no images should be loaded,
   *     3   load only images from the site on which the current page resides
   *         (i.e., if viewing foo.example.com, foo.example.com/foo.jpg and
   *         bar.foo.example.com/bar.jpg load but example.com/quux.jpg does not)
   * javascript.enabled
   * - true if JavaScript is enabled, false otherwise
   */

  // POP-UPS

  /**
   * Displays the popup exceptions dialog where specific site popup preferences
   * can be set.
   */
  showPopupExceptions: function ()
  {
    this._showExceptions("popup");
  },

  // IMAGES

  /**
   * Converts the value of the permissions.default.image preference into a
   * Boolean value for use in determining the state of the "load images"
   * checkbox, returning true if images should be loaded and false otherwise.
   */
  readLoadImages: function ()
  {
    var pref = document.getElementById("permissions.default.image");
    return (pref.value == 1 || pref.value == 3);
  },

  /**
   * Returns the "load images" preference value which maps to the state of the
   * preferences UI.
   */
  writeLoadImages: function ()
  { 
    return (document.getElementById("loadImages").checked) ? 1 : 2;
  },

  /**
   * Displays image exception preferences for which websites can and cannot
   * load images.
   */
  showImageExceptions: function ()
  {
    this._showExceptions("image");
  },

  // JAVASCRIPT

  /**
   * Displays the advanced JavaScript preferences for enabling or disabling
   * various annoying behaviors.
   */
  showAdvancedJS: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/advanced-scripts.xul",
                                           "", null);  
  },

  // FONTS

  /**
   * Displays the fonts dialog, where web page font names and sizes can be
   * configured.
   */  
  configureFonts: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/fonts.xul",
                                           "", null);
  },

  /**
   * Displays the colors dialog, where default web page/link/etc. colors can be
   * configured.
   */
  configureColors: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/colors.xul",
                                           "", null);  
  },

#ifdef MOZ_FEEDS
  // FEEDS
  
  /*
   * Preferences:
   *
   * XXX fill these in when this code is written!
   */

  /**
   * Displays a dialog from which a feed reader can be chosen.
   */
  chooseFeedReader: function ()
  {
    openDialog("chrome://browser/content/feeds/options.xul", "", "modal,centerscreen");
  },

#endif

  // FILE TYPES

  /**
   * Displays the file type configuration dialog.
   */
  configureFileTypes: function ()
  {
    document.documentElement.openWindow("Preferences:DownloadActions",
                                        "chrome://browser/content/preferences/downloadactions.xul",
                                        "", null);
  }

};
