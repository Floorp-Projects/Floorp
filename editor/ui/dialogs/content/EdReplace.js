/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Kin Blas <kin@netscape.com>
 * Contributor(s): Akkana Peck <akkana@netscape.com>
 *
 */

var gReplaceDialog;      // Quick access to document/form elements.
var gFindInst;           // nsIWebBrowserFind that we're going to use
var gFindService;        // Global service which remembers find params

function initDialogObject()
{
  // Create gReplaceDialog object and initialize.
  gReplaceDialog = new Object();
  gReplaceDialog.findKey         = document.getElementById("dialog.findKey");
  gReplaceDialog.replaceKey      = document.getElementById("dialog.replaceKey");
  gReplaceDialog.caseSensitive   = document.getElementById("dialog.caseSensitive");
  gReplaceDialog.wrap            = document.getElementById("dialog.wrap");
  gReplaceDialog.searchBackwards = document.getElementById("dialog.searchBackwards");
  gReplaceDialog.findNext        = document.getElementById("findNext");
  gReplaceDialog.replace         = document.getElementById("replace");
  gReplaceDialog.replaceAll      = document.getElementById("replaceAll");
  gReplaceDialog.bundle          = null;
  gReplaceDialog.editor          = null;
}

function loadDialog()
{
  // Set initial dialog field contents.
  // Set initial dialog field contents. Use the gFindInst attributes first,
  // this is necessary for window.find()
  gReplaceDialog.findKey.value           = (gFindInst.searchString
                                            ? gFindInst.searchString
                                            : gFindService.searchString);
  gReplaceDialog.replaceKey.value = gFindService.replaceString;
  gReplaceDialog.caseSensitive.checked   = (gFindInst.matchCase
                                            ? gFindInst.matchCase
                                            : gFindService.matchCase);
  gReplaceDialog.wrap.checked            = (gFindInst.wrapFind
                                            ? gFindInst.wrapFind
                                            : gFindService.wrapFind);
  gReplaceDialog.searchBackwards.checked = (gFindInst.findBackwards
                                            ? gFindInst.findBackwards
                                            : gFindService.findBackwards);

  doEnabling();
}

function onLoad()
{
  dump("EdReplace: onLoad()\n");

  // Get the xul <editor> element:
  var editorXUL = window.opener.document.getElementById("content-frame");

  // Get the nsIWebBrowserFind service:
  //gFindInst = editorXUL.docShell.QueryInterface(Components.interfaces.nsIWebBrowserFind);
  gFindInst = editorXUL.webBrowserFind;

  try {
  // get the find service, which stores global find state
    gFindService = Components.classes["@mozilla.org/find/find_service;1"]
                         .getService(Components.interfaces.nsIFindService);
  } catch(e) { dump("No find service!\n"); gFindService = 0; }

  // Init gReplaceDialog.
  initDialogObject();

  try {
    gReplaceDialog.editor = editorXUL.editor.QueryInterface(Components.interfaces.nsIPlaintextEditor);
  } catch(e) { dump("Couldn't get an editor! " + e + "\n"); }
  // If we don't get the editor, then we won't allow replacing.

  // Change "OK" to "Find".
  //dialog.find.label = document.getElementById("fBLT").getAttribute("label");

  // Fill dialog.
  loadDialog();

  if (gReplaceDialog.findKey.value)
    gReplaceDialog.findKey.select();
  else
    gReplaceDialog.findKey.focus();
}

function onUnload() {
  // Disconnect context from this dialog.
  gFindReplaceData.replaceDialog = null;
}

function saveFindData()
{
  // Set data attributes per user input.
  if (gFindService)
  {
    gFindService.searchString  = gReplaceDialog.findKey.value;
    gFindService.matchCase     = gReplaceDialog.caseSensitive.checked;
    gFindService.wrapFind      = gReplaceDialog.wrap.checked;
    gFindService.findBackwards = gReplaceDialog.searchBackwards.checked;
  }
}

function setUpFindInst()
{
  gFindInst.searchString  = gReplaceDialog.findKey.value;
  gFindInst.matchCase     = gReplaceDialog.caseSensitive.checked;
  gFindInst.wrapFind      = gReplaceDialog.wrap.checked;
  gFindInst.findBackwards = gReplaceDialog.searchBackwards.checked;
}

function onFindNext()
{
  // Transfer dialog contents to the find service.
  saveFindData();
  // set up the find instance
  setUpFindInst();

  // Search.
  var result = gFindInst.findNext();

  if (!result)
  {
    if (!gReplaceDialog.bundle)
      gReplaceDialog.bundle = document.getElementById("findBundle");
    window.alert(gReplaceDialog.bundle.getString("notFoundWarning"));
    gReplaceDialog.findKey.select();
    gReplaceDialog.findKey.focus();
  } 
  return false;
}

function onReplace()
{
  if (!gReplaceDialog.editor)
    return;

  // Transfer dialog contents to the find service.
  saveFindData();

  gReplaceDialog.editor.insertText(gReplaceDialog.replaceKey.value);
}

function onReplaceAll()
{
  // Transfer dialog contents to the find service.
  saveFindData();
  // set up the find instance
  setUpFindInst();

  // Search.
  while (true) {
    var result = gFindInst.findNext();
    dump("result of find: " + result + "\n");
    if (!result)
      break;
    gReplaceDialog.editor.insertText(gReplaceDialog.replaceKey.value);
  }

  return false;
}

function doEnabling()
{
  gReplaceDialog.enabled = gReplaceDialog.findKey.value;
  gReplaceDialog.findNext.disabled = !gReplaceDialog.findKey.value;
  gReplaceDialog.replace.disabled = (!gReplaceDialog.findKey.value
                                     || !gReplaceDialog.replaceKey.value);
  gReplaceDialog.replaceAll.disabled = (!gReplaceDialog.findKey.value
                                        || !gReplaceDialog.replaceKey.value);
}
