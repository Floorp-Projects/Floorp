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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@mozilla.org>
 *   Jens Bannmann <jens.b@web.de>
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

var gFinalHeight = 60;
var gSlideIncrement = 1;
var gSlideTime = 10;
var gOpenTime = 3000; // total time the alert should stay up once we are done animating.
var gAlertCookie = "";
var gAlertListener = null;
var gPendingPreviewFetchRequests = 0;

function prefillAlertInfo()
{
  // unwrap all the args....
  // arguments[0] --> array of folders with new mail
  // arguments[1] --> the observer to call back with notifications about the alert
 
  gAlertListener = window.arguments[1];

  // walk the array of folders with new mail.
  var foldersWithNewMail = window.arguments[0];
  // for now just grab the first folder which should be a root folder
  // for the account that has new mail. 
  var rootFolder = foldersWithNewMail.GetElementAt(0).QueryInterface(Components.interfaces.nsIWeakReference).QueryReferent(Components.interfaces.nsIMsgFolder);

  // generate an account label string based on the root folder
  var label = document.getElementById('alertTitle');
  var totalNumNewMessages = rootFolder.getNumNewMessages(true);
  label.value = document.getElementById('bundle_messenger').getFormattedString(totalNumNewMessages == 1 ? "newBiffNotification_message" : "newBiffNotification_messages", 
                                                                                     [rootFolder.prettiestName, totalNumNewMessages]);

  // this is really the root folder and we have to walk through the list to find the real folder that has new mail in it...:(
  var allFolders = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  rootFolder.ListDescendents(allFolders);
  var numFolders = allFolders.Count();
  var folderSummaryInfoEl = document.getElementById('folderSummaryInfo');
  for (var folderIndex = 0; folderIndex < numFolders; folderIndex++)
  {
    var folder = allFolders.GetElementAt(folderIndex).QueryInterface(Components.interfaces.nsIMsgFolder);
    if (folder.hasNewMessages)
    {
      var asyncFetch = {};
      folderSummaryInfoEl.parseFolder(folder, new urlListener(folder), asyncFetch);
      if (asyncFetch.value)
        gPendingPreviewFetchRequests++;
    }
  }
  }

function urlListener(aFolder)
{
  this.mFolder = aFolder;
}

urlListener.prototype = {
  OnStartRunningUrl: function(aUrl)
  {
  },

  OnStopRunningUrl: function(aUrl, aExitCode)
  {
    var folderSummaryInfoEl = document.getElementById('folderSummaryInfo');
    var asyncFetch = {};
    folderSummaryInfoEl.parseFolder(this.mFolder, null, asyncFetch);
    gPendingPreviewFetchRequests--;

    // when we are done running all of our urls for fetching the preview text,
    // start the alert.
    if (!gPendingPreviewFetchRequests)
    {
      resizeAlert();
      setTimeout(animateAlert, gSlideTime);
    }
  },
}

function onAlertLoad()
{
  // read out our initial settings from prefs.
  try 
  {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService();
    prefService = prefService.QueryInterface(Components.interfaces.nsIPrefService);
    var prefBranch = prefService.getBranch(null);
    gSlideIncrement = prefBranch.getIntPref("alerts.slideIncrement");
    gSlideTime = prefBranch.getIntPref("alerts.slideIncrementTime");
    gOpenTime = prefBranch.getIntPref("alerts.totalOpenTime");
  } catch (ex) {}

  resizeAlert();

  // if we aren't waiting to fetch preview text, then go ahead and 
  // start showing the alert.
  if (!gPendingPreviewFetchRequests)
    setTimeout(animateAlert, gSlideTime);
}

function resizeAlert()
{
  sizeToContent();
  gFinalHeight = window.outerHeight;
  window.outerHeight = 1;

  // be sure to offset the alert by 10 pixels from the far right edge of the screen
  window.moveTo( (screen.availLeft + screen.availWidth - window.outerWidth) - 10, screen.availTop + screen.availHeight - window.outerHeight);
}

function animateAlert()
{
  if (window.outerHeight < gFinalHeight)
  {
    window.screenY -= gSlideIncrement;
    window.outerHeight += gSlideIncrement;
    setTimeout(animateAlert, gSlideTime);
  }
  else
    setTimeout(closeAlert, gOpenTime);  
}

function closeAlert()
{
  if (window.outerHeight > 1)
  {
    window.screenY += gSlideIncrement;
    window.outerHeight -= gSlideIncrement;
    setTimeout(closeAlert, gSlideTime);
  }
  else
  {
    if (gAlertListener)
      gAlertListener.observe(null, "alertfinished", gAlertCookie); 
    window.close(); 
  }
}

function onAlertClick()
{
  if (gAlertListener && gAlertTextClickable)
    gAlertListener.observe(null, "alertclickcallback", gAlertCookie);
}
