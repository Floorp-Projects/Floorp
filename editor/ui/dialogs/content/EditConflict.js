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
 */

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  
  //doSetOkCancel(null,PreventCancel);
  SetWindowLocation();
}

function KeepCurrentPage()
{
dump("KeepCurrentPage\n");
  // Simple close dialog and don't change current page
  //TODO: Should we force saving of the current page?
  SaveWindowLocation();
  window.close();
  return;
}

function UseOtherPage()
{
dump("UseOtherPage\n");
  // Reload the URL -- that will get other editor's contents
  //editorShell.LoadUrl(editorShell.editorDocument.location);
  setTimeout("editorShell.LoadUrl(editorShell.editorDocument.location)", 10);
  SaveWindowLocation();
  window.close();
}

function PreventCancel()
{
  SaveWindowLocation();

  // Don't let Esc key close the dialog!
  return false;
}
