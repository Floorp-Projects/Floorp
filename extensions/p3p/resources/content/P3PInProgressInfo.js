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

var privacyUI;

function Init() {
  privacyUI = window.arguments[0];

  var element = document.getElementById( "currentURI" );
  if (element) {
    element.setAttribute("value", privacyUI.GetCurrentURI());
  }
}

function doRefreshButton() {
  var privacyStatus = privacyUI.GetPrivacyStatus( );
  if (privacyStatus != 6) {
    if (privacyStatus == 1) {
      window.openDialog("chrome://communicator/content/P3PNoPrivacyInfo.xul","","modal=no,chrome,resizable=yes",
                        privacyUI);

    } else if (privacyStatus == 2) {
      window.openDialog("chrome://communicator/content/P3PPrivacyMetInfo.xul","","modal=no,chrome,resizable=yes",
                        privacyUI);

    } else if (privacyStatus == 3) {
      window.openDialog("chrome://communicator/content/P3PPrivacyNotMetInfo.xul","","modal=no,chrome,resizable=yes",
                        privacyUI);

    } else if (privacyStatus == 4) {
      window.openDialog("chrome://communicator/content/P3PPartialPrivacyInfo.xul","","modal=no,chrome,resizable=yes",
                        privacyUI);

    } else if (privacyStatus == 5) {
      window.openDialog("chrome://communicator/content/P3PBadPrivacyInfo.xul","","modal=no,chrome,resizable=yes",
                        privacyUI);
    }
    doCancelButton();
  }
}
