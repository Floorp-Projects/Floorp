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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *   Srilatha Moturi <srilatha@netscape.com>
 *   Rajiv Dayal <rdayal@netscape.com>
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

function mailnewsOverlayStartup()
{
  if (!("mapiInitialized" in parent))
  {
    parent.mapiDisabled = true;
    parent.newsDisabled = true;
    var mapiRegistry = getMapiRegistry();
    parent.mapiInitialized = !!mapiRegistry;
    if (mapiRegistry)
    {
      var prefWindow = parent.hPrefWindow;
      // initialise preference and registry components.
      // While the data is coming from the system registry, we use a set
      // of parallel preferences to indicate if the ui should be locked.
      parent.mapiPref = {};
      parent.mapiReg = {};

      // Only register callback if mapiRegistry exists so
      // we do not need to check its existence in onOK
      prefWindow.registerOKCallbackFunc(onOK);

      var isDefault;
      const kPrefBase = "system.windows.lock_ui.";
      if (prefWindow.getPrefIsLocked(kPrefBase + "defaultMailClient"))
      {
        isDefault = prefWindow.getPref("bool", kPrefBase + "defaultMailClient");
        mapiRegistry.isDefaultMailClient = isDefault;
        parent.mapiReg.isDefaultMailClient = isDefault;
      }
      else
      {
        parent.mapiReg.isDefaultMailClient = mapiRegistry.isDefaultMailClient;
        parent.mapiDisabled = false;
      }
      parent.mapiPref.isDefaultMailClient = parent.mapiReg.isDefaultMailClient;

      if (prefWindow.getPrefIsLocked(kPrefBase + "defaultNewsClient"))
      {
        isDefault = prefWindow.getPref("bool", kPrefBase + "defaultNewsClient");
        mapiRegistry.isDefaultNewsClient = isDefault;
        parent.mapiReg.isDefaultNewsClient = isDefault;
      }
      else
      {
        parent.mapiReg.isDefaultNewsClient = mapiRegistry.isDefaultNewsClient;
        parent.newsDisabled = false;
      }
      parent.mapiPref.isDefaultNewsClient = parent.mapiReg.isDefaultNewsClient;
    }
  }

  var mailnewsEnableMapi = document.getElementById("mailnewsEnableMapi");
  var mailnewsEnableNews = document.getElementById("mailnewsEnableNews");

  if (parent.mapiInitialized)
  {
    // when we switch between different panes set the checkbox based on the saved state
    mailnewsEnableMapi.checked = parent.mapiPref.isDefaultMailClient;
    mailnewsEnableNews.checked = parent.mapiPref.isDefaultNewsClient;
  }
  mailnewsEnableMapi.disabled = parent.mapiDisabled;
  mailnewsEnableNews.disabled = parent.newsDisabled;
}

function getMapiRegistry() {
  if ("@mozilla.org/mapiregistry;1" in Components.classes)
    return Components.classes["@mozilla.org/mapiregistry;1"]
                     .getService(Components.interfaces.nsIMapiRegistry);
  return null;
}

function onOK()
{
  var mapiRegistry = getMapiRegistry();
  if (parent.mapiReg.isDefaultMailClient != parent.mapiPref.isDefaultMailClient)
    mapiRegistry.isDefaultMailClient = parent.mapiPref.isDefaultMailClient;

  if (parent.mapiReg.isDefaultNewsClient != parent.mapiPref.isDefaultNewsClient)
    mapiRegistry.isDefaultNewsClient = parent.mapiPref.isDefaultNewsClient;
}
