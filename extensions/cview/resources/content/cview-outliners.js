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

function initOutliners()
{
    const ATOM_CTRID = "@mozilla.org/atom-service;1";
    const nsIAtomService = Components.interfaces.nsIAtomService;

    var atomsvc =
        Components.classes[ATOM_CTRID].getService(nsIAtomService);

    cview.interfaceView.atomInterface = atomsvc.getAtom("interface");

    cview.componentView.atomComponent = atomsvc.getAtom("component");
    cview.componentView.atomMethod    = atomsvc.getAtom("method");
    cview.componentView.atomInterface = atomsvc.getAtom("interface");

    var i;
    var ary = new Array();
    var names = keys(Components.interfaces).sort();
    for (i in names)
    {
        ary.push(new InterfaceRecord (names[i]));
    }

    cview.interfaceView.childData.setSortColumn ("ifc-name");
    cview.interfaceView.childData.appendChildren(ary, true);
    
    ary = new Array();
    names = keys(Components.classes).sort();
    for (i in names)
    {
        ary.push(new ComponentRecord (names[i]));
    }

    cview.componentView.childData.setSortColumn ("cmp-name");
    cview.componentView.childData.appendChildren (ary, true);
       
    var outliner = document.getElementById("component-outliner");
    outliner.outlinerBoxObject.view = cview.componentView;

    outliner = document.getElementById("interface-outliner");
    outliner.outlinerBoxObject.view = cview.interfaceView;

}

var interfaceShare = new Object();

cview.interfaceView = new TreeOView(interfaceShare);

cview.interfaceView.getRowProperties =
function ifc_getrow (index, properties)
{    
    properties.AppendElement(cview.interfaceView.atomInterface);
}

function InterfaceRecord (ifc)
{
    this.setColumnPropertyName ("ifc-name",   "name");
    this.setColumnPropertyName ("ifc-number", "number");

    this.name = ifc;
    this.ifc = Components.interfaces[ifc];
    if (this.ifc)
    {
        this.number = this.ifc.number;
    }
    else
    {
        this.number = "???";
        this.isBroke = true;
    }

    this.sortName = this.name;
    this.sortNumber = this.number;
}

InterfaceRecord.prototype = new TreeOViewRecord(interfaceShare);

InterfaceRecord.prototype.getText =
function cir_text ()
{
    return ("Interface name - " + this.name + "\n" +
            "Interface ID   - " + this.number + "\n");
}

var componentShare = new Object();

cview.componentView = new TreeOView(componentShare);

cview.componentView.getCellProperties =
function cmp_getrow (index, colID, properties)
{
    if (colID != "cmp-name")
        return;

    var row = this.childData.locateChildByVisualRow(index);
    if (!row)
        return;
    
    row.getProperties(properties);
}

function ComponentRecord (cmp)
{
    this.setColumnPropertyName ("cmp-name",   "name");
    this.setColumnPropertyName ("cmp-number", "number");

    this.name = cmp;
    this.cmp = Components.classes[cmp];
    if (this.cmp)
    {
        this.number = this.cmp.number;
    }
    else
    {
        this.number = "???";
        this.isBroke = true;
    }

    this.sortName = this.name;
    this.sortNumber = this.number;
    this.reserveChildren();
}

ComponentRecord.prototype = new TreeOViewRecord(componentShare);

ComponentRecord.prototype.getProperties =
function cr_getprops (properties)
{
    properties.AppendElement(cview.componentView.atomComponent);
}

ComponentRecord.prototype.getText =
function cr_text ()
{
    return ("Contract ID    - " + this.name + "\n" +
            "Class ID       - " + this.number + "\n");
}

ComponentRecord.prototype.onPreOpen =
function cr_preopen ()
{
    if ("populated" in this)
        return;
    
    var ex;
    cls = Components.classes[this.name];
    if (!cls)
        return;

    try
    {
        cls = cls.createInstance();
    }
    catch (ex)
    {
        dd("caught " + ex + " creating instance of " + cls);
        return;
    }

    for (var i in Components.interfaces)
    {
        try
        {
            var ifc = Components.interfaces[i];
            cls = cls.QueryInterface(ifc);
            this.appendChild (new ChildInterfaceRecord(ifc, cls));
        }
        catch (ex)
        {
        }
        
    }

    this.populated = true;    
}

function ChildInterfaceRecord (ifc, instance)
{
    this.setColumnPropertyName ("cmp-name",   "name");
    this.setColumnPropertyName ("cmp-number", "number");

    this.instance = instance;
    this.ifc = ifc;
    if (ifc)
    {
        this.name = ifc.name;
        this.number = ifc.number;
    }
    else
    {
        this.name ="???";
        this.number = "???";
        this.isBroke = true;
    }

    this.sortName = this.name;
    this.sortNumber = this.number;
    this.reserveChildren();
}

ChildInterfaceRecord.prototype = new TreeOViewRecord(componentShare);

ChildInterfaceRecord.prototype.getText =
function cir_text ()
{
    return (this.parentRecord.getText() +
            "Interface name - " + this.name + "\n" +
            "Interface ID   - " + this.number + "\n");
}

ChildInterfaceRecord.prototype.onPreOpen =
function cir_preopen ()
{
    if ("populated" in this)
        return;

    for (var m in this.instance)
        this.appendChild(new MethodRecord(m));

    this.populated = true;
}

ChildInterfaceRecord.prototype.getProperties =
function cir_getprops (properties)
{
    properties.AppendElement(cview.componentView.atomInterface);
}

function MethodRecord (name)
{
    this.setColumnPropertyName ("cmp-name",   "name");
    this.name = name;
    this.sortName = this.name;
}

MethodRecord.prototype = new TreeOViewRecord(componentShare);

MethodRecord.prototype.getText =
function mr_text ()
{
    return (this.parentRecord.getText() +
            "Methods        - " + 
            keys(this.parentRecord.instance).join("\n                 "));
}

MethodRecord.prototype.getProperties =
function mr_getprops (properties)
{
    properties.AppendElement(cview.componentView.atomMethod);
}
