/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@nestcape.com, original author
 */

/*
 * This file contains event handlers for cview.  The main code of cview is
 * divided between this file and cview-static.js
 */

function onLoad()
{
    initOutliners();
    cview.totalComponents = cview.visibleComponents = 
        cview.componentView.rowCount;
    cview.totalInterfaces = cview.visibleInterfaces = 
        cview.interfaceView.rowCount;
    refreshLabels();
}

function onUnload()
{
    /* nothing important to do onUnload yet */
}

function onComponentClick(e)
{
    if (e.originalTarget.localName == "outlinercol")
    {
        onOutlinerResort(e, cview.componentView);
    }
    else
    {
        /*
        cview.lastContractID = e.target.parentNode.getAttribute("contractid");
    
        if (cview.interfaceMode == "implemented-by")
        {
            cview.interfaceFilter = cview.lastContractID;
            filterInterfaces();
        }
        */
    }
}

function onInterfaceClick(e)
{
    if (e.originalTarget.localName == "outlinercol")
    {
        onOutlinerResort(e, cview.interfaceView);
    }
    else
    {
        /*
        cview.lastIID = e.target.parentNode.getAttribute("iid");
        refreshLabels();
        */
    }
}

function onOutlinerResort (e, view)
{
    /* resort by column */
    var rowIndex = new Object();
    var colID = new Object();
    var childElt = new Object();
    
    var obo = view.outliner;
    obo.getCellAt(e.clientX, e.clientY, rowIndex, colID, childElt);
    var prop;
    switch (colID.value.substr(4))
    {
        case "name":
            prop = "sortName";
            break;
        case "number":
            prop = "sortNumber";
            break;
    }
    
    var root = view.childData;
    var dir = (prop == root._share.sortColumn) ?
        root._share.sortDirection * -1 : 1;
    root.setSortColumn (prop, dir);
}

function onComponentSelect (e)
{
    var index = cview.componentView.outliner.selection.currentIndex;
    var row = cview.componentView.childData.locateChildByVisualRow (index);
    if (!row)
        return;

    var text = document.getElementById ("output-text");
    text.value = row.getText();
}

function onInterfaceSelect (e)
{
    var index = cview.interfaceView.outliner.selection.currentIndex;
    var row = cview.interfaceView.childData.locateChildByVisualRow (index);
    if (!row)
        return;

    var text = document.getElementById ("output-text");
    text.value = row.getText();
}
            
function onLXRIFCLookup (e, type)
{
    var index = cview.interfaceView.outliner.selection.currentIndex;
    var row = cview.interfaceView.childData.locateChildByVisualRow (index);
    if (!row)
        return;
    
    /* Specifing "_content" as the second parameter to window.open places
     * the url in the most recently open browser window
     */
    window.open ("http://lxr.mozilla.org/mozilla/" + type + row.name,
                 "_content", "");
}

function onChangeDisplayMode (e)
{
    var id = e.target.getAttribute("id");
    var ary = id.match (/menu-(cmp|ifc)-show-(.*)/);
    if (!ary)
    {
        dd ("** UNKNOWN ID '" + id + "' in onChangeDisplayMode **");
        return;
    }
    
    var typeDesc;
        var el;
    if (ary[1] == "cmp") {
        typeDesc = "Components";
        el = document.getElementById("menu-cmp-show-all");
        el.setAttribute("checked", false);
        el = document.getElementById("menu-cmp-show-contains");
        el.setAttribute("checked", false);
        el = document.getElementById("menu-cmp-show-starts-with");
        el.setAttribute("checked", false);
    } else {
        typeDesc = "Interfaces";
        el = document.getElementById("menu-ifc-show-all");
        el.setAttribute("checked", false);
        el = document.getElementById("menu-ifc-show-contains");
        el.setAttribute("checked", false);
        el = document.getElementById("menu-ifc-show-starts-with");
        el.setAttribute("checked", false);
        el = document.getElementById("menu-ifc-show-implemented-by");
        el.setAttribute("checked", false);
    }
    
    e.target.setAttribute ("checked", true);
    var filter = "";

    if (ary[2] == "contains")
    {
        filter = window.prompt ("Show only " + typeDesc + " containing...");
        if (!filter)
            return;
        filter = new RegExp(filter, "i");    
    }
    
    if (ary[2] == "starts-with")
    {
        filter = window.prompt ("Show only " + typeDesc + " starting with...");
        if (!filter)
            return;
        filter = new RegExp(filter, "i");    
    }

    if (ary[2] == "implemented-by")
        filter = cview.lastContractID;

    if (ary[1] == "cmp")
    {
        cview.componentMode = ary[2];
        cview.componentFilter = filter;
        filterComponents();
    }
    else
    {
        cview.interfaceMode = ary[2];
        cview.interfaceFilter = filter;
        filterInterfaces();
    }
    
}
