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
# The Original Code is .
# 
# The Initial Developer of the Original Code is . 
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
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

#define PROVISIONAL_SECURITY_UI

var _elementIDs = ["moveSystemCaret", "hideTabBar",
                    "loadInBackground", "warnOnClose", "useAutoScrolling",
                    "useSmoothScrolling", "enableAutoImageResizing",
                    "useSSL2", "useSSL3", "useTLS1", "useTypeAheadFind",
                    "linksOnlyTypeAheadFind",
#ifdef PROVISIONAL_SECURITY_UI
                    "certSelection", "securityOCSPEnabled", "serviceURL", "signingCA",
#endif
                    "enableSoftwareInstall", "enableSmartUpdate", 
                    "enableExtensionUpdate", "enableAutoInstall"];

#ifdef PROVISIONAL_SECURITY_UI
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIOCSPResponder = Components.interfaces.nsIOCSPResponder;
const nsISupportsArray = Components.interfaces.nsISupportsArray;

var certdb;
var ocspResponders;
#endif
function Startup() {
  updatePrefs();
#ifdef PROVISIONAL_SECURITY_UI
  var ocspEntry;
  var i;

  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  ocspResponders = certdb.getOCSPResponders();

  var signersMenu = document.getElementById("signingCA");
  var signersURL = document.getElementById("serviceURL");
  for (i=0; i<ocspResponders.length; i++) {
    ocspEntry = ocspResponders.queryElementAt(i, nsIOCSPResponder);
    var menuItemNode = document.createElement("menuitem");
    menuItemNode.setAttribute("value", ocspEntry.responseSigner);
    menuItemNode.setAttribute("label", ocspEntry.responseSigner);
    signersMenu.firstChild.appendChild(menuItemNode);
  }

  doSecurityEnabling();
  
  // XXXben menulists suck, see explanation in pref-privacy.js
  // style resolution problem inside scrollable areas. 
  var scb = document.getElementById("signingCABox");
  var sca = document.getElementById("signingCA");
  sca.removeAttribute("hidden");
  sca.parentNode.removeChild(sca);
  scb.appendChild(sca);
#endif
}

function updatePrefs() {
  var enabled = document.getElementById("useTypeAheadFind").checked;
  var linksOnly = document.getElementById("linksOnlyTypeAheadFind");
  linksOnly.disabled = !enabled;
}

#ifdef PROVISIONAL_SECURITY_UI
function doSecurityEnabling()
{
  var signersMenu = document.getElementById("signingCA");
  var signersURL = document.getElementById("serviceURL");
  var radiogroup = document.getElementById("securityOCSPEnabled");
  
  switch ( radiogroup.value ) {
  case "0":
  case "1":
    signersMenu.setAttribute("disabled", true);
    signersURL.setAttribute("disabled", true);
    break;
  case "2":
  default:
    signersMenu.removeAttribute("disabled");
    signersURL.removeAttribute("disabled");
  }
}

function changeURL()
{
  var signersMenu = document.getElementById("signingCA");
  var signersURL = document.getElementById("serviceURL");
  var CA = signersMenu.getAttribute("value");
  var i;
  var ocspEntry;

  for (i=0; i < ocspResponders.length; i++) {
    ocspEntry = ocspResponders.queryElementAt(i, nsIOCSPResponder);
    if (CA == ocspEntry.responseSigner) {
      signersURL.setAttribute("value", ocspEntry.serviceURL);
      break;
    }
  }
}

function openCrlManager()
{
    window.open('chrome://pippki/content/crlManager.xul',  "",
                'chrome,width=500,height=400,resizable=1');
}

function openCertManager()
{
  //check for an existing certManager window and focus it; it's not application modal
  const kWindowMediatorContractID = "@mozilla.org/appshell/window-mediator;1";
  const kWindowMediatorIID = Components.interfaces.nsIWindowMediator;
  const kWindowMediator = Components.classes[kWindowMediatorContractID].getService(kWindowMediatorIID);
  var lastCertManager = kWindowMediator.getMostRecentWindow("mozilla:certmanager");
  if (lastCertManager)
    lastCertManager.focus();
  else {
    window.open('chrome://pippki/content/certManager.xul',  "",
                'chrome,centerscreen,resizable=yes,dialog=no');
  }
}

function openDeviceManager()
{
  //check for an existing deviceManger window and focus it; it's not application modal
  const kWindowMediatorContractID = "@mozilla.org/appshell/window-mediator;1";
  const kWindowMediatorIID = Components.interfaces.nsIWindowMediator;
  const kWindowMediator = Components.classes[kWindowMediatorContractID].getService(kWindowMediatorIID);
  var lastCertManager = kWindowMediator.getMostRecentWindow("mozilla:devicemanager");
  if (lastCertManager)
    lastCertManager.focus();
  else {
    window.open('chrome://pippki/content/device_manager.xul',  "devmgr",
                'chrome,centerscreen,resizable=yes,dialog=no');
  }
}
#endif

function checkForUpdates()
{
  var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                          .getService(Components.interfaces.nsIUpdateService);
  updates.checkForUpdates([], 0, Components.interfaces.nsIUpdateItem.TYPE_ANY, 
                          Components.interfaces.nsIUpdateService.SOURCE_EVENT_USER,
                          window);
}

