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
# The Original Code is Mozilla Communicator.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corp.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Sean Su <ssu@netscape.com>
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

var receiptSend;
var notInToCcPref;
var notInToCcLabel;
var outsideDomainPref;
var outsideDomainLabel;
var otherCasesPref;
var otherCasesLabel;

function setColorWell(aPicker)
{
  var colorRef;

  colorRef = aPicker.nextSibling; // colour value is held here
  colorRef.setAttribute( "value", aPicker.color );
}

function getColorFromWellAndSetValue(aPickerId)
{
  var picker;
  var colorRef;
  var color;

  picker       = document.getElementById(aPickerId);
  colorRef     = picker.nextSibling;
  color        = colorRef.getAttribute("value");
  picker.color = color;

  return color;
}     

function Startup()
{
  getColorFromWellAndSetValue("labelColorPicker1");
  getColorFromWellAndSetValue("labelColorPicker2");
  getColorFromWellAndSetValue("labelColorPicker3");
  getColorFromWellAndSetValue("labelColorPicker4");
  getColorFromWellAndSetValue("labelColorPicker5");

  // return receipt startup
  receiptSend = document.getElementById("receiptSend");
  notInToCcPref = document.getElementById("notInToCcPref");
  notInToCcLabel = document.getElementById("notInToCcLabel");
  outsideDomainPref = document.getElementById("outsideDomainPref");
  outsideDomainLabel = document.getElementById("outsideDomainLabel");
  otherCasesPref = document.getElementById("otherCasesPref");
  otherCasesLabel = document.getElementById("otherCasesLabel");

  EnableDisableAllowedReceipts();

  // proxy connection
  DoEnabling();

  // autocomplete
  enableAutocomplete();
        
  return true;
}                   

/* Function to restore pref values to application defaults */
function restoreColorAndDescriptionToDefaults()
{
  var prefColor;
  var description;
  var pickerColor;
  var dataColor;
  var labelDescription;
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefs = prefService.getDefaultBranch(null);

  /* there are only 5 labels */
	for(var i = 1; i <= 5; i++)
  {
    /* set the default description from prefs */
    description = prefs.getComplexValue("mailnews.labels.description." + i,
                                         Components.interfaces.nsIPrefLocalizedString).data;
    labelDescription = document.getElementById("label" + i + "TextBox");
    labelDescription.value = description;

    /* set the default color from prefs */
    prefColor = prefs.getCharPref("mailnews.labels.color." + i);
    pickerColor = document.getElementById("labelColorPicker" + i);
    pickerColor.color = prefColor;

    // need to call setColorWell() so that the default pref value will be updated
    // in the preferences file.
    setColorWell(pickerColor);
  }
}

// return receipts....
function EnableDisableAllowedReceipts() {
  if (receiptSend && (receiptSend.getAttribute("value") == "false")) {
    notInToCcPref.setAttribute("disabled", "true");
    notInToCcLabel.setAttribute("disabled", "true");
    outsideDomainPref.setAttribute("disabled", "true");
    outsideDomainLabel.setAttribute("disabled", "true");
    otherCasesPref.setAttribute("disabled", "true");
    otherCasesLabel.setAttribute("disabled", "true");
  }
  else {
    notInToCcPref.removeAttribute("disabled");
    notInToCcLabel.removeAttribute("disabled");
    outsideDomainPref.removeAttribute("disabled");
    outsideDomainLabel.removeAttribute("disabled");
    otherCasesPref.removeAttribute("disabled");
    otherCasesLabel.removeAttribute("disabled");
  }
  return true;
}

// proxy config support
function DoEnabling()
{
  var i;
  var http = document.getElementById("networkProxyHTTP");
  var httpPort = document.getElementById("networkProxyHTTP_Port");
  var socks = document.getElementById("networkProxySOCKS");
  var socksPort = document.getElementById("networkProxySOCKS_Port");
  var socksVersion = document.getElementById("networkProxySOCKSVersion");
  var socksVersion4 = document.getElementById("networkProxySOCKSVersion4");
  var socksVersion5 = document.getElementById("networkProxySOCKSVersion5");
  var ssl = document.getElementById("networkProxySSL");
  var sslPort = document.getElementById("networkProxySSL_Port");
  var noProxy = document.getElementById("networkProxyNone");
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var autoReload = document.getElementById("autoReload");

  // convenience arrays
  var manual = [http, httpPort, socks, socksPort, socksVersion, socksVersion4, socksVersion5, ssl, sslPort, noProxy];
  var auto = [autoURL, autoReload];

  // radio buttons
  var radiogroup = document.getElementById("networkProxyType");

  switch ( radiogroup.value ) {
    case "0":
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute( "disabled", "true" );
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      break;
    case "1":
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      if (!radiogroup.disabled)
        for (i = 0; i < manual.length; i++)
          manual[i].removeAttribute( "disabled" );
      break;
    case "2":
    default:
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute( "disabled", "true" );
      if (!radiogroup.disabled)
        for (i = 0; i < auto.length; i++)
          auto[i].removeAttribute( "disabled" );
      break;
  }
}

const nsIProtocolProxyService = Components.interfaces.nsIProtocolProxyService;
const kPROTPROX_CID = '{e9b301c0-e0e4-11D3-a1a8-0050041caf44}';

function ReloadPAC() {
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var pps = Components.classesByID[kPROTPROX_CID]
                       .getService(nsIProtocolProxyService);
  pps.configureFromPAC(autoURL.value);
}   

