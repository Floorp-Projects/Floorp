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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian Neal <bugzilla@arlen.demon.co.uk>
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

var receiptSend;
var notInToCcPref;
var notInToCcLabel;
var outsideDomainPref;
var outsideDomainLabel;
var otherCasesPref;
var otherCasesLabel;

function Startup() {
  receiptSend = document.getElementById("receiptSend");
  notInToCcPref = document.getElementById("notInToCcPref");
  notInToCcLabel = document.getElementById("notInToCcLabel");
  outsideDomainPref = document.getElementById("outsideDomainPref");
  outsideDomainLabel = document.getElementById("outsideDomainLabel");
  otherCasesPref = document.getElementById("otherCasesPref");
  otherCasesLabel = document.getElementById("otherCasesLabel");

  EnableDisableAllowedReceipts();
        
  return true;
}

function EnableDisableAllowedReceipts() {
  var prefWindow = parent.hPrefWindow;
  var notInToCcLocked = prefWindow.getPrefIsLocked("mail.mdn.report.not_in_to_cc");
  var outsideDomainLocked = prefWindow.getPrefIsLocked("mail.mdn.report.outside_domain");
  var otherCasesLocked = prefWindow.getPrefIsLocked("mail.mdn.report.other");

  var disableAll = receiptSend && (receiptSend.getAttribute("value") == "false");
  notInToCcPref.disabled = disableAll || notInToCcLocked;
  notInToCcLabel.disabled = disableAll;
  outsideDomainPref.disabled = disableAll || outsideDomainLocked;
  outsideDomainLabel.disabled = disableAll;
  otherCasesPref.disabled = disableAll || otherCasesLocked;
  otherCasesLabel.disabled = disableAll;
  return true;
}
