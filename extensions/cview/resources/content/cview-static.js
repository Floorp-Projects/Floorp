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
 * stored on this cview object.  Doing this instead of global variable is
 * somewhat religious; it avoids any "m" prefix for module vars, makes
 * it trivial to track down all globals, and in theory makes it possible
 * to embed the code in another app without colliding globals.
 */

/* number of items to process per refresh step */
cview.REFRESH_ITEMS_PER_CYCLE = 30;
/* number of milliseconds to wait between refresh steps */
cview.REFRESH_CYCLE_DELAY = 200;

cview.componentMode = "all";
cview.interfaceMode = "all";


/*
 * Refreshing components and interfaces is a time consuming process.
 * We'll be asserting about 1300 (currently) new resources into the rdf graph,
 * each with 4 arcs.  In order to keep mozilla from becoming *totally*
 * unresponsive during this refresh, we do the update in steps.  The refresh
 * cycle can be tweaked by adjusting the REFRESH_ITEMS_PER_CYCLE and
 * REFRESH_CYCLE_DELAY properties on the cview object.
 *
 * Normally, components and interfaces will only need to be refreshed once
 * per execution, at app start.  After that, items are hidden/shown with the
 * filter*() functions.
 */

/*
 * One step in the components refresh process
 */
function refreshComponentsStep()
{
    var i;
    
    for (i = 0; i < cview.REFRESH_ITEMS_PER_CYCLE; i++)
    {
        var cls = cview.pendingComponents.pop();
        
        if (!cls)
            break;
            
        var res = cview.rdf.GetResource ("component:" + (++cview.componentID));
        cview.rdf.Assert (res, cview.rdf.resContractID,
                          cview.rdf.GetLiteral(cls.name));
        cview.rdf.Assert (res, cview.rdf.resCLSID,
                          cview.rdf.GetLiteral(cls.id));
        cview.rdf.Assert (res, cview.rdf.resShow,
                          cview.rdf.litTrue);
        cview.rdf.Assert (cview.rdf.resRoot, cview.rdf.resCmp, res);
    }

    if (cls)
        cview.componentRefreshTimeout =
            window.setTimeout ("refreshComponentsStep();",
                               cview.REFRESH_CYCLE_DELAY);
    else
    {
        cview.rdf.setTreeRoot ("component-list", cview.rdf.resRoot);
        cview.componentMode = "all";
        delete cview.componentRefreshTimeout;
        delete cview.pendingComponents;
        delete cview.componentID;
    }

    cview.visibleComponents += i;
    refreshLabels();
    
}

/*
 * Kick off a components refresh.
 */
function refreshComponents()
{
    cview.rdf.setTreeRoot ("component-list", cview.rdf.resNullRoot);

    cview.rdf.clearTargets (cview.rdf.resRoot, cview.rdf.resCmp, true);

    if (cview.componentRefreshTimeout)
        window.clearTimeout (cview.componentRefreshTimeout);
    
    cview.pendingComponents = new Array();
    
    for (var c in Components.classes)
        cview.pendingComponents.push(Components.classes[c]);

    cview.totalComponents = cview.pendingComponents.length;
    cview.visibleComponents = 0;
    cview.componentID = 0;
    cview.componentMode = "refreshing";
    refreshLabels();
    
    cview.componentRefreshTimeout =
        window.setTimeout ("refreshComponentsStep();",
                           cview.REFRESH_CYCLE_DELAY);       
}

/*
 * One step in the interface refresh process
 */
function refreshInterfacesStep()
{
    var i;
    
    for (i = 0; i < cview.REFRESH_ITEMS_PER_CYCLE; i++)
    {
        var ifc = cview.pendingInterfaces.pop();

        if (!ifc)
            break;
            
        var res = cview.rdf.GetResource ("interface:" + (++cview.interfaceID));
        cview.rdf.Assert (res, cview.rdf.resIName,
                          cview.rdf.GetLiteral(ifc.name));
        cview.rdf.Assert (res, cview.rdf.resIID,
                          cview.rdf.GetLiteral(ifc.id));
        cview.rdf.Assert (res, cview.rdf.resShow,
                          cview.rdf.litTrue);        
        cview.rdf.Assert (cview.rdf.resRoot, cview.rdf.resIfc, res);
    }
    
    if (ifc)
        cview.interfaceRefreshTimeout =
            window.setTimeout ("refreshInterfacesStep();",
                               cview.REFRESH_CYCLE_DELAY);
    else
    {
        cview.rdf.setTreeRoot ("interface-list", cview.rdf.resRoot);
        cview.interfaceMode = "all";
        delete cview.interfaceID;
        delete cview.interfaceRefreshTimeout;
        delete cview.pendingInterfaces;
    }

    cview.visibleInterfaces += i;
    refreshLabels();
    
}

/*
 * Kick off the interface refresh process
 */
function refreshInterfaces()
{
    cview.rdf.setTreeRoot ("interface-list", cview.rdf.resNullRoot);
    cview.rdf.clearTargets (cview.rdf.resRoot, cview.rdf.resIfc, true);
    
    if (cview.interfaceRefreshTimeout)
        window.clearTimeout (cview.interfaceRefreshTimeout);
    
    cview.pendingInterfaces = new Array();
    
    for (var i in Components.interfaces)
        cview.pendingInterfaces.push(Components.interfaces[i]);

    cview.totalInterfaces = cview.pendingInterfaces.length;
    cview.visibleInterfaces = 0;
    cview.interfaceID = 0;
    cview.interfaceMode = "refreshing";
    refreshLabels();

    cview.interfaceRefreshTimeout =
        window.setTimeout ("refreshInterfacesStep();",
                           cview.REFRESH_CYCLE_DELAY);
}

/*
 * hide/show entries in the components list based on the componentMode and
 * componentFilter properties of the cview object.
 */
function filterComponents()
{
    cview.rdf.setTreeRoot ("component-list", cview.rdf.resNullRoot);
    
    cview.visibleComponents = 0;
    var cmps = cview.rdf.GetTargets (cview.rdf.resRoot, cview.rdf.resCmp);

    while (cmps.hasMoreElements())
    {
        var cmpNode = cmps.getNext().QueryInterface(nsIRDFResource);
        var cmpName = cview.rdf.GetTarget(cmpNode, cview.rdf.resContractID);
        if (cmpName)
        {
            cmpName = cmpName.QueryInterface(nsIRDFLiteral);
            cmpName = cmpName.Value;
        }    
        else
        {
            dd ("** NODE WITHOUT NAME ARC in filterComponents **");
            continue;
        }
        
        switch (cview.componentMode)
        {
            case "all":
                cview.rdf.Change (cmpNode, cview.rdf.resShow,
                                  cview.rdf.litTrue);
                cview.visibleComponents++;
                break;

            case "contains":
                if (cmpName.search(cview.componentFilter) != -1)
                {
                    cview.rdf.Change (cmpNode, cview.rdf.resShow,
                                      cview.rdf.litTrue);
                    cview.visibleComponents++;
                }
                else
                    cview.rdf.Change (cmpNode, cview.rdf.resShow,
                                      cview.rdf.litFalse);
                break;

            case "starts-with":
                if (cmpName.search(cview.componentFilter) == 0)
                {
                    cview.rdf.Change (cmpNode, cview.rdf.resShow,
                                      cview.rdf.litTrue);
                    cview.visibleComponents++;
                }
                else
                    cview.rdf.Change (cmpNode, cview.rdf.resShow,
                                      cview.rdf.litFalse);
                break;
        }
        
    }
    
    cview.rdf.setTreeRoot ("component-list", cview.rdf.resRoot);
    refreshLabels();
    
}

/*
 * Same as filterComponents() except it works on the interface list
 */
function filterInterfaces()
{
    var e;
    var cls;
    
    cview.rdf.setTreeRoot ("interface-list", cview.rdf.resNullRoot);
    
    var ifcs = cview.rdf.GetTargets (cview.rdf.resRoot, cview.rdf.resIfc);

    cview.visibleInterfaces = 0;

    if (cview.interfaceMode == "implemented-by")
    {
        cls = Components.classes[cview.interfaceFilter];
        if (cls)
            cls = cls.createInstance();
    }
    
    while (ifcs.hasMoreElements())
    {
        var ifcNode = ifcs.getNext().QueryInterface(nsIRDFResource);
        var ifcName = cview.rdf.GetTarget(ifcNode, cview.rdf.resIName);
        if (ifcName)
        {
            ifcName = ifcName.QueryInterface (nsIRDFLiteral);
            ifcName = ifcName.Value;
        }    
        else
        {
            dd ("** NODE WITHOUT NAME ARC in filterInterfaces **");
            continue;
        }
        
        switch (cview.interfaceMode)
        {
            case "all":
                cview.rdf.Change (ifcNode, cview.rdf.resShow,
                                  cview.rdf.litTrue);
                cview.visibleInterfaces++;
                break;

            case "contains":
                if (ifcName.search(cview.interfaceFilter) != -1)
                {
                    cview.rdf.Change (ifcNode, cview.rdf.resShow,
                                      cview.rdf.litTrue);
                    cview.visibleInterfaces++;    
                }
                else
                    cview.rdf.Change (ifcNode, cview.rdf.resShow,
                                      cview.rdf.litFalse);
                break;

            case "starts-with":
                if (ifcName.search(cview.interfaceFilter) == 0)
                {
                    cview.rdf.Change (ifcNode, cview.rdf.resShow,
                                      cview.rdf.litTrue);
                    cview.visibleInterfaces++;
                }
                else
                    cview.rdf.Change (ifcNode, cview.rdf.resShow,
                                      cview.rdf.litFalse);
                break;

            case "implemented-by":
                if (cls)
                    try
                    {
                        cls.QueryInterface(Components.interfaces[ifcName]);
                        cview.rdf.Change (ifcNode, cview.rdf.resShow,
                                          cview.rdf.litTrue);
                        cview.visibleInterfaces++;
                    }
                    catch (e)
                    {
                        cview.rdf.Change (ifcNode, cview.rdf.resShow,
                                          cview.rdf.litFalse);
                    }
                else
                    cview.rdf.Change (ifcNode, cview.rdf.resShow,
                                      cview.rdf.litFalse);
                break;
        }
        
    }
    
    cview.rdf.setTreeRoot ("interface-list", cview.rdf.resRoot);
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

        case "refreshing":
            value = "Refreshing Components list...";
            break;
            
        default:
            value = "Components?";
            break;
    }

    value += " (" + cview.visibleComponents + "/" +
        cview.totalComponents + ")";
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

        case "refreshing":
            value = "Refreshing Interfaces list...";
            break;

        default:
            value = "Interfaces?";
            break;
    }

    value += " (" + cview.visibleInterfaces + "/" +
        cview.totalInterfaces + ")";
    document.getElementById("interface-label").setAttribute ("value", value);
}
