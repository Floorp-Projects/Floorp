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

    /* see cview-rdf.js for the definition of RDFHelper */
    cview.rdf = new RDFHelper();
    
    cview.rdf.initTree ("component-list");
    cview.rdf.initTree ("interface-list");

    cview.rdf.setTreeRoot ("component-list", cview.rdf.resRoot);
    cview.rdf.setTreeRoot ("interface-list", cview.rdf.resRoot);

    onSortCol ("componentcol-contractid");
    onSortCol ("interfacecol-iname");
    
    refreshComponents();
    refreshInterfaces();

    refreshLabels();
    
}

function onUnload()
{
    /* nothing important to do onUnload yet */
}

function onSortCol(sortColName)
{
    /* sort the <tree> widget on the column name |sortColName|.  This code
     * was basically ripped off some mozilla code that I can't recall at the
     * moment.
     */
    const nsIXULSortService = Components.interfaces.nsIXULSortService;
    const isupports_uri = "@mozilla.org/rdf/xul-sort-service;1";
    
    var node = document.getElementById(sortColName);
    // determine column resource to sort on
    var sortResource =
        node.getAttribute("rdf:resource");
    if (!node)
        return false;
 
    var sortDirection = "ascending";
        //node.setAttribute("sortActive", "true");

    switch (sortColName)
    {
        case "componentcol-contractid":
            document.getElementById("componentcol-clsid")
                .setAttribute("sortDirection", "natural");
            break;
        case "componentcol-clsid":
            document.getElementById("componentcol-contractid")
                .setAttribute("sortDirection", "natural");
            break;
        case "interfacecol-iname":
            document.getElementById("interfacecol-iid")
                .setAttribute("sortDirection", "natural");
            break;
        case "interfacecol-iid":
            document.getElementById("interfacecol-iname")
                .setAttribute("sortDirection", "natural");
            break;
    }
    
    var currentDirection = node.getAttribute('sortDirection');
    
    if (currentDirection == "ascending")
        sortDirection = "descending";
    else if (currentDirection == "descending")
        sortDirection = "natural";
    else
        sortDirection = "ascending";
    
    node.setAttribute ("sortDirection", sortDirection);
 
    var xulSortService =
        Components.classes[isupports_uri].getService(nsIXULSortService);
    if (!xulSortService)
        return false;
    try
    {
        xulSortService.Sort(node, sortResource, sortDirection);
    }
    catch(ex)
    {
        //dd("Exception calling xulSortService.Sort()");
    }
    
}

function onComponentClick(e)
{
    cview.lastContractID = e.target.parentNode.getAttribute("contractid");
    
    if (cview.interfaceMode == "implemented-by")
    {
        cview.interfaceFilter = cview.lastContractID;
        filterInterfaces();
    }
}

function onInterfaceClick(e)
{
    cview.lastIID = e.target.parentNode.getAttribute("iid");
    refreshLabels();
}

function onLXRIFCLookup (e, type)
{
    var treerow = document.popupNode.parentNode;
    
    var iname = treerow.getAttribute("iname");
    if (!iname)
    {
        dd ("** NO INAME attribute in onLXRIFCLookup **");
        return;    
    }

    /* Specifing "_content" as the second parameter to window.open places
     * the url in the most recently open browser window
     */
    window.open ("http://lxr.mozilla.org/mozilla/" + type + iname,
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

    var typeDesc = (ary[1] == "cmp") ? "Components" : "Interfaces";
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
