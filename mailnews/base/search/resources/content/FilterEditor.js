/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */


var validityManager;

function filterEditorOnLoad()
{
    validityManager = Components.classes["mozilla.mail.search.validityManager.1"].getService(Components.interfaces.nsIMsgSearchValidityManager);

    var searchAttr = document.getElementById("searchAttr");
    searchAttr.scope = 0;
}


function scopeChanged(event)
{
    var menuitem = event.target;

    var searchattr = document.getElementById("searchAttr");
    dump("setting scope to " + menuitem.data + "\n");
    try {
      searchattr.scope = menuitem.data;
    } catch (ex) {
        
    }
    //    DumpDOM(searchattr.anonymousContent[0]);
}
