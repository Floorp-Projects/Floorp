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

const PREF_RELOAD = true;
const PREF_WRITETHROUGH = true;
const PREF_CHARSET = "utf-8";   // string prefs stored in this charset

function PrefRecord(name, defaultValue, label, help, group)
{
    this.name = name;
    this.defaultValue = defaultValue;
    this.help = help;
    this.label = label ? label : name;
    this.group = group ? group : "";
    // Prepend the group 'general' if there isn't one already.
    if (this.group.match(/^(\.|$)/))
        this.group = "general" + this.group;
    this.realValue = null;
}

function PrefManager (branchName, defaultBundle)
{
    var prefManager = this;
    
    function pm_observe (prefService, topic, prefName)
    {
        var r = prefManager.prefRecords[prefName];
        if (!r)
            return;
        
        var oldValue = (r.realValue != null) ? r.realValue : r.defaultValue;
        r.realValue = prefManager.getPref(prefName, PREF_RELOAD);
        prefManager.onPrefChanged(prefName, r.realValue, oldValue);
    };

    const PREF_CTRID = "@mozilla.org/preferences-service;1";
    const nsIPrefService = Components.interfaces.nsIPrefService;
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    const nsIPrefBranchInternal = Components.interfaces.nsIPrefBranchInternal;

    this.prefService =
        Components.classes[PREF_CTRID].getService(nsIPrefService);
    this.prefBranch = this.prefService.getBranch(branchName);
    this.defaultValues = new Object();
    this.prefs = new Object();
    this.prefNames = new Array();
    this.prefRecords = new Object();
    this.observer = { observe: pm_observe };

    this.prefBranchInternal =
        this.prefBranch.QueryInterface(nsIPrefBranchInternal);
    this.prefBranchInternal.addObserver("", this.observer, false);
    
    this.defaultBundle = defaultBundle;

    this.valid = true;
}

PrefManager.prototype.destroy =
function pm_destroy()
{
    if (this.valid)
    {
        this.prefBranchInternal.removeObserver("", this.observer);
        this.valid = false;
    }
}

PrefManager.prototype.getBranch =
function pm_getbranch(suffix)
{
    return this.prefService.getBranch(this.prefBranch.root + suffix);
}

PrefManager.prototype.getBranchManager =
function pm_getbranchmgr(suffix)
{
    return new PrefManager(this.prefBranch.root + suffix);
}

PrefManager.prototype.onPrefChanged =
function pm_changed(prefName, prefManager, topic)
{
    /* clients can override this to hear about pref changes */
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

PrefManager.prototype.isKnownPref =
function pm_ispref(prefName)
{
    return (prefName in this.prefRecords);
}

PrefManager.prototype.addPrefs =
function pm_addprefs(prefSpecs)
{
    for (var i = 0; i < prefSpecs.length; ++i)
    {
        this.addPref(prefSpecs[i][0], prefSpecs[i][1],
                     3 in prefSpecs[i] ? prefSpecs[i][3] : null, null,
                     2 in prefSpecs[i] ? prefSpecs[i][2] : null);
    }
}

PrefManager.prototype.addDeferredPrefs =
function pm_addprefsd(targetManager, writeThrough)
{
    function deferGet(prefName)
    {
        return targetManager.getPref(prefName);
    };

    function deferSet(prefName, value)
    {
        return targetManager.setPref(prefName, value);
    };

    var setter = null;
    
    if (writeThrough)
        setter = deferSet;
    
    var prefs = targetManager.prefs;
    
    for (var i = 0; i < prefs.length; ++i)
        this.addPref(prefs[i], deferGet, setter);
}

PrefManager.prototype.updateArrayPref =
function pm_arrayupdate(prefName)
{
    var record = this.prefRecords[prefName];
    if (!ASSERT(record, "Unknown pref: " + prefName))
        return;

    if (record.realValue == null)
        record.realValue = record.defaultValue;
    
    if (!ASSERT(record.realValue instanceof Array, "Pref is not an array"))
        return;

    this.prefBranch.setCharPref(prefName, this.arrayToString(record.realValue));
    this.prefService.savePrefFile(null);
}

PrefManager.prototype.stringToArray =
function pm_s2a(string)
{
    if (string.search(/\S/) == -1)
        return [];
    
    var ary = string.split(/\s*;\s*/);
    for (var i = 0; i < ary.length; ++i)
        ary[i] = toUnicode(unescape(ary[i]), PREF_CHARSET);

    return ary;
}

PrefManager.prototype.arrayToString =
function pm_a2s(ary)
{
    var escapedAry = new Array()
    for (var i = 0; i < ary.length; ++i)
        escapedAry[i] = escape(fromUnicode(ary[i], PREF_CHARSET));

    return escapedAry.join("; ");
}

PrefManager.prototype.getPref =
function pm_getpref(prefName, reload)
{
    var prefManager = this;
    
    function updateArrayPref() { prefManager.updateArrayPref(prefName); };
    
    var record = this.prefRecords[prefName];
    if (!ASSERT(record, "Unknown pref: " + prefName))
        return null;
    
    var defaultValue;

    if (typeof record.defaultValue == "function")
    {
        // deferred pref, call the getter, and don't cache the result.
        defaultValue = record.defaultValue(prefName);
    }
    else
    {
        if (!reload && record.realValue != null)
            return record.realValue;

        defaultValue = record.defaultValue;
    }
    
    var realValue = null;

    try
    {
        if (typeof defaultValue == "boolean")
        {
            realValue = this.prefBranch.getBoolPref(prefName);
        }
        else if (typeof defaultValue == "number")
        {
            realValue = this.prefBranch.getIntPref(prefName);
        }
        else if (defaultValue instanceof Array)
        {
            realValue = this.prefBranch.getCharPref(prefName);
            realValue = this.stringToArray(realValue);
            realValue.update = updateArrayPref;
        }
        else if (typeof defaultValue == "string" ||
                 defaultValue == null)
        {
            realValue = toUnicode(this.prefBranch.getCharPref(prefName),
                                  PREF_CHARSET);
        }
    }
    catch (ex)
    {
        // if the pref doesn't exist, ignore the exception.
    }

    if (realValue == null)
        return defaultValue;

    record.realValue = realValue;
    return realValue;
}

PrefManager.prototype.setPref =
function pm_setpref(prefName, value)
{
    var prefManager = this;
    
    function updateArrayPref() { prefManager.updateArrayPref(prefName); };

    var record = this.prefRecords[prefName];
    if (!ASSERT(record, "Unknown pref: " + prefName))
        return null;
    
    var defaultValue = record.defaultValue;

    if (typeof defaultValue == "function")
        defaultValue = defaultValue(prefName);
    
    if ((record.realValue == null && value == defaultValue) ||
        record.realValue == value)
    {
        // no realvalue, and value is the same as default value ... OR ...
        // no change at all.  just bail.
        return record.realValue;
    }

    if (value == defaultValue)
    {
        this.clearPref(prefName);
        return value;
    }
    
    if (typeof defaultValue == "boolean")
    {
        this.prefBranch.setBoolPref(prefName, value);
    }
    else if (typeof defaultValue == "number")
    {
        this.prefBranch.setIntPref(prefName, value);
    }
    else if (defaultValue instanceof Array)
    {
        var str = this.arrayToString(value);
        this.prefBranch.setCharPref(prefName, str);
        value.update = updateArrayPref;
    }
    else
    {
        this.prefBranch.setCharPref(prefName, fromUnicode(value, PREF_CHARSET));
    }
    
    this.prefService.savePrefFile(null);

    record.realValue = value;
    
    return value;
}

PrefManager.prototype.clearPref =
function pm_reset(prefName)
{
    this.prefRecords[prefName].realValue = null;
    this.prefBranch.clearUserPref(prefName);
    this.prefService.savePrefFile(null);
}

PrefManager.prototype.addPref =
function pm_addpref(prefName, defaultValue, setter, bundle, group)
{
    var prefManager = this;
    if (!bundle)
        bundle = this.defaultBundle;
    
    function updateArrayPref() { prefManager.updateArrayPref(prefName); };
    function prefGetter() { return prefManager.getPref(prefName); };
    function prefSetter(value) { return prefManager.setPref(prefName, value); };
    
    if (!ASSERT(!(prefName in this.defaultValues),
                "Preference already exists: " + prefName))
    {
        return;
    }
    
    if (!setter)
        setter = prefSetter;
    
    if (defaultValue instanceof Array)
        defaultValue.update = updateArrayPref;
    
    var label = getMsgFrom(bundle, "pref." + prefName + ".label", null, prefName);
    var help  = getMsgFrom(bundle, "pref." + prefName + ".help", null, 
                           MSG_NO_HELP);
    
    if (label == prefName)
        dd("WARNING: !!! Preference without label: " + prefName);
    if (help == MSG_NO_HELP)
        dd("WARNING: Preference without help text: " + prefName);
    
    this.prefRecords[prefName] = new PrefRecord (prefName, defaultValue, 
                                                 label, help, group);
    
    this.prefNames.push(prefName);
    this.prefNames.sort();
    
    this.prefs.__defineGetter__(prefName, prefGetter);
    this.prefs.__defineSetter__(prefName, setter);
}
