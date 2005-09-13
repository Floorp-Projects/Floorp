# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is the Thunderbird Preferences System.
#
# The Initial Developer of the Original Code is
# Scott MacGregor.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Scott MacGregor <mscott@mozilla.org>
#   Asaf Romano <mozilla.mano@sent.com>
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

const kMailApp = 1;
const kNewsApp = 2;
const kFeedApp = 3;

var gDefaultClientPane = {
  mMapiBundle: null,
  mBrandBundle: null,

  init: function ()
  {
    this.mBrandBundle = document.getElementById("brandBundle");
    this.mMapiBundle = document.getElementById("mapiBundle");
  },

  checkDefaultAppNow: function (appType) 
  {
    var mapiReg = Components.classes["@mozilla.org/mapiregistry;1"]
                            .getService(Components.interfaces.nsIMapiRegistry);

    var brandShortName = this.mBrandBundle.getString("brandShortName");
    var promptTitle = this.mMapiBundle
                          .getFormattedString("dialogTitle", [brandShortName]);
    var promptMessage;
    var IPS = Components.interfaces.nsIPromptService;
    var prompt = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                           .getService(IPS);

    var isDefault;
    var str;
    if (appType == kMailApp)
    {
      isDefault = mapiReg.isDefaultMailClient;
      str = "Mail";
    }
    else if (appType == kNewsApp)
    {
      isDefault = mapiReg.isDefaultNewsClient;
      str = "News";
    } 
    else if (appType == kFeedApp)
    {
      isDefault = mapiReg.isDefaultFeedClient;
      str = "Feed";
    }
    else {
      throw ("Invalid appType value passed to gGeneralPane.checkDefaultAppNow");
    }

    if (!isDefault) 
    {
      promptMessage = this.mMapiBundle.getFormattedString("setDefault" + str,
                                                    [brandShortName]);

      var rv = prompt.confirmEx(window, promptTitle, promptMessage,
                                (IPS.BUTTON_TITLE_YES * IPS.BUTTON_POS_0) +
                                (IPS.BUTTON_TITLE_NO * IPS.BUTTON_POS_1),
                                null, null, null, null, { });
      if (rv == 0) 
      {
        if (appType == kMailApp)
          mapiReg.isDefaultMailClient = true;
        else if (appType == kNewsApp)
          mapiReg.isDefaultNewsClient = true;
        else
          mapiReg.isDefaultFeedClient = true;
      }
    }
    else 
    {
      promptMessage = this.mMapiBundle.getFormattedString("alreadyDefault" + str,
                                                          [brandShortName]);

      prompt.alert(window, promptTitle, promptMessage);
    }
  }
};

