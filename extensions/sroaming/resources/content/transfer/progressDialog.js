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
 *   Netscape Editor team
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

/* Displays the progress of the down/upload of profile files to an
   HTTP/FTP server.
   It also initiates the actual transfer, using transfer.js,
   see mozSRoamingStream.cpp for the reasons.

   The Set*() part of this file needs a serious shakeout.
*/

var gTransfer; // main |Transfer| object, for the main files to be tranferred
var gDialog = new Object(); // references to widgets
var gFinished = false;  // all files finished (done or failed)
var gTransferFailed = false; // any file failed
var gResults = ""; /* stores the transfer result messages (in human language)
                      to be later displayed to the user on his request. */
var gTimerID;
var gTimeout = 1000;
var gAllowEnterKey = false;

function Startup()
{
  //ddump("In sroaming/transfer/progressDialog::Startup()");

  gDialog.FileList           = document.getElementById("FileList");
  gDialog.StatusMessage      = document.getElementById("StatusMessage");
  gDialog.ListingProgress    = document.getElementById("ListingProgress");
  gDialog.Close              = document.documentElement.getButton("cancel");

  try
  {
    GetParams(); // dialog params -> gTransfer
    checkAndTransfer(gTransfer, null); // half-blocking
  }
  catch (e)
  {
    /* All kinds of exceptions should end up here, esp. from init, meaning
       this is the main last backstop for unexpected or fatal errors,
       esp. those not related to a certain file. */
    GlobalError(ErrorMessageForException(e));
    return;
  }

  window.title = GetString("TransferProgressCaption");

  var directionString = gTransfer.download
                        ? GetString("TransferFromSite")
                        : GetString("TransferToSite");
  document.getElementById("TransferToFromSite").value = 
                      directionString.replace(/%site%/, gTransfer.sitename);

  // Show transferring destination URL
  document.getElementById("TransferUrl").value = gTransfer.remoteDir;

  // Add the files to the UI as quickly as possible
  gDialog.FileList.setAttribute("rows", gTransfer.files.length);
  for (var i = 0; i < gTransfer.files.length; i++)
    SetProgressStatus(i);
  window.sizeToContent();
}

/*
  Reads in the params we got from the calling function. Creates gTransfer.
*/
function GetParams()
{
  var params = window.arguments[0].QueryInterface(
                                   Components.interfaces.nsIDialogParamBlock);
  /* For definition of meaning of params, see
     mozSRoamingStream.cpp::DownUpLoad() */

  // download
  var direction = params.GetInt(0);
  //ddump("Passing in: Int 0 (direction) is " + direction);
  if (direction != 1 && direction != 2)
    throw "Error: Bad direction param";
  var download = direction == 1;

  // serial/parallel
  var serial_param = params.GetInt(1);
  //ddump("Passing in: Int 1 (serial) is " + serial_param);
  if (serial_param != 1 && serial_param != 2)
    throw "Error: Bad serial param";
  var serial = serial_param == 1;
  serial = true;

  // files count
  var count = params.GetInt(2);
  //ddump("Passing in: Int 2 (files count) is " + count);
  if (count == 0)
    throw GetString("NoFilesSelected");
  if (count < 0)
    throw "Error: Bad count param";

  // save pw
  var savepw_param = params.GetInt(3);
  //ddump("Passing in: Int 3 (save pw) is " + savepw_param);
  var savepw = false;
  if (savepw_param == 1)
    savepw = true;

  // profile dir
  var profileDir = params.GetString(1);
  //ddump("Passing in: String 1 (profile dir) is " + profileDir);
  if (profileDir == null || profileDir == "")
    throw "Error: Bad profile dir param";

  // remote dir
  var remoteDir = params.GetString(2);
  //ddump("Passing in: String 2 (remote dir) is " + remoteDir);
  if (remoteDir == null || remoteDir == "")
    throw "Error: Bad remote dir param";

  // password
  var password = params.GetString(3);
  //ddump("Passing in: String 3 (password) is " + password);
  // ignore errors

  // filenames
  var files = new Array();
  for (var i = 0; i < count; i++)
  {
    var filename = params.GetString(i + 4);  // filenames start at item 4
    //ddumpCont("Passing in: String " + (i + 4));
    //ddump(" (file " + i + ") is " + filename);
    if (filename == null || filename == "")
      throw "Error: Bad filename";
    files[i] = new Object();
    files[i].filename = filename;
    files[i].mimetype = undefined;
    files[i].size = undefined;
  }

  gTransfer = new Transfer(download, serial,
                           profileDir, remoteDir,
                           password, savepw,
                           files,
                           undefined, SetProgressStatus);
}

function PassBackParams()
{
  //ddump("PassBackParam()");
  var params = window.arguments[0].QueryInterface(
                                    Components.interfaces.nsIDialogParamBlock);
  if (gTransfer)
  {
    params.SetInt(0, gTransfer.getSaveLogin());
    //ddump(" int0: " + gTransfer.getSaveLogin());
    if (gTransfer.getSaveLogin() == 1)
    {
      params.SetString(0, gTransfer.getUsername());
      params.SetString(1, gTransfer.getPassword());
      //ddump(" string0: " + gTransfer.getUsername());
      //ddump(" string1: " + gTransfer.getPassword());
    }
  }
  else /* e.g. if we didn't get good prefs and couldn't create a transfer
          in the first place */
  {
    params.SetInt(0, 0);
    params.SetString(0, "");
    params.SetString(1, "");
  }
  //ddump(" done");
}



/* Add filename to list of files to transfer
   or set status for file already in the list.

   @param filenr integer  index of file in gTransfer
   @return boolean  if file was in the list
*/
function SetProgressStatus(filenr)
{
  //ddumpCont("SetProgressStatus(" + filenr + "): ");
  if (!gTransfer || !gTransfer.files[filenr])
  {
    dumpError("no such file");
    return false;
  }
  var file = gTransfer.files[filenr];
  var filename = file.filename;
  var status = file.status;
  //ddump(filename + ", " + status + ", " + NameForStatusCode(file.statusCode));

  if (status == "busy")
    SetStatusMessage(GetString("Transferring"));

  // update file listbox
  // if we already have this file's item, just set attribute for status icon
  found = false;
  var listitems = document.getElementsByTagName("listitem");
  for (var i = 0, l = listitems.length; i < l; i++)
  {
    var li = listitems[i];
    if (li.getAttribute("filename") == filename)
    {
      if (li.getAttribute("progress") != status)
      {
        var oldstat = li.getAttribute("progress");
        //ddump("  Setting "+filename+" from "+oldstat+" to "+status); 
        li.setAttribute("progress", status);
      }
      found = true;
    }
  }
  // otherwise, add the item
  if (!found)
  {
    var listitem = document.createElement("listitem");
    listitem.setAttribute("class", "listitem-iconic progressitem");
    // The progress attribute triggers CSS to show icon for each status state
    listitem.setAttribute("progress", status);
    listitem.setAttribute("filename", filename); // bookkeeping
    listitem.setAttribute("label", GetFileDescription(filename));
    gDialog.FileList.appendChild(listitem);
  }

  // CheckDone(false); -- already called from conflictCheck.js

  return found;
}

function SetProgressFinishedAll()
{
  SetStatusMessage(GetString(gTransferFailed
                             ? "TransferFailed"
                             : "TransferCompleted"));

  for (var i = 0, l = gTransfer.files.length; i < l; i++)
    addFileStatus(gTransfer.files[i]);

  if (gTransferFailed)
  {
    /* Show "Troubleshooting" button to help solving problems
       and legend for successful / failed icons */
    document.getElementById("failureBox").hidden = false;
    window.sizeToContent();
  }

  gDialog.Close.setAttribute("label", GetString("Close"));
  // Now allow "Enter/Return" key to close the dialog
  gDialog.Close.setAttribute("default","true");
  gAllowEnterKey = true;
}

/*
  checks, if all files finished (failed or success) and change dialog, if so
  @param close  bool  If all files finished successfully, also close dialog
 */
function CheckDone(close)
{
  //ddump("CheckDone()");
  for (var i = 0; i < gTransfer.files.length; i++)
  {
    var file = gTransfer.files[i];
    //ddumpCont("  Checking " + i + ", " + file.filename + ", ");
    //ddump(file.status);
    if (file.status == "failed")
      gTransferFailed = true;
    else if (file.status != "done")
      return;
  }
  //ddump("  Yes, we're done");
  gFinished = true;

  // Finish progress messages, settings buttons etc.
  SetProgressFinishedAll();

  if (!close || gTransferFailed)
    return;
  //ddump("  Closing");

  if (gTimeout > 0)
    // Leave window open a minimum amount of time 
    gTimerID = setTimeout(CloseDialog, gTimeout);
  else
    CloseDialog();
}


// Close stuff

function onClose()
{
  CleanUpDialog();
  return true;
}

// called, if the user presses Cancel in the conflict resolve dialog
function onCancel()
{
  CloseDialog();
}

function onEnterKey()
{
  if (gAllowEnterKey)
    return CloseDialog();

  return false;
}

function CloseDialog()
{
  CleanUpDialog();
  try {
    window.close();
  } catch (e) {}
}

function CleanUpDialog()
{
  if (gTimerID)
  {
    clearTimeout(gTimerID);
    gTimerID = null;
  }
  if (!gFinished && gTransfer)
  {
    gTransfer.cancel();
  }
  PassBackParams();
}

// UI stuff

// Sets the text in the prominent center of the dialog
function SetStatusMessage(message)
{
  if (!message)
    message = "";
  if (gTransferFailed)
  	return;
  gDialog.StatusMessage.firstChild.nodeValue = message;
}

// For fatal errors like unexpected exceptions. Bail and go home.
function GlobalError(message)
{
  GetPromptService().alert(window, GetString("FatalError"),
                           message);
  CloseDialog();
}

/*
  Records the transfer result of a file, to be alter displayed to the user
  on request.
  @param file  TransferFile
 */
function addFileStatus(file)
{
  gResults += GetFileDescription(file.filename)
              + ": " + ErrorMessageForFile(file) + "\n";
}

// replace with nicer dialog
function showErrors()
{
  GetPromptService().alert(window, GetString("TransferErrorsTitle"),
                           gResults);
}

// set to on, if a listing.xml file is being transferred (and off afterwards)
function SetListingTransfer(on)
{
  gDialog.ListingProgress.setAttribute("hidden", on ? "false" : "true");
  //gDialog.ListingProgress.setAttribute("value", on ? "1" : "0");
  SetStatusMessage(on ? GetString("TransferringListing") : null);
}
