/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

window.addEventListener("load", SetPrivacyButton, false);
window.addEventListener("unload", RemovePrivacyButton, false);

var privacyUI;

function SetPrivacyButton()
{
  var ui = Components.classes["@mozilla.org/p3p-browser-ui;1"].createInstance();
  if (ui) {
    privacyUI = ui.QueryInterface(Components.interfaces.nsIP3PUI);

    if (privacyUI) {
      var button = document.getElementById("privacy-button");
      if (button && window._content) {
        privacyUI.Init(window._content, button);
      }
    }
  }
}

function RemovePrivacyButton()
{
  if (privacyUI) {
    privacyUI.Close();
  }
}

function displayPrivacyInfo()
{
  var p3pService = Components.classes["@mozilla.org/p3p-service;1"].getService();
  if (p3pService) {
    p3pService = p3pService.QueryInterface(Components.interfaces.nsIP3PService);

    if (p3pService) {
      var isEnabled = p3pService.P3PIsEnabled();
      if (isEnabled) {
        if (privacyUI) {
          var status = privacyUI.GetPrivacyStatus();

          if (status == 1) {
            window.openDialog("chrome://communicator/content/P3PNoPrivacyInfo.xul","","modal=no,chrome,resizable=yes",
                              privacyUI);
          } else if (status == 2) {
            window.openDialog("chrome://communicator/content/P3PPrivacyMetInfo.xul","","modal=no,chrome,resizable=yes",
                              privacyUI);
          } else if (status == 3) {
            window.openDialog("chrome://communicator/content/P3PPrivacyNotMetInfo.xul","","modal=no,chrome,resizable=yes",
                              privacyUI);
          } else if (status == 4) {
            window.openDialog("chrome://communicator/content/P3PPartialPrivacyInfo.xul","","modal=no,chrome,resizable=yes",
                              privacyUI);
          } else if (status == 5) {
            window.openDialog("chrome://communicator/content/P3PBadPrivacyInfo.xul","","modal=no,chrome,resizable=yes",
                              privacyUI);
          } else if (status == 6) {
            window.openDialog("chrome://communicator/content/P3PInProgressInfo.xul","","modal=no,chrome,resizable=yes",
                              privacyUI);
          }
        }
      }
    }
  }
}

function SelectTipText() {
  var result = false;

  var text = document.getElementById("tiptext");
  if (text) {
    var statustext = document.getElementById("statustext");
    if (statustext) {
      var status = privacyUI.GetPrivacyStatus();

      if (status == 1) {
        text.setAttribute("value", statustext.getAttribute("none"));
      } else if (status == 2) {
        text.setAttribute("value", statustext.getAttribute("met"));
      } else if (status == 3) {
        text.setAttribute("value", statustext.getAttribute("notmet"));
      } else if (status == 4) {
        text.setAttribute("value", statustext.getAttribute("broken"));
      } else if (status == 5) {
        text.setAttribute("value", statustext.getAttribute("inprog"));
      }
      result = true;
    }
  }

  return result;
}
