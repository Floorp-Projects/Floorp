/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is The JavaScript Debugger.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>, original author
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

function PrefManager (branchName)
{
    var prefManager = this;
    
    function pm_observe (prefService, topic, prefName)
    {
        prefManager.dirtyPrefs[prefName] = true;
    };

    const PREF_CTRID = "@mozilla.org/preferences-service;1";
    const nsIPrefService = Components.interfaces.nsIPrefService;
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    const nsIPrefBranchInternal = Components.interfaces.nsIPrefBranchInternal;

    this.prefService =
        Components.classes[PREF_CTRID].getService(nsIPrefService);
    this.prefBranch = this.prefService.getBranch(branchName);
    this.prefNames = new Array();
    this.dirtyPrefs = new Object();
    this.prefs = new Object();
    this.observer = { observe: pm_observe };

    this.prefBranchInternal =
        this.prefBranch.QueryInterface(nsIPrefBranchInternal);
    this.prefBranchInternal.addObserver("", this.observer, false);
}


PrefManager.prototype.destroy =
function pm_destroy()
{
    this.prefBranchInternal.removeObserver("", this.observer);
}

PrefManager.prototype.listPrefs =
function pm_listprefs (prefix)
{
    var list = new Array();
    var names = this.prefNames;
    for (var i = 0; i < names.length; ++i)
    {
        if (!prefix || names[i].indexOf(prefix) == 0)
            list.push (names[i]);
    }

    return list;
}

PrefManager.prototype.readPrefs =
function pm_readprefs ()
{
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;

    var list = this.prefBranch.getChildList("", {});
    for (var i = 0; i < list.length; ++i)
    {
        if (!(list[i] in this))
        {
            var type = this.prefBranch.getPrefType (list[i]);
            var defaultValue;
            
            switch (type)
            {
                case nsIPrefBranch.PREF_INT:
                    defaultValue = 0;
                    break;
                
                case nsIPrefBranch.PREF_BOOL:
                    defaultValue = false;
                    break;

                default:
                    defaultValue = "";
            }
            
            this.addPref(list[i], defaultValue);
        }
    }
}

PrefManager.prototype.addPrefs =
function pm_addprefs (prefSpecs)
{
    for (var i = 0; i < prefSpecs.length; ++i)
        this.addPref(prefSpecs[i][0], prefSpecs[i][1]);
}
        
PrefManager.prototype.addPref =
function pm_addpref (prefName, defaultValue)
{
    var realValue;
    
    var prefManager = this;
    
    function prefGetter ()
    {
        //dd ("getter for ``" + prefName + "''");
        //dd ("default: " + defaultValue);
        //dd ("real: " + realValue);

        if (typeof realValue == "undefined" ||
            prefName in prefManager.dirtyPrefs)
        {
            try
            {
                if (typeof defaultValue == "boolean")
                {
                    realValue = prefManager.prefBranch.getBoolPref(prefName);
                }
                else if (typeof defaultValue == "number")
                {
                    realValue = prefManager.prefBranch.getIntPref(prefName);
                }
                else if (defaultValue instanceof Array)
                {
                    realValue = prefManager.prefBranch.getCharPref(prefName);
                    realValue = realValue.split(/s*;\s*/);
                    for (i = 0; i < realValue.length; ++i)
                        realValue[i] = unencode(realValue[i]);
                }
                else if (typeof defaultValue == "string")
                {
                    realValue = prefManager.prefBranch.getCharPref(prefName);
                }
                else
                {
                    realValue = defaultValue;
                }
            }
            catch (ex)
            {
                //dd ("caught exception reading pref ``" + prefName + "''\n" +
                //ex);
                realValue = defaultValue;
            }
        }

        delete prefManager.dirtyPrefs[prefName];
        return realValue;
    }
    
    function prefSetter (value)
    {
        try
        {
            if (typeof defaultValue == "boolean")
            {
                prefManager.prefBranch.setBoolPref(prefName, value);
            }
            else if (typeof defaultValue == "number")
            {
                prefManager.prefBranch.setIntPref(prefName, value);
            }
            else if (defaultValue instanceof Array)
            {
                var ary = new Array();
                for (i = 0; i < value.length; ++i)
                    ary[i] = encode(value[i]);
                
                prefManager.prefBranch.setCharPref(prefName, ary.join("; "));
            }
            else
            {
                prefManager.prefBranch.setCharPref(prefName, value);
            }

            prefManager.prefService.savePrefFile(null);
        }
        catch (ex)
        {
            dd ("caught exception writing pref ``" + prefName + "''\n" + ex);
        }

        return value;
    };
    
    if (!arrayContains(prefManager.prefNames, prefName))
        prefManager.prefNames.push(prefName);

    prefManager.prefNames.sort();
    prefManager.prefs.__defineGetter__(prefName, prefGetter);
    prefManager.prefs.__defineSetter__(prefName, prefSetter);
}
