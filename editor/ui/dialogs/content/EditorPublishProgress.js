/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Charles Manske (cmanske@netscape.com)
 */

var gInProgress = true;
var gPublishData;
var gPersistObj;
var gTotalFileCount = 0;
var gSucceededCount = 0;
var gFinishedCount = 0;
var gFinished = false;
var gFinalMessage = GetString("PublishFailed");
var gTimerID;

function Startup()
{
  if (!InitEditorShell())
    return;

  gPublishData = window.arguments[0];
  if (!gPublishData)
  {
    dump("No publish data!\n");
    window.close();
    return;
  }

  gDialog.FileList          = document.getElementById("FileList");
  gDialog.StatusMessage     = document.getElementById("StatusMessage");
  gDialog.KeepOpen          = document.getElementById("KeepOpen");
  gDialog.Close             = document.documentElement.getButton("cancel");

  SetWindowLocation();
  window.title = GetString("PublishProgressCaption").replace(/%title%/, editorShell.GetDocumentTitle());

  document.getElementById("PublishToSite").value = 
    GetString("PublishToSite").replace(/%title%/, TruncateStringAtWordEnd(gPublishData.siteName, 25)); 

  // Show publishing destination URL
  document.getElementById("PublishUrl").value = gPublishData.publishUrl;
  
  // Show subdirectories only if not empty
  if (gPublishData.docDir || gPublishData.otherDir)
  {
    if (gPublishData.docDir)
      document.getElementById("docDir").value = gPublishData.docDir;
    else
      document.getElementById("DocSubdir").setAttribute("hidden", "true");
      
    if (gPublishData.publishOtherFiles && gPublishData.otherDir)
      document.getElementById("otherDir").value = gPublishData.otherDir;
    else
      document.getElementById("OtherSubdir").setAttribute("hidden", "true");
  }
  else
    document.getElementById("Subdirectories").setAttribute("hidden", "true");

  // Add the document to the "publish to" list as quick as possible!
  SetProgressStatus(gPublishData.filename, "busy");

  if (gPublishData.publishOtherFiles)
  {
    // When publishing images as well, expand list to show more items
    gDialog.FileList.setAttribute("rows", 5);
    window.sizeToContent();
  }

  // Now that dialog is initialized, we can start publishing
  window.opener.StartPublishing();
}

// this function is to be used when we cancel persist's saving
// since not all messages will be returned to us if we cancel
// this function changes status for all non-done/non-failure to failure
function SetProgressStatusCancel()
{
  var listitems = document.getElementsByTagName("listitem");
  if (!listitems)
    return;

  for (var i=0; i < listitems.length; i++)
  {
    var attr = listitems[i].getAttribute("progress");
    if (attr != "done" && attr != "failed")
      listitems[i].setAttribute("progress", "failed");
  }
}

// Add filename to list of files to publish
// or set status for file already in the list
// Returns true if file was in the list
function SetProgressStatus(filename, status)
{
  if (!filename)
    return false;

  if (!status)
    status = "busy";

  // Just set attribute for status icon 
  // if we already have this filename 
  var listitems = document.getElementsByTagName("listitem");
  if (listitems)
  {
    for (var i=0; i < listitems.length; i++)
    {
      if (listitems[i].getAttribute("label") == filename)
      {
        listitems[i].setAttribute("progress", status);
        return true;
      }
    }
  }
  // We're adding a new file item to list
  gTotalFileCount++;

  var listitem = document.createElementNS(XUL_NS, "listitem");
  if (listitem)
  {
    listitem.setAttribute("class", "listitem-iconic progressitem");
    // This triggers CSS to show icon for each status state
    listitem.setAttribute("progress", status);
    listitem.setAttribute("label", filename);
    gDialog.FileList.appendChild(listitem);
  }
  return false;
}

function SetProgressFinished(filename, networkStatus)
{
  if (filename)
  {
    var status = networkStatus ? "failed" : "done";
    if (networkStatus == 0)
      gSucceededCount++;

    if (SetProgressStatus(filename, status))
      gFinishedCount++;
  }

  if (networkStatus != 0)
  {
    // XXX Interpret networkStatus and call SetStatusMessage() with 
    //  appropriate error description.
    if (filename == gPublishData.filename)
      gFinalMessage = GetString("PublishFailed");
    else
      gFinalMessage = GetString("PublishSomeFileFailed");
  }
  else if (gFinishedCount == gTotalFileCount || !filename)
  {
    gFinished = true;
    gDialog.Close.setAttribute("label", GetString("Close"));
    gFinalMessage = GetString("PublishCompleted");
  }
  SetStatusMessage(gFinalMessage);
}

function SetStatusMessage(message)
{
  gDialog.StatusMessage.value = message;
  window.sizeToContent();
}

function CheckKeepOpen()
{
  if (gTimerID)
  {
    clearTimeout(gTimerID);
    gTimerID = null;
  }
}

function onClose()
{
  if (gTimerID)
  {
    clearTimeout(gTimerID);
    gTimerID = null;
  }

  if (!gFinished && gPersistObj)
  {
    try {
      gPersistObj.cancelSave();
    } catch (e) {}
  }
  SaveWindowLocation();

  // Tell caller so they can cleanup and restore editability
  window.opener.FinishPublishing();
  return true;
}

function RequestCloseDialog()
{
  if (gFinished && !gDialog.KeepOpen.checked)
  {
    // Leave window open a minimum amount of time 
    gTimerID = setTimeout("CloseDialog();", 3000);
  }
  // If window remains open, be sure final message is set
  SetStatusMessage(gFinalMessage);
}

function CloseDialog()
{
  SaveWindowLocation();
  window.opener.FinishPublishing();
  try {
    window.close();
  } catch (e) {}
}