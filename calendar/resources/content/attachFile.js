/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 */

/**
 * In order to distinguish clearly globals that are initialized once when js load (static globals) and those that need to be 
 * initialize every time a compose window open (globals), I (ducarroz) have decided to prefix by s... the static one and
 * by g... the other one. Please try to continue and repect this rule in the future. Thanks.
 */
/**
 * static globals, need to be initialized only once
 */
var sPrefs = null;
var sPrefBranchInternal = null;

const kComposeAttachDirPrefName = "mail.compose.attach.dir";

// First get the preferences service
try {
 var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefService);
 sPrefs = prefService.getBranch(null);
 sPrefBranchInternal = sPrefs.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
}
catch (ex) {
 dump("failed to preferences services\n");
}

function GetLastAttachDirectory()
{
  var lastDirectory;

  try {
    lastDirectory = sPrefs.getComplexValue(kComposeAttachDirPrefName, Components.interfaces.nsILocalFile);
  }
  catch (ex) {
    // this will fail the first time we attach a file
    // as we won't have a pref value.
    lastDirectory = null;
  }

  return lastDirectory;
}

// attachedLocalFile must be a nsILocalFile
function SetLastAttachDirectory(attachedLocalFile)
{
  try {
    var file = attachedLocalFile.QueryInterface(Components.interfaces.nsIFile);
    var parent = file.parent.QueryInterface(Components.interfaces.nsILocalFile);
    
    sPrefs.setComplexValue(kComposeAttachDirPrefName, Components.interfaces.nsILocalFile, parent);
  }
  catch (ex) {
    dump("error: SetLastAttachDirectory failed: " + ex + "\n");
  }
}

function AttachFile()
{
  var currentAttachment = "";
  
  //Get file using nsIFilePicker and convert to URL
  try {
      var nsIFilePicker = Components.interfaces.nsIFilePicker;

      var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
      fp.init(window, "Attach File", nsIFilePicker.modeOpen);
      
      var lastDirectory = GetLastAttachDirectory();
      if (lastDirectory) 
        fp.displayDirectory = lastDirectory;

      fp.appendFilters(nsIFilePicker.filterAll);
      
      if (fp.show() == nsIFilePicker.returnOK) {
         currentAttachment = fp.fileURL.spec;
         SetLastAttachDirectory(fp.file)
      }
  }
  catch (ex) {
    dump("failed to get the local file to attach\n");
  }
  
  if (currentAttachment == "")
    return;

  if (DuplicateFileCheck(currentAttachment))
  {
    dump("Error, attaching the same item twice\n");
  }
  else
  {
    var attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"]
                     .createInstance(Components.interfaces.nsIMsgAttachment);
    attachment.url = currentAttachment;
    AddAttachment(attachment);
  }
}

function AddAttachment(attachment)
{
  if (attachment && attachment.url)
  {
    var bucket = document.getElementById("attachmentBucket");
    var item = document.createElement("listitem");

    if (!attachment.name)
      attachment.name = attachment.url;

    item.setAttribute("label", attachment.name);    //use for display only
    item.attachment = attachment;   //full attachment object stored here
    try {
      item.setAttribute("tooltiptext", unescape(attachment.url));
    } catch(e) {
      item.setAttribute("tooltiptext", attachment.url);
    }
    item.setAttribute("class", "listitem-iconic");
    item.setAttribute("image", "moz-icon:" + attachment.url);
    bucket.appendChild(item);
  }
}

function DuplicateFileCheck(FileUrl)
{
  var bucket = document.getElementById('attachmentBucket');
  for (var index = 0; index < bucket.childNodes.length; index++)
  {
    var item = bucket.childNodes[index];
    var attachment = item.attachment;
    if (attachment)
    {
      if (FileUrl == attachment.url)
         return true;
    }
  }

  return false;
}

function RemoveSelectedAttachment()
{
  var child;
  var bucket = document.getElementById("attachmentBucket");
  if (bucket.selectedItems.length > 0) {
    for (var item = bucket.selectedItems.length - 1; item >= 0; item-- )
    {
      child = bucket.removeChild(bucket.selectedItems[item]) = null;
      // Let's release the attachment object hold by the node else it won't go away until the window is destroyed
      child.attachment = null;
    }
  }
}

function AttachmentBucketClicked(event)
{
  if (event.button != 0)
    return;

  if (event.originalTarget.localName == "listboxbody")
  {
     AttachFile();
  }
}

var attachmentBucketObserver = {

  canHandleMultipleItems: true,

  onDrop: function (aEvent, aData, aDragSession)
    {
      var dataList = aData.dataList;
      var dataListLength = dataList.length;
      var errorTitle;
      var attachment;
      var errorMsg;

      for (var i = 0; i < dataListLength; i++) 
      {
        var item = dataList[i].first;
        var prettyName;
        var rawData = item.data;
        
        if (item.flavour.contentType == "text/x-moz-url" ||
            item.flavour.contentType == "text/x-moz-message-or-folder" ||
            item.flavour.contentType == "application/x-moz-file")
        {
          if (item.flavour.contentType == "application/x-moz-file")
          {
            var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
            rawData = ioService.getURLSpecFromFile(rawData);
          }
          else
          {
            var separator = rawData.indexOf("\n");
            if (separator != -1) 
            {
              prettyName = rawData.substr(separator+1);
              rawData = rawData.substr(0,separator);
            }
          }

          if (DuplicateFileCheck(rawData)) 
          {
            dump("Error, attaching the same item twice\n");
          }
          else 
          {
            attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"]
                         .createInstance(Components.interfaces.nsIMsgAttachment);
            attachment.url = rawData;
            attachment.name = prettyName;
            AddAttachment(attachment);
          }
        }
      }
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      var attachmentBucket = document.getElementById("attachmentBucket");
      attachmentBucket.setAttribute("dragover", "true");
    },

  onDragExit: function (aEvent, aDragSession)
    {
      var attachmentBucket = document.getElementById("attachmentBucket");
      attachmentBucket.removeAttribute("dragover");
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/x-moz-message-or-folder");
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      return flavourSet;
    }
};

