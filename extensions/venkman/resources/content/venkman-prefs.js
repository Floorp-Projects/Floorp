/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is The JavaScript Debugger
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Robert Ginda, <rginda@netscape.com>, original author
 *
 */

console.prefs = new Object();

const PREF_CTRID = "@mozilla.org/preferences-service;1";
const nsIPrefService = Components.interfaces.nsIPrefService;
const nsIPrefBranch = Components.interfaces.nsIPrefBranch;

function initPrefs()
{

    console.prefs = new Object();
    console.prefs.prefService =
        Components.classes[PREF_CTRID].getService(nsIPrefService);
    console.prefs.prefBranch = 
        console.prefs.prefService.getBranch("extensions.venkman.");
    console.prefs.prefNames = new Array();
    
    //    console.addPref ("input.commandchar", "/");    
    console.addPref ("enableChromeFilter", false);
    console.addPref ("profile.template.html",
                     "chrome://venkman/content/profile.html.tpl");
    console.addPref ("profile.ranges",
                     "1000000, 5000, 2500, 1000, 750, 500, 250, 100, 75, 50, " +
                     "25, 10, 7.5, 5, 2.5, 1, 0.75, 0.5, 0.25");
    console.addPref ("sourcetext.tab.width", 4);
    console.addPref ("input.history.max", 20);
    console.addPref ("input.dtab.time", 500);
    console.addPref ("initialScripts", "");

    var list = console.prefs.prefBranch.getChildList("extensions.venkman.", {});
    for (var p in list)
    {
        dd ("pref list " + list[p]);
        if (!(list[p] in console.prefs))
            console.addPref(list[p]);
    }                                                 
}

console.addPref =
function con_addpref (prefName, defaultValue)
{
    var realValue;
    
    function prefGetter ()
    {
        if (typeof realValue == "undefined")
        {
            var type = this.prefBranch.getPrefType (prefName);
            try
            {
                switch (type)
                {
                    case nsIPrefBranch.PREF_STRING:
                        realValue = this.prefBranch.getCharPref (prefName);
                        break;
                        
                    case nsIPrefBranch.PREF_INT:
                        realValue = this.prefBranch.getIntPref (prefName);
                        break;
                        
                    case nsIPrefBranch.PREF_BOOL:
                        realValue = this.prefBranch.getBoolPref (prefName);
                        break;
                        
                    default:
                        realValue = defaultValue;
                }
            }
            catch (ex)
            {
                //dd ("caught exception reading pref ``" + prefName + "'' " +
                //    type + "\n" + ex);
                realValue = defaultValue;
            }
        }
        return realValue;
    }
    
    function prefSetter (value)
    {
        try
        {    
            switch (typeof value)
            {
                case "int":
                    realValue = value;
                    this.prefBranch.setIntPref (prefName, value);
                    break;
                    
                case "boolean":
                    realValue = value;
                    this.prefBranch.setBoolPref (prefName, value);
                    break;
                    
                default:
                    realValue = value;
                    this.prefBranch.setCharPref (prefName, value);
                    break;       
            }

            this.prefService.savePrefFile(null);
        }
        catch (ex)
        {
            dd ("caught exception writing pref ``" + prefName + "''\n" + ex);
        }

        return value;

    }

    if (prefName in console.prefs)
        return;

    console.prefs.prefNames.push(prefName);    
    console.prefs.prefNames.sort();
    console.prefs.__defineGetter__(prefName, prefGetter);
    console.prefs.__defineSetter__(prefName, prefSetter);
}
