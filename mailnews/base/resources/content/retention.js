/*
 * -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Bienvenu <bienvenu@mozilla.org>
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
 * ***** END LICENSE BLOCK ****** */


function initCommonRetentionSettings(retentionSettings)
{
  document.getElementById("retention.keepUnread").checked =  retentionSettings.keepUnreadMessagesOnly;
  document.getElementById("retention.keepMsg").value = retentionSettings.retainByPreference;
  if(retentionSettings.daysToKeepHdrs > 0)
    document.getElementById("retention.keepOldMsgMin").value = 
    (retentionSettings.daysToKeepHdrs > 0) ? retentionSettings.daysToKeepHdrs : 30;
  document.getElementById("retention.keepNewMsgMin").value = 
    (retentionSettings.numHeadersToKeep > 0) ? retentionSettings.numHeadersToKeep : 30;

  document.getElementById("retention.keepMsg").value = retentionSettings.retainByPreference;
}



function saveCommonRetentionSettings()
{
  var retentionSettings = new Object;

  retentionSettings.retainByPreference = document.getElementById("retention.keepMsg").value;

  retentionSettings.daysToKeepHdrs = document.getElementById("retention.keepOldMsgMin").value;
  retentionSettings.numHeadersToKeep = document.getElementById("retention.keepNewMsgMin").value;
  retentionSettings.keepUnreadMessagesOnly = document.getElementById("retention.keepUnread").checked;

  return retentionSettings;
}


function onCheckKeepMsg()
{
  if (gLockedPref && gLockedPref["retention.keepMsg"]) {
    // if the pref associated with the radiobutton is locked, as indicated
    // by the gLockedPref, skip this function.  All elements in this
    // radiogroup have been locked by the function onLockPreference.
    return;
  }

  var keepMsg = document.getElementById("retention.keepMsg").value;
  document.getElementById("retention.keepOldMsgMin").disabled = keepMsg != 2;
  document.getElementById("retention.keepNewMsgMin").disabled = keepMsg != 3;
}
