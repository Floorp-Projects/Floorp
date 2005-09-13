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

var gPrivacyPane = {
  mPane: null,
  mInitialized: false,

  init: function ()
  {
    this.mPane = document.getElementById("panePrivacy");
    this.updateRemoteImagesState(); 
    
    // Update the MP buttons
    this.updateMasterPasswordButton();

    var preference = document.getElementById("mail.preferences.privacy.selectedTabIndex");
    if (preference.value)
      document.getElementById("privacyPrefs").selectedIndex = preference.value;
    this.mInitialized = true;
  },

  tabSelectionChanged: function ()
  {
    if (this.mInitialized)
      document.getElementById("mail.preferences.privacy.selectedTabIndex")
              .valueFromPreferences = document.getElementById("privacyPrefs").selectedIndex;
  },

  updateRemoteImagesState: function()
  {
    var checkbox = document.getElementById('networkImageDisableImagesInMailNews');

    if (checkbox.checked) 
    {
      document.getElementById('useWhiteList').removeAttribute('disabled');
      document.getElementById('whiteListAbURI').removeAttribute('disabled');
    }
    else
    {
      document.getElementById('useWhiteList').setAttribute('disabled', 'true');
      document.getElementById('whiteListAbURI').setAttribute('disabled', 'true');
    }    
  },

  initReencryptCallback: function()
  {
    var wallet = Components.classes['@mozilla.org/wallet/wallet-service;1'];
    wallet = wallet.getService().QueryInterface(Components.interfaces.nsIWalletService);
    wallet.WALLET_InitReencryptCallback(window);
  },

  viewPasswords: function()
  {
    document.documentElement.openWindow("Toolkit:PasswordManager", "chrome://messenger/content/preferences/viewpasswords.xul",
                                        "", null);
  },

  changeMasterPassword: function ()
  {
    document.documentElement.openSubDialog("chrome://mozapps/content/preferences/changemp.xul",
                                           "", null);
    this.updateMasterPasswordButton();
  },
    
  updateMasterPasswordButton: function ()
  {
    // See if there's a master password and set the button label accordingly
    var secmodDB = Components.classes["@mozilla.org/security/pkcs11moduledb;1"]
                             .getService(Components.interfaces.nsIPKCS11ModuleDB);
    var slot = secmodDB.findSlotByName("");
    if (slot) {
      const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;
      var status = slot.status;
      var noMP = status == nsIPKCS11Slot.SLOT_UNINITIALIZED || 
                 status == nsIPKCS11Slot.SLOT_READY;

      var bundle = document.getElementById("bundlePreferences");
      document.getElementById("setMasterPassword").label =
        bundle.getString(noMP ? "setMasterPassword" : "changeMasterPassword");
      
      document.getElementById("removeMasterPassword").disabled = noMP;
    }
  },

  removeMasterPassword: function ()
  {
    var secmodDB = Components.classes["@mozilla.org/security/pkcs11moduledb;1"]
                              .getService(Components.interfaces.nsIPKCS11ModuleDB); 
    if (secmodDB.isFIPSEnabled) 
    {
      var bundle = document.getElementById("bundlePreferences");
      promptService.alert(window,
                          bundle.getString("pw_change_failed_title"),
                          bundle.getString("pw_change2empty_in_fips_mode"));
    }
    else 
    {
      document.documentElement.openSubDialog("chrome://mozapps/content/preferences/removemp.xul",
                                             "", null);
      this.updateMasterPasswordButton();
      document.getElementById("setMasterPassword").focus();
    }
  },

  showCertificates: function ()
  {
    document.documentElement.openWindow("mozilla:certmanager", "chrome://pippki/content/certManager.xul",
                                        "width=600,height=400", null);
  },
  
  showCRLs: function ()
  {
    document.documentElement.openWindow("Mozilla:CRLManager", "chrome://pippki/content/crlManager.xul",
                                        "width=600,height=400", null);
  },
  
  showOCSP: function ()
  {
    document.documentElement.openSubDialog("chrome://mozapps/content/preferences/ocsp.xul",
                                           "", null);
  },
  
  showSecurityDevices: function ()
  {
    document.documentElement.openWindow("mozilla:devicemanager", "chrome://pippki/content/device_manager.xul",
                                        "width=600,height=400", null);
  }
};
