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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Dan Haddix (dan6992@hotmail.com)
 */


var dialog;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  
   doSetOKCancel(onOK, null);

 // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.urlInput = document.getElementById("urlInput");

  dialog.url = window.arguments[0].getAttribute("href");

  if (dialog.url != '')
    dialog.urlInput.value = dialog.url;

  // Kinda clunky: Message was wrapped in a <p>,
  // so actual message is a child text node
  //dialog.srcMessage = (document.getElementById("srcMessage")).firstChild;

  //if (null == dialog.srcInput ||
  //    null == dialog.srcMessage )
  //{
  //  dump("Not all dialog controls were found!!!\n");
  //}

  // Set initial focus

  dialog.urlInput.focus();
}

function onOK()
{
  dump(window.arguments[0].id+"\n");
  window.arguments[0].setAttribute("href", dialog.urlInput.value);
  window.close();
}