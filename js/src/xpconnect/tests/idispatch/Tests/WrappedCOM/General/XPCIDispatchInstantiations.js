/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the IDispatch implementation for XPConnect.
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Verify that we can access but not overwrite the values of read-only 
 * attributes.
 */

test();

function testInstantiate(name, id, scriptable, methods)
{
    var IDISPATCH_TEXT = "[xpconnect wrapped IDispatch";
    var obj = COMObject(id);
    var value = obj.toString().substr(0, IDISPATCH_TEXT.length);
    reportCompare(
        IDISPATCH_TEXT,
        value,
        "var obj = COMObject(" + id + ");");
    try
    {
        obj = COMObject(id);
    }
    catch (e)
    {
        if (scriptable)
        {
            reportFailure("COMObject('" + id + "') generated an exception");
        }
        return;
    }
    value = obj.toString().substr(0, IDISPATCH_TEXT.length);
    reportCompare(
        scriptable ? IDISPATCH_TEXT : undefined,
        scriptable ? value : obj,
        "var obj = COMObject(" + id + ");");
}

function testEnumeration(compInfo)
{
    var obj = COMObject(compInfo.cid);
    compareObject(obj, compInfo, "Enumeration Test");
}

function test()
{
    try
    {

        printStatus("Instantiation Tests - " + objectsDesc.length + " objects to test");
        for (index = 0; index < objectsDesc.length; ++index)
        {
            compInfo = objectsDesc[index];
            printStatus("Testing " + compInfo.name);
            testInstantiate(compInfo.name, compInfo.cid, compInfo.scriptable);
            testInstantiate(compInfo.name, compInfo.progid, compInfo.scriptable);
        }
        // Test a non-existant COM object
        var obj;
        try
        {
            obj = COMObject("dwbnonexistantobject");
            printFailure("var obj = COMObject('dwbnonexistantobject'); did not throw an exception");
        }
        catch (e)
        {
        }
        try
        {
            obj = COMObject("dwbnonexistantobject");
            printFailure("obj = COMObject('dwbnonexistantobject'); did not throw an exception");
        }
        catch (e)
        {
        }
        printStatus("Enumeration Tests - testing " + objectsDesc.length + " objects");
        for (var index = 0; index < objectsDesc.length; ++index)
        {
            printStatus("Enumerating " + objectsDesc[index].name);
            testEnumeration(objectsDesc[index]);
        }

    } 
    catch (e)
    {
        reportFailure("Unhandled exception occured:" + e.toString());
    }
}
