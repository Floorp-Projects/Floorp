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

// the actual filter that we're editing
var gFilter;
var nsIMsgSearchValidityManager = Components.interfaces.nsIMsgSearchValidityManager;

function filterEditorOnLoad()
{
    if (window.arguments && window.arguments[0]) {
        var args = window.arguments[0];
        if (args.filter) {
            gFilter = window.arguments[0].filter;
        
            dump("Filter editor loading with filter " + gFilter.filterName + "\n");
            initializeDialog(gFilter);
        } else {
            if (args.filterList)
                setScope(getScopeFromFilterList(args.filterList));
            dump("New filter\n");
        }
    }
}

// set scope on all visible searhattribute tags
function setScope(scope) {
    var searchAttributes = document.getElementsByTagName("searchattribute");
    for (var i = 0; i<searchAttributes.length; i++) {
        searchAttributes[i].searchScope = scope;
    }
}


function scopeChanged(event)
{
    var menuitem = event.target;

    var searchattr = document.getElementById("searchAttr");
    try {
      searchattr.searchScope = menuitem.data;
    } catch (ex) {
        
    }
}

function getScopeFromFilterList(filterList)
{
    var type = filterList.folder.server.type;
    if (type == "nntp") return nsIMsgSearchValidityManager.news;
    return nsIMsgSearchValidityManager.onlineMail;
}

function getScope(filter) {
    return getScopeFromFilterList(filter.filterList);
}

function initializeDialog(filter)
{
    var filterName = document.getElementById("filterName");
    filterName.value = filter.filterName;

    setScope(getScope(filter));
}
