/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Insert Source HTML dialog */

var dialog;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);
  
  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.srcInput = document.getElementById("srcInput");

  // Kinda clunky: Message was wrapped in a <p>,
  // so actual message is a child text node
  dialog.srcMessage = (document.getElementById("srcMessage")).firstChild;

  if (null == dialog.srcInput ||
      null == dialog.srcMessage )
  {
    dump("Not all dialog controls were found!!!\n");
  }

  // Set initial focus

  dialog.srcInput.focus();
}

function onOK()
{
  if (dialog.srcInput.value != "")
    editorShell.InsertSource(dialog.srcInput.value);
  else {
    dump("Null value -- not inserting\n");
    return false;
  }
  
  return true;
}

