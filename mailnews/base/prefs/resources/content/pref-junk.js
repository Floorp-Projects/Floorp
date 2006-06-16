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
 * The Original Code is SeaMonkey Internet Suite code.
 *
 * The Initial Developer of the Original Code is Karsten Düsterloh <mnyromyr@tprac.de>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@mozilla.org> (original Thunderbird parts)
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

function Startup()
{
  UpdateDependentElement(document.getElementById("manualMark"), "manualMarkMode");
  UpdateDependentElement(document.getElementById("enableJunkLogging"), "openJunkLog");
}

function UpdateDependentElement(aBaseElement, aDependentElementId)
{
  var element = document.getElementById(aDependentElementId);
  var isLocked = parent.hPrefWindow.getPrefIsLocked(element.getAttribute("prefstring"));
  element.disabled = !aBaseElement.checked || isLocked;
}

function OpenJunkLog()
{
  window.openDialog("chrome://messenger/content/junkLog.xul",
                    "junkLog",
                    "chrome,modal,titlebar,resizable,centerscreen");
}

function ResetTrainingData()
{
  // make sure the user really wants to do this
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);
  var bundle = document.getElementById("bundlePreferences");
  var title  = bundle.getString("confirmResetJunkTrainingTitle");
  var text   = bundle.getString("confirmResetJunkTrainingText");

  // if the user says no, then just fall out
  if (promptService.confirmEx(window, title, text,
                              promptService.STD_YES_NO_BUTTONS |
                              promptService.BUTTON_POS_1_DEFAULT,
                              "", "", "", null, {}))
    return;

  // otherwise go ahead and remove the training data
  var junkmailPlugin = Components.classes["@mozilla.org/messenger/filter-plugin;1?name=bayesianfilter"]
                                 .getService(Components.interfaces.nsIJunkMailPlugin);

  if (junkmailPlugin)
    junkmailPlugin.resetTrainingData();
}
