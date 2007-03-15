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

const HORIZONTAL = 1;
const LEFT = 2;
const TOP = 4;
const MSG_FOLDER_FLAG_VIRTUAL = 0x0020;

var gSlideTime = 50;
var gNumNewMsgsToShowInAlert = 4; // the more messages we show in the alert, the larger it will be
var gOpenTime = 3000; // total time the alert should stay up once we are done animating.
var gAlertListener = null;
var gPendingPreviewFetchRequests = 0;
var gUserInitiated = false;
var gFadeIncrement = .05;
var gOrigin = 0;

function prefillAlertInfo()
{
  // unwrap all the args....
  // arguments[0] --> array of folders with new mail
  // arguments[1] --> the observer to call back with notifications about the alert
  // arguments[2] --> user initiated boolean. true if the user initiated opening the alert 
  //                 (which means skip the fade effect and don't auto close the alert)
  // arguments[3] --> the alert origin returned by the look and feel
  var foldersWithNewMail = window.arguments[0];  
  gAlertListener = window.arguments[1];
  gUserInitiated = window.arguments[2];
  gOrigin = window.arguments[3];

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
  folderSummaryInfoEl.mMaxMsgHdrsInPopup = gNumNewMsgsToShowInAlert;
  for (var folderIndex = 0; folderIndex < numFolders; folderIndex++)
  {
    var folder = allFolders.GetElementAt(folderIndex).QueryInterface(Components.interfaces.nsIMsgFolder);
    if (folder.hasNewMessages && !(folder.flags & MSG_FOLDER_FLAG_VIRTUAL))
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
      showAlert();
  }
}

function onAlertLoad()
{
  // read out our initial settings from prefs.
  try 
  {
    var prefBranch = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService).getBranch(null);
    gSlideTime = prefBranch.getIntPref("alerts.slideIncrementTime");
    gOpenTime = prefBranch.getIntPref("alerts.totalOpenTime");
  } catch (ex) {}
  
  // bogus call to make sure the window is moved offscreen until we are ready for it.
  resizeAlert(true);

  // if we aren't waiting to fetch preview text, then go ahead and 
  // start showing the alert.
  if (!gPendingPreviewFetchRequests)
    setTimeout(showAlert, 0); // let the JS thread unwind, to give layout 
                              // a chance to recompute the styles and widths for our alert text.
}

// If the user initiated the alert, show it right away, otherwise start opening the alert with
// the fade effect. 
function showAlert()
{
  if (!gUserInitiated) // set the initial opacity before we resize the window
    document.getElementById('alertContainer').style.opacity = 0;
  
  // resize the alert based on our current content  
  resizeAlert(false);
  
  if (document.getElementById('folderSummaryInfo').hasMessages)
  {
    if (!gUserInitiated) // don't fade in if the user opened the alert
      setTimeout(fadeOpen, gSlideTime);
  }
  else
    closeAlert(); // no mail, so don't bother showing the alert...
}

function resizeAlert(aMoveOffScreen)
{
  // sizeToContent is not working. It isn't honoring the max widths we are attaching to our inner
  // objects like the folder summary element. While the folder summary element is cropping, 
  // sizeToContent ends up thinking the window needs to be much wider than it should be. 
  // use resizeTo and make up our measurements...
  //sizeToContent();
  
  // Use the wider of the alert groove and the folderSummaryInfo box, then 
  // add on the width of alertImageBox + some small amount of fudge. For the height, 
  // just use the size of the alertBox, that appears to be pretty accurate.
  var windowWidth = Math.max (document.getBoxObjectFor(document.getElementById('alertGroove')).width,
                              document.getBoxObjectFor(document.getElementById('folderSummaryInfo')).width);
  resizeTo(windowWidth + document.getBoxObjectFor(document.getElementById('alertImageBox')).width + 30, 
           document.getBoxObjectFor(document.getElementById('alertBox')).height + 10);                     
  
  // leftover hack to get the window properly hidden when we first open it
  if (aMoveOffScreen)
    window.outerHeight = 1;

  // Determine position and move window
  var x = gOrigin & LEFT ? screen.availLeft :
          (screen.availLeft + screen.availWidth - window.outerWidth);
  var y = gOrigin & TOP ? screen.availTop :
          (screen.availTop + screen.availHeight - window.outerHeight);
  window.moveTo(x, y);
}

function fadeOpen()
{
  var alertContainer = document.getElementById('alertContainer');
  var newOpacity = parseFloat(window.getComputedStyle(alertContainer, "").opacity) + gFadeIncrement;
  alertContainer.style.opacity = newOpacity;
  
  if (newOpacity < 1.0)    
    setTimeout(fadeOpen, gSlideTime);
  else // switch gears and start closing the alert
    setTimeout(fadeClose, gOpenTime);  
}

function fadeClose()
{
  var alertContainer = document.getElementById('alertContainer');
  var newOpacity = parseFloat(window.getComputedStyle(alertContainer, "").opacity) - gFadeIncrement;
  alertContainer.style.opacity = newOpacity;
  
  if (newOpacity <= 0)
    closeAlert();
  else
    setTimeout(fadeClose, gSlideTime);
}

function closeAlert()
{
  if (gAlertListener)
    gAlertListener.observe(null, "alertfinished", ""); 
  window.close(); 
}
