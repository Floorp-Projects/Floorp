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
 * This file contains "static" functions for cview.  In this case static just
 * means not-an-event-handler.  If you can come up with a better name, let
 * me know.  The main code of cview is divided between this file and
 * cview-handlers.js
 */

var cview = new Object();
/*
 * instead of creating a slew of global variables, all app-wide values are
 * stored on this cview object.
 */

/* number of items to process per refresh step */
cview.REFRESH_ITEMS_PER_CYCLE = 30;
/* number of milliseconds to wait between refresh steps */
cview.REFRESH_CYCLE_DELAY = 200;

cview.componentMode = "all";
cview.interfaceMode = "all";

/*
 * hide/show entries in the components list based on the componentMode and
 * componentFilter properties of the cview object.
 */
function filterComponents()
{
    function filterAll (rec)
    {
        if (rec.isHidden)
            rec.unHide();
        cview.visibleComponents++;
    }
        
    function filterContains (rec)
    {
        if (rec.name.search(cview.componentFilter) != -1)
        {
            if (rec.isHidden)
                rec.unHide();
            cview.visibleComponents++;
        }
        else
        {
            if (!rec.isHidden)
                rec.hide();
        }
    }
        
    function filterStarts (rec)
    {
        if (rec.name.search(cview.componentFilter) == 0)
        {
            if (rec.isHidden)
                rec.unHide();
            cview.visibleComponents++;
        }
        else
        {
            if (!rec.isHidden)
                rec.hide();
        }
    }        
            
    var cmps  = cview.componentView.childData.childData;
    var count = cmps.length;

    cview.visibleComponents = 0;

    switch (cview.componentMode)
    {
        case "all":
            filterFunction = filterAll;
            break;

        case "contains":
            filterFunction = filterContains;
            break;

        case "starts-with":
            filterFunction = filterStarts;
            break;
    }
        

    for (var i = 0; i < count; ++i)
        filterFunction (cmps[i]);
    
    refreshLabels();
    
}

/*
 * Same as filterComponents() except it works on the interface list
 */
function filterInterfaces()
{
    function filterAll (rec)
    {
        if (rec.isHidden)
            rec.show();
        cview.visibleInterfaces++;
    }
        
    function filterContains (rec)
    {
        if (rec.name.search(cview.interfaceFilter) != -1)
        {
            if (rec.isHidden)
                rec.show();
            cview.visibleInterfaces++;
        }
        else
        {
            if (!rec.isHidden)
                rec.hide();
        }
    }
        
    function filterStarts (rec)
    {
        if (rec.name.search(cview.interfaceFilter) == 0)
        {
            if (rec.isHidden)
                rec.show();
            cview.visibleInterfaces++;
        }
        else
        {
            if (!rec.isHidden)
                rec.hide();
        }
    }

    var ifcs  = cview.interfaceView.childData.childData;
    var count = ifcs.length;

    cview.visibleInterfaces = 0;

    switch (cview.interfaceMode)
    {
        case "all":
            filterFunction = filterAll;
            break;

        case "contains":
            filterFunction = filterContains;
            break;

        case "starts-with":
            filterFunction = filterStarts;
            break;
    }
        

    for (var i = 0; i < count; ++i)
        filterFunction (ifcs[i]);
    
    refreshLabels();    

}


/*
 * Update the list headings based on the componentMode and
 * componentFilter properties of the cview object.
 */
function refreshLabels()
{
    var value = "";
    
    switch (cview.componentMode)
    {
        case "all":
            value = "All components";
            break;
            
        case "contains":
            value = "Components containing '" + cview.componentFilter + "'";
            break;

        case "starts-with":
            value = "Components starting with '" + cview.componentFilter + "'";
            break;

        default:
            value = "Components?";
            break;
    }

    value += " (" + cview.visibleComponents + "/" + cview.totalComponents + ")";
    document.getElementById("component-label").setAttribute ("value", value);
    
    switch (cview.interfaceMode)
    {
        case "all":
            value = "All interfaces";
            break;
            
        case "contains":
            value = "Interfaces containing '" + cview.interfaceFilter + "'";
            break;

        case "starts-with":
            value = "Interfaces starting with '" + cview.interfaceFilter + "'";
            break;

        case "implemented-by":
            if (!cview.interfaceFilter)
                value = "Please select a component";
            else
                value = "Interfaces implemented by '" +
                    Components.classes[cview.interfaceFilter] + "'";
            break;

        default:
            value = "Interfaces?";
            break;
    }

    value += " (" + cview.visibleInterfaces + "/" + cview.totalInterfaces + ")";
    document.getElementById("interface-label").setAttribute ("value", value);
}
