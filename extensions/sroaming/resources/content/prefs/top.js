/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
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
 * The Original Code is Mozilla Roaming code.
 *
 * The Initial Developer of the Original Code is 
 *   Ben Bucksch <http://www.bucksch.org> of
 *   Beonex <http://www.beonex.com>
 * Portions created by the Initial Developer are Copyright (C) 2002-2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* See all.js */

var _elementIDs = []; // no prefs (nsIPrefBranch) needed, see above
var addedHandler = false; /* Communicator's broken prefwindow
                   makes onload/onunload trigger every time the user
                   switches a pane (i.e. several times for the same pane).
                   This does not happen with the tabs in Firefox, and while
                   trying to make it work with both at once, I was almost
                   going insane. This onload/onunload/okHandler stuff here is
                   extremely fragile. This version happens to work, I don't
                   know why, but I can't see it anymore, after fighting
                   with it for maybe 2 days in Communicatior and slmost
                   another day in Firefox, only on this
                   onload/onunload/okHandler crap. */

function Load()
{
  parent.initPanel('chrome://sroaming/content/prefs/top.xul');

  // dataManager.pageData doesn't work, because it needs to work on both panes
  if (!parent.roaming)
    parent.roaming = new RoamingPrefs();
  DataToUI();

  if (parent != null && "firefox" in parent)
  {
    if (!parent.firefox.topLoad)
      parent.firefox.topLoad = Load;
    if (!parent.firefox.topUnload)
      parent.firefox.topUnload = Unload;
  }

  if (!addedHandler)
  {
    addedHandler = true;
    parent.hPrefWindow.registerOKCallbackFunc(function()
    {
      UIToData();
      parent.roaming.okClicked();
    });
  }
}


// UI

// write data to widgets
function DataToUI()
{
  var data = parent.roaming;
  E("enabled").checked = data.Enabled;
  E("methodSelect").value = data.Method;
  E("streamURL").value = data.Stream.BaseURL;
  E("streamUsername").value = data.Stream.Username;
  E("streamPassword").value = data.Stream.Password;
  E("streamSavePW").checked = data.Stream.SavePW;
  E("copyDir").value = data.Copy.RemoteDir;

  SwitchDeck(data.Method, E("methodSettingsDeck"));
  EnableTree(data.Enabled, E("method"));
  E("streamPassword").disabled = !data.Stream.SavePW || !data.Enabled;
}

// write widget content to data
function UIToData()
{
  var data = parent.roaming;
  data.Enabled = E("enabled").checked;
  data.Method = E("methodSelect").value;
  data.Stream.BaseURL = E("streamURL").value;
  data.Stream.Username = E("streamUsername").value;
  data.Stream.Password = E("streamPassword").value;
  data.Stream.SavePW = E("streamSavePW").checked;
  data.Copy.RemoteDir = E("copyDir").value;
  data.changed = true; // excessive
}

// updates UI (esp. en/disabling of widgets) to match new user input
function SwitchChanged()
{
  UIToData();
  DataToUI();
}

// Displays a general warning about roaming
function ActivationWarning()
{
  try
  {
    /* The pref here is intended not to exist (code logic falling back to
       false) when the user first activates roaming,
       then to be set, so that the user doesn't see the dialog twice.
       It being a pref also leaves admins the choice of supressing it,
       e.g. when they set up appropriate backups on behalf of the user on
       the server or client in a large installation. */
    var prefBranch = Components.classes["@mozilla.org/preferences;1"]
		       .getService(Components.interfaces.nsIPrefBranch);
    try
    {
      showWarning = prefBranch.getBoolPref("roaming.showInitialWarning");
      if (!showWarning)
        return;
    } catch(e) {}

    var bundle = document.getElementById("bundle_roaming_prefs");
    GetPromptService().alert(window, GetString("Alert"),
                             bundle.getString("ActivationWarning"));

    prefBranch.setBoolPref("roaming.showInitialWarning", false);
  } catch(e) {}
}
