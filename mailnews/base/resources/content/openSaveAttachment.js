/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Henrik Gemal <gemal@gemal.dk>
 */

function onLoad()
{
  var docnamebox = document.getElementById("documentNameBox");
  if (docnamebox)
  {
    var docname = window.arguments[0].dspname;
    var item = document.createElement('text');
    if (item)
    {
      docnamebox.appendChild(item);
      item.setAttribute('value', unescape(docname));
    }
  }
  var saveit = document.getElementById("saveIt");
  if (saveit)
  {
    window.arguments[0].opval = 2;
  }
  doSetOKCancel(0, onCancel, 0, 0);
  moveToAlertPosition();
}

function onOpen()
{
  window.arguments[0].opval = 1;
}

function onSave()
{
  window.arguments[0].opval = 2;
}

function onCancel()
{
  window.arguments[0].opval = 0;
  return true;
}
