/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function onInit() 
{
  var accountName = document.getElementById("server.prettyName");
  var title = document.getElementById("am-main-title");
  var defaultTitle = title.getAttribute("defaultTitle");
  var titleValue;

  if(accountName.value)
    titleValue = defaultTitle+" - <"+accountName.value+">";
  else
    titleValue = defaultTitle;

  title.setAttribute("title",titleValue);
    
  setupSignatureItems();
}

function selectFile()
{
    var fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(nsIFilePicker);
  
    var prefutilitiesBundle = document.getElementById("bundle_prefutilities");
    var title = prefutilitiesBundle.getString("choosefile");
    fp.init(window, title, nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll);

    // Get current signature folder, if there is one.
    // We can set that to be the initial folder so that users 
    // can maintain their signatures better.
    var sigFolder = GetSigFolder();
    if (sigFolder)
        fp.displayDirectory = sigFolder;
  
    var ret = fp.show();
    if (ret == nsIFilePicker.returnOK) {
        var folderField = document.getElementById("identity.signature");
        folderField.value = fp.file.path;
    }
}

function GetSigFolder()
{
    var sigFolder = null;
    try {
        var result = parent.getServerIdAndPageIdFromTree(parent.accounttree);
        var account = parent.getAccountFromServerId(result.serverId);
        var identity = account.defaultIdentity;
        var signatureFile = identity.signature;

        if (signatureFile) {
            signatureFile = signatureFile.QueryInterface( Components.interfaces.nsILocalFile );
            sigFolder = signatureFile.parent;

            if (!sigFolder.exists) 
                sigFolder = null;
        }
    }
    catch (ex) {
        dump("failed to get signature folder..\n");
    }
    return sigFolder;
}

// If a signature is need to be attached, the associated items which
// displays the absolute path to the signature (in a textbox) and the way
// to select a new signature file (a button) are enabled. Otherwise, they
// are disabled. Check to see if the attachSignature is locked to block
// broadcasting events.
function setupSignatureItems()
{ 
   var signature = document.getElementById("identity.signature");
   var browse = document.getElementById("identity.sigbrowsebutton");

   var attachSignature = document.getElementById("identity.attachSignature");
   var checked = attachSignature.checked;

   if (checked && !getAccountValueIsLocked(signature))
     signature.removeAttribute("disabled");
   else
     signature.setAttribute("disabled", "true");

   if (checked && !getAccountValueIsLocked(browse))
     browse.removeAttribute("disabled");
   else
     browse.setAttribute("disabled", "true");
}

function editVCardCallback(escapedVCardStr)
{
  var escapedVCard = document.getElementById("identity.escapedVCard");
  escapedVCard.value = escapedVCardStr;
}

function editVCard()
{
  var escapedVCard = document.getElementById("identity.escapedVCard");

  // read vCard hidden value from UI
  window.openDialog("chrome://messenger/content/addressbook/abNewCardDialog.xul",
                    "",
                    "chrome,resizable=no,titlebar,modal",
                    {escapedVCardStr:escapedVCard.value, okCallback:editVCardCallback,
                     titleProperty:"editVCardTitle", hideABPicker:true});
}

