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

function Startup()
{
  // return receipt startup
  receiptSend = document.getElementById("receiptSend");
  notInToCcPref = document.getElementById("notInToCcPref");
  notInToCcLabel = document.getElementById("notInToCcLabel");
  outsideDomainPref = document.getElementById("outsideDomainPref");
  outsideDomainLabel = document.getElementById("outsideDomainLabel");
  otherCasesPref = document.getElementById("otherCasesPref");
  otherCasesLabel = document.getElementById("otherCasesLabel");

  EnableDisableAllowedReceipts();
        
  toggleRemoteImagesPrefUI(document.getElementById('networkImageDisableImagesInMailNews'));
        
  return true;
}                   

// privacy 
function toggleRemoteImagesPrefUI(aCheckbox)
{
  if (aCheckbox.checked) 
  {
    document.getElementById('useWhiteList').removeAttribute('disabled');
    document.getElementById('whiteListAbURI').removeAttribute('disabled');
  }
  else
  {
    document.getElementById('useWhiteList').setAttribute('disabled', 'true');
    document.getElementById('whiteListAbURI').setAttribute('disabled', 'true');
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

// Password Management

function editPasswords()
{
  window.openDialog("chrome://communicator/content/wallet/SignonViewer.xul","_blank","chrome,centerscreen,resizable=yes", "S");
}

function editMasterPassword()
{
  window.openDialog("chrome://messenger/content/pref-masterpass.xul","_blank","chrome,centerscreen,resizable=yes", "masterPassword");
}

function checkForUpdates()
{
  var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                          .getService(Components.interfaces.nsIUpdateService);
  updates.checkForUpdates([], 0, Components.interfaces.nsIUpdateItem.TYPE_ANY, 
                          Components.interfaces.nsIUpdateService.SOURCE_EVENT_USER,
                          window);
}

function showConnections()
{
  openDialog("chrome://messenger/content/pref-connection.xul", "", "centerscreen,chrome,modal=yes,dialog=yes");
}
