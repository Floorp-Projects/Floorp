/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Mail Code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

function onLoad()
{ 
  var feedLocationEl = document.getElementById('feedLocation');
  var rssAccountMenuItem = document.getElementById('rssAccountMenuItem');

  if (window.arguments[0].feedLocation)
    feedLocationEl.value = window.arguments[0].feedLocation;  

  // root the location picker to the news & blogs server
  document.getElementById('selectFolder').setAttribute('ref', window.arguments[0].serverURI);

  SetFolderPicker(window.arguments[0].folderURI ? window.arguments[0].folderURI : window.arguments[0].serverURI, 'selectFolder');
  document.getElementById('selectFolder').setInitialSelection();

  rssAccountMenuItem.label = window.arguments[0].serverPrettyName;
  rssAccountMenuItem.value = window.arguments[0].serverURI;

  // set quick mode value
  document.getElementById('quickMode').checked = window.arguments[0].quickMode;

  if (!window.arguments[0].newFeed)
  {
    // if we are editing an existing feed, disable the top level account
    rssAccountMenuItem.setAttribute('disabled', 'true');
    feedLocationEl.setAttribute('readonly', true);
  }
}

function onOk()
{
  var feedLocation = document.getElementById('feedLocation').value;
  // trim leading and trailing white space from the url
  feedLocation = feedLocation.replace( /^\s+/, "").replace( /\s+$/, ""); 

  window.arguments[0].feedLocation = feedLocation;
  window.arguments[0].folderURI = document.getElementById('selectFolder').getAttribute("uri");; 
  window.arguments[0].quickMode = document.getElementById('quickMode').checked;
  window.arguments[0].result = true;

  return true;
}

function PickedMsgFolder(selection,pickerID)
{
  var selectedUri = selection.getAttribute('id');
  SetFolderPicker(selectedUri,pickerID);
}   

function SetFolderPicker(uri,pickerID)
{
  var picker = document.getElementById(pickerID);
  var msgfolder = GetMsgFolderFromUri(uri, true);
  if (!msgfolder) 
    return;

  picker.setAttribute("label",msgfolder.name);
  picker.setAttribute("uri",uri);
}

// CopyWebsiteAddress takes the website address title button, extracts
// the website address we stored in there and copies it to the clipboard
function CopyWebsiteAddress(websiteAddressNode)
{
  if (websiteAddressNode)
  {
    var websiteAddress = websiteAddressNode.value;
    var contractid = "@mozilla.org/widget/clipboardhelper;1";
    var iid = Components.interfaces.nsIClipboardHelper;
    var clipboard = Components.classes[contractid].getService(iid);
    clipboard.copyString(websiteAddress);
  }
}
