/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is JSIRC Test Client #3
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

/*
 * currently recognized prefs:
 * + extensions.irc.
 *   +- nickname (String)  initial nickname
 *   +- username (String)  initial username (ie: username@host.tld)
 *   +- desc     (String)  initial description (used in whois info)
 *   +- defaultNet (String) default network to use for irc:// urls
 *   +- bugURL   (String) url to use for "bug 12345" links.  Use %s to place
 *                        the bug number.
 *   +- initialURLs (String) irc:// urls to connect to on startup, semicolon
 *   |                       seperated
 *   +- initialScripts (String) urls for scripts to run at startup,
 *   |                          semicolon seperated
 *   +- newTabLimit (Number) max number of tabs to have open before disabling
 *   |                       automatic tab creation for private messages.
 *   |                       use 0 for unlimited new tabs, or 1 to disable
 *   |                       automatic tab creation.
 *   +- raiseNewTab (Boolean) bring new tabs created in response to a private
 *   |                        message to the front.
 *   +- munger   (Boolean) send output through text->html munger
 *   |  +- colorCodes (Boolean) enable color code handling
 *   |  +- <various>  (Boolean) enable specific munger entry
 *   |  +- smileyText (Boolean) true => display text (and graphic) when
 *   |                                  matching smileys
 *   |                          false => show only the smiley graphic
 *   +- nickCompleteStr (String) String to use when tab-completing nicknames
 *   |                           at the beginning of sentences
 *   +- stalkWords (String) List of words to add to the stalk victims list
 *   |                      semicolon seperated (see the /stalk command)
 *   +- deleteOnPart (Boolean) Delete channel window automatically after a /part
 *   |
 *   |  The following beep prefs can be set to the text "beep" to use the
 *   |  system beep, or "" to disable the beep.
 *   +- msgBeep   (String) url to sound to play when a /msg is recieved
 *   +- stalkBeep (String) url to sound to play when a /stalk matches
 *   +- queryBeep (String) url to sound to play for new msgs in a /query
 *   |
 *   +- notify
 *   |  +- aggressive (Boolean) flash trayicon/ bring window to top when
 *   |                          your nickname is mentioned.
 *   +- settings
 *   |  +- autoSave (Boolean) Save settings on exit
 *   +- style   
 *   |  +- default (String) url to default style sheet
 *   +- views
 *   |  +- collapseMsgs (Boolean) Collapse consecutive messages from same
 *   |  |                         user
 *   |  +- copyMessages (Boolean) Copy important messages to the network
 *   |  |                         window
 *   |  +- client
 *   |  |  +- maxlines  (Number) max lines to keep in *client* view
 *   |  +- network
 *   |  |  +- maxlines  (Number) max lines to keep in network views
 *   |  +- channel
 *   |  |  +- maxlines  (Number) max lines to keep in channel views
 *   |  +- chanuser
 *   |     +- maxlines  (Number) max lines to keep in /msg views
 *   +- debug
 *      +- tracer (Boolean) enable/disable debug message tracing
 */

function initPrefs()
{
    client.prefSpecs = {
        "nickname": ["CIRCNetwork.prototype.INITIAL_NICK",          "IRCMonkey"],
        "username": ["CIRCNetwork.prototype.INITIAL_NAME",          "chatzilla"],
        "desc":     ["CIRCNetwork.prototype.INITIAL_DESC",   "New Now Know How"],
        "defaultNet":        ["client.DEFAULT_NETWORK",                "moznet"],
        "charset":           ["client.CHARSET",                              ""],
        "initialURLs":       ["client.INITIAL_URLS",                   "irc://"],
        "initialScripts":    ["client.INITIAL_SCRIPTS",                      ""],
        "newTabLimit":       ["client.NEW_TAB_LIMIT",                        15],
        "raiseNewTab":       ["client.RAISE_NEW_TAB",                     false],
        "nickCompleteStr":   ["client.ADDRESSED_NICK_SEP",                 ", "],
        "stalkWords":        ["client.stalkingVictims",                      []],
        "deleteOnPart":      ["client.DELETE_ON_PART",                     true],
        "stalkBeep":         ["client.STALK_BEEP",                       "beep"],
        "msgBeep":           ["client.MSG_BEEP",                    "beep beep"],
        "queryBeep":         ["client.QUERY_BEEP",                       "beep"],
        "munger":            ["client.munger.enabled",                     true],
        "munger.colorCodes": ["client.enableColors",                       true],
        "munger.smileyText": ["client.smileyText",                        false],
        "bugURL":            ["client.BUG_URL",
                               "http://bugzilla.mozilla.org/show_bug.cgi?id=%s"],
        "notify.aggressive": ["client.FLASH_WINDOW",                       true],
        "settings.autoSave": ["client.SAVE_SETTINGS",                      true],
        "debug.tracer"     : ["client.debugHook.enabled",                 false],
        "style.default":     ["client.DEFAULT_STYLE",
                                   "chrome://chatzilla/skin/output-default.css"],
        "views.collapseMsgs":      ["client.COLLAPSE_MSGS",               false],
        "views.copyMessages":      ["client.COPY_MESSAGES",                true],
        "views.client.maxlines":   ["client.MAX_MESSAGES",                  200],
        "views.network.maxlines":  ["CIRCNetwork.prototype.MAX_MESSAGES",   100],
        "views.channel.maxlines":  ["CIRCChannel.prototype.MAX_MESSAGES",   300],
        "views.chanuser.maxlines": ["CIRCChanUser.prototype.MAX_MESSAGES",  200]
    };

    const PREF_CTRID = "@mozilla.org/preferences-service;1";
    const nsIPrefService = Components.interfaces.nsIPrefService;
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    const nsIPrefBranchInternal = Components.interfaces.nsIPrefBranchInternal;

    client.prefService =
        Components.classes[PREF_CTRID].getService(nsIPrefService);
    client.prefBranch = client.prefService.getBranch ("extensions.irc.");

    var internal = client.prefBranch.QueryInterface(nsIPrefBranchInternal);
    internal.addObserver("", client.prefObserver, false);

    readPrefs();
}

function destroyPrefs()
{
    const nsIPrefBranchInternal = Components.interfaces.nsIPrefBranchInternal;
    var internal = client.prefBranch.QueryInterface(nsIPrefBranchInternal);
    internal.removeObserver("", client.prefObserver);
}

client.prefObserver = new Object();

client.prefObserver.observe =
function pref_observe (prefService, topic, prefName)
{
    if (!("prefLock" in client))
        readPref(prefName);
}

function readPref(prefName)
{
    //if (prefName.indexOf("extensions.irc." == 0))
    //    prefName = prefName.substr(15);
    var ary;
    
    if (prefName in client.prefSpecs)
    {
        var varName = client.prefSpecs[prefName][0];
        var defaultValue = client.prefSpecs[prefName][1];
        var prefValue;
        
        if (typeof defaultValue == "boolean")
        {
            prefValue = getBoolPref(prefName, defaultValue);
            eval (varName + " = " + prefValue);
        }
        else if (typeof defaultValue == "number")
        {
            prefValue = getIntPref(prefName, defaultValue);
            eval (varName + " = " + prefValue);
        }
        else if (defaultValue instanceof Array)
        {
            prefValue = getCharPref(prefName, defaultValue.join("; "));
            if (prefValue)
                eval (varName + " = " + prefValue.split(/\s*;\s*/).toSource());
            else
                eval (varName + " = []");
        }
        else
        {
            prefValue = getCharPref (prefName, defaultValue);
            eval (varName + " = " + prefValue.quote());
        }
    }
    else if ((ary = prefName.match(/munger.(.*)/)) &&
              ary[1] in client.munger.entries)
    {
        client.munger.entries[ary[1]].enabled =
            getBoolPref ("munger." + prefName,
                         client.munger.entries[ary[1]].enabled);
    }
    else
    {
        dd ("readPref: UNKNOWN PREF ``" + prefName + "''");
    }
}

function readPrefs()
{
    for (var p in client.prefSpecs)
        readPref(p);
    
    if (!client.INITIAL_URLS)
        client.INITIAL_URLS = "irc://";

    for (var entry in client.munger.entries)
    {
        if (entry[0] != ".")
        {
            client.munger.entries[entry].enabled =
                getBoolPref ("munger." + entry,
                             client.munger.entries[entry].enabled);
        }
    }    
}

function writePref(prefName)
{
    //if (prefName.indexOf("extensions.irc." == 0))
    //    prefName = prefName.substr(15);
    var ary;

    if (prefName in client.prefSpecs)
    {
        var varName = client.prefSpecs[prefName][0];
        var defaultValue = client.prefSpecs[prefName][1];
        var prefValue = eval(varName);
        
        if (typeof defaultValue == "boolean")
            client.prefBranch.setBoolPref(prefName, prefValue);
        else if (typeof defaultValue == "number")
            client.prefBranch.setIntPref(prefName, prefValue);
        else if (defaultValue instanceof Array)
            client.prefBranch.setCharPref(prefName, prefValue.join("; "));
        else
            client.prefBranch.setCharPref(prefName, prefValue);
    }
    else if ((ary = prefName.match(/munger\.(.*)/)) &&
              ary[1] in client.munger.entries)
    {
        client.prefBranch.setBoolPref (prefName,
                                       client.munger.entries[ary[1]].enabled);
    }
    else
    {
        dd ("writePref: UNKNOWN PREF ``" + prefName + "''");
    }

    if (!("prefLock" in client))
        client.prefService.savePrefFile(null);
}

function writePrefs (rootNode)
{
    client.prefLock = true;

    for (var p in client.prefSpecs)
        writePref(p);
    
    for (var entry in client.munger.entries)
    {
        if (entry[0] != ".")
        {
            writePref("munger." + entry);
        }
    }    

    delete client.prefLock;

    client.prefService.savePrefFile(null);
}

function getCharPref (prefName, defaultValue)
{
    var e, rv;
    
    try
    {
        rv = client.prefBranch.getCharPref (prefName);
    }
    catch (e)
    {
        rv = defaultValue;
    }

    //dd ("getCharPref: returning '" + rv + "' for " + prefName);
    return rv;
    
}

function getIntPref (prefName, defaultValue)
{
    var e;

    try
    {
        return client.prefBranch.getIntPref (prefName);
    }
    catch (e)
    {
        return defaultValue;
    }
    
}

function getBoolPref (prefName, defaultValue)
{
    var e;

    try
    {
        return client.prefBranch.getBoolPref (prefName);
    }
    catch (e)
    {
        return defaultValue;
    }
    
}
