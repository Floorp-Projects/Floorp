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

function readIRCPrefs (rootNode)
{
    const PREF_CTRID = "@mozilla.org/preferences-service;1";
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch
    var pref = Components.classes[PREF_CTRID].getService(nsIPrefBranch);
    if(!pref)
        throw ("Can't find pref component.");

    if (!rootNode)
        rootNode = "extensions.irc.";

    if (!rootNode.match(/\.$/))
        rootNode += ".";
    
    CIRCNetwork.prototype.INITIAL_NICK =
        getCharPref (pref, rootNode + "nickname",
                     CIRCNetwork.prototype.INITIAL_NICK);
    CIRCNetwork.prototype.INITIAL_NAME =
        getCharPref (pref, rootNode + "username",
                     CIRCNetwork.prototype.INITIAL_NAME);
    CIRCNetwork.prototype.INITIAL_DESC =
        getCharPref (pref, rootNode + "desc",
                     CIRCNetwork.prototype.INITIAL_DESC);
    client.DEFAULT_NETWORK =
        getCharPref (pref, rootNode + "defaultNet", "moznet");
    client.CHARSET = getCharPref (pref, rootNode + "charset", "");
    client.INITIAL_URLS =
        getCharPref (pref, rootNode + "initialURLs", "irc://");
    if (!client.INITIAL_URLS)
        client.INITIAL_URLS = "irc://";
    client.INITIAL_SCRIPTS =
        getCharPref (pref, rootNode + "initialScripts", "");
    client.NEW_TAB_LIMIT =
        getIntPref (pref, rootNode + "newTabLimit", 15);
    client.RAISE_NEW_TAB =
        getBoolPref (pref, rootNode + "raiseNewTab", false);
    client.ADDRESSED_NICK_SEP =
        getCharPref (pref, rootNode + "nickCompleteStr",
                     client.ADDRESSED_NICK_SEP).replace(/\s*$/, "");
    client.INITIAL_VICTIMS =
        getCharPref (pref, rootNode + "stalkWords", "");
    
    client.DELETE_ON_PART =
        getCharPref (pref, rootNode + "deleteOnPart", true);

    client.STALK_BEEP =
        getCharPref (pref, rootNode + "stalkBeep", "beep");
    client.MSG_BEEP =
        getCharPref (pref, rootNode + "msgBeep", "beep beep");
    client.QUERY_BEEP =
        getCharPref (pref, rootNode + "queryBeep", "beep");
    
    client.munger.enabled =
        getBoolPref (pref, rootNode + "munger", client.munger.enabled);

    client.enableColors =
        getBoolPref (pref, rootNode + "munger.colorCodes", true);

    client.smileyText =
        getBoolPref (pref, rootNode + "munger.smileyText", false);

    for (var entry in client.munger.entries)
    {
        if (entry[0] != ".")
        {
            client.munger.entries[entry].enabled =
                getBoolPref (pref, rootNode + "munger." + entry,
                             client.munger.entries[entry].enabled);
        }
    }

    client.FLASH_WINDOW =
        getBoolPref (pref, rootNode + "notify.aggressive", true);

    client.SAVE_SETTINGS =
        getBoolPref (pref, rootNode + "settings.autoSave", true);

    client.DEFAULT_STYLE =
        getCharPref (pref, rootNode + "style.default",
                     "chrome://chatzilla/skin/output-default.css");
    
    client.COLLAPSE_MSGS = 
        getBoolPref (pref, rootNode + "views.collapseMsgs", false);

    client.COPY_MESSAGES = 
        getBoolPref (pref, rootNode + "views.copyMessages", true);

    client.MAX_MESSAGES = 
        getIntPref (pref, rootNode + "views.client.maxlines",
                    client.MAX_MESSAGES);

    CIRCNetwork.prototype.MAX_MESSAGES =
        getIntPref (pref, rootNode + "views.network.maxlines",
                    CIRCChanUser.prototype.MAX_MESSAGES);

    CIRCChannel.prototype.MAX_MESSAGES =
        getIntPref (pref, rootNode + "views.channel.maxlines",
                    CIRCChannel.prototype.MAX_MESSAGES);

    CIRCUser.prototype.MAX_MESSAGES =
        getIntPref (pref, rootNode + "views.chanuser.maxlines",
                    CIRCChanUser.prototype.MAX_MESSAGES);
    
    var h = client.eventPump.getHook ("event-tracer");
    h.enabled = client.debugMode =
        getBoolPref (pref, rootNode + "debug.tracer", h.enabled);
    
}

function writeIRCPrefs (rootNode)
{
    const PREF_CTRID = "@mozilla.org/preferences-service;1";
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    var pref = Components.classes[PREF_CTRID].getService(nsIPrefBranch);
    if(!pref)
        throw ("Can't find pref component.");

    if (!rootNode)
        rootNode = "extensions.irc.";

    if (!rootNode.match(/\.$/))
        rootNode += ".";
    
    pref.setCharPref (rootNode + "nickname",
                      CIRCNetwork.prototype.INITIAL_NICK);
    pref.setCharPref (rootNode + "username",
                      CIRCNetwork.prototype.INITIAL_NAME);
    pref.setCharPref (rootNode + "desc", CIRCNetwork.prototype.INITIAL_DESC);
    pref.setCharPref (rootNode + "charset", client.CHARSET);
    pref.setCharPref (rootNode + "nickCompleteStr", client.ADDRESSED_NICK_SEP);
    pref.setCharPref (rootNode + "initialURLs", client.INITIAL_URLS);
    pref.setCharPref (rootNode + "initialScripts", client.INITIAL_SCRIPTS);
    pref.setIntPref  (rootNode + "newTabLimit", client.NEW_TAB_LIMIT);
    pref.setBoolPref (rootNode + "raiseNewTab", client.RAISE_NEW_TAB);
    pref.setCharPref (rootNode + "style.default", client.DEFAULT_STYLE);
    pref.setCharPref (rootNode + "stalkWords",
                      client.stalkingVictims.join ("; "));
    pref.setCharPref (rootNode + "stalkBeep", client.STALK_BEEP);
    pref.setCharPref (rootNode + "msgBeep", client.MSG_BEEP);
    pref.setCharPref (rootNode + "queryBeep", client.QUERY_BEEP);    
    pref.setBoolPref (rootNode + "munger", client.munger.enabled);
    pref.setBoolPref (rootNode + "munger.colorCodes", client.enableColors);
    pref.setBoolPref (rootNode + "munger.smileyText", client.smileyText);

    for (var entry in client.munger.entries)
    {
        if (entry[0] != ".")
        {
            pref.setBoolPref (rootNode + "munger." + entry,
                              client.munger.entries[entry].enabled);
        }
    }
    pref.setBoolPref (rootNode + "notify.aggressive", client.FLASH_WINDOW);
    pref.setBoolPref (rootNode + "views.collapseMsgs", client.COLLAPSE_MSGS);
    pref.setBoolPref (rootNode + "views.copyMessages", client.COPY_MESSAGES);
    pref.setIntPref (rootNode + "views.client.maxlines", client.MAX_MESSAGES);
    pref.setIntPref (rootNode + "views.network.maxlines",
                     CIRCChanUser.prototype.MAX_MESSAGES);
    pref.setIntPref (rootNode + "views.channel.maxlines",
                     CIRCChannel.prototype.MAX_MESSAGES);
    pref.setIntPref (rootNode + "views.chanuser.maxlines",
                     CIRCChanUser.prototype.MAX_MESSAGES);
    
    var h = client.eventPump.getHook ("event-tracer");
    pref.setBoolPref (rootNode + "debug.tracer", h.enabled);
    
}

function getCharPref (prefObj, prefName, defaultValue)
{
    var e, rv;
    
    try
    {
        rv = prefObj.getCharPref (prefName);
    }
    catch (e)
    {
        rv = defaultValue;
    }

    //dd ("getCharPref: returning '" + rv + "' for " + prefName);
    return rv;
    
}

function getIntPref (prefObj, prefName, defaultValue)
{
    var e;

    try
    {
        return prefObj.getIntPref (prefName);
    }
    catch (e)
    {
        return defaultValue;
    }
    
}

function getBoolPref (prefObj, prefName, defaultValue)
{
    var e;

    try
    {
        return prefObj.getBoolPref (prefName);
    }
    catch (e)
    {
        return defaultValue;
    }
    
}
