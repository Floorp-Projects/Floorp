# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Initial Developer of the Original Code is Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
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

var gOCSPDialog = {
  mCertDB         : null,
  mOCSPResponders : null,
  mPrefBranch     : null,

  init: function ()
  {
    this.mCertDB = Components.classes["@mozilla.org/security/x509certdb;1"]
                             .getService(Components.interfaces.nsIX509CertDB);
    this.mOCSPResponders = this.mCertDB.getOCSPResponders();
    this.mPrefBranch = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

    document.getElementById('securityOCSPEnabled').value = this.mPrefBranch.getIntPref('security.OCSP.enabled');

    var selectedItemIndex = 0;
    var signingCAValue;
    try {
      signingCAValue = this.mPrefBranch.getCharPref('security.OCSP.signingCA');
    } catch (ex) {}

    var signingCA = document.getElementById("signingCA");
    var serviceURL = document.getElementById('serviceURL');
    const nsIOCSPResponder = Components.interfaces.nsIOCSPResponder;
    for (var index = 0; index < this.mOCSPResponders.length; ++index) 
    {
      var ocspEntry = this.mOCSPResponders.queryElementAt(index, nsIOCSPResponder);
      var menuitem = document.createElement("menuitem");
      menuitem.setAttribute("value", ocspEntry.responseSigner);
      menuitem.setAttribute("label", ocspEntry.responseSigner);
      signingCA.firstChild.appendChild(menuitem);
      if (ocspEntry.responseSigner == signingCAValue)
        selectedItemIndex = index;       
    }

    // now select the correct item in the list...
    signingCA.selectedIndex = selectedItemIndex;

    try 
    {
      serviceURL.value = this.mPrefBranch.getCharPref('security.OCSP.URL');
    } 
    catch (ex)
    {
      // if there is no default OCSP server URL overrride, then just choose the one for the current signingCA...
      this.chooseServiceURL();
    }
    
    this.updateUI();
  },

  onAccept: function()
  {
    this.mPrefBranch.setIntPref('security.OCSP.enabled', document.getElementById('securityOCSPEnabled').value);
    this.mPrefBranch.setCharPref('security.OCSP.signingCA', document.getElementById('signingCA').value);
    this.mPrefBranch.setCharPref('security.OCSP.URL', document.getElementById('serviceURL').value);
  },
  
  updateUI: function ()
  {
    var signingCA = document.getElementById("signingCA");
    var serviceURL = document.getElementById("serviceURL");
    var securityOCSPEnabled = document.getElementById("securityOCSPEnabled");

    var OCSPEnabled = parseInt(securityOCSPEnabled.value);
    signingCA.disabled = serviceURL.disabled = OCSPEnabled == 0 || OCSPEnabled == 1;
  },
  
  chooseServiceURL: function ()
  {
    var signingCA = document.getElementById("signingCA");
    var serviceURL = document.getElementById("serviceURL");
    var CA = signingCA.value;
    
    const nsIOCSPResponder = Components.interfaces.nsIOCSPResponder;
    for (var i = 0; i < this.mOCSPResponders.length; ++i) 
    {
      var ocspEntry = this.mOCSPResponders.queryElementAt(i, nsIOCSPResponder);
      if (CA == ocspEntry.responseSigner) 
      {
        serviceURL.value = ocspEntry.serviceURL;
        break;
      }
    }
  },
};
