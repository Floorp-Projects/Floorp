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

function GetSelectedFilter() {
    var filterTree = document.getElementById("filterTree");
    var selectedRows = filterTree.getElementsByAttribute("selected", "true");
    
    var selectedURIs = new Array();
    
    for (var i=0; i<selectedRows.length; i++) {
      selectedURIs[i] = selectedRows[i].getAttribute("id");
    }

    return selectedURIs[0];
}

function EditFilter() {
  var uri = GetSelectedFilter();
  dump("Editing filter " + uri + "\n");
  if (uri)
    window.openDialog("chrome://messenger/content/FilterEditor.xul", "FilterEditor", "chrome");
}

function NewFilter() {
  var uri = GetSelectedFilter();
  dump("Editing filter " + uri + "\n");

  // pass the URI too, so that we know what filter to put this before
  window.openDialog("chrome://messenger/content/FilterEditor.xul", "FilterEditor", "chrome");

}
