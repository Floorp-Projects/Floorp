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
 *
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
 *   +- network  (String)  network to connect to on startup
 *   +- channel  (String)  channel to join after connecting
 *   +- munger   (Boolean) send output through text->html munger
 *   +- toolbar
 *   |  +- icons (Boolean) display icons in toolbar buttons
 *   +- notify
 *      +- aggressive (Boolean) flash trayicon/ bring window to top when
 *                              your nickname is mentioned.
 *   +- style   
 *   |  +- default (String) default style (relative to chrome://chatzilla/skin)
 *   |  +- user
 *   |     +- pre  (String) full path to user css, loaded before system style
 *   |     +- post (String) full path to user css, loaded after system style
 *   +- views
 *   |  +- client
 *   |  |  +- maxlines (Number) max lines to keep in *client* view
 *   |  +- channel
 *   |  |  +- maxlines (Number) max lines to keep in channel views
 *   |  +- chanuser
 *   |     +- maxlines (Number) max lines to keep in /msg views
 *   +- debug
 *      +- tracer (Boolean) enable/disable debug message tracing
 */

function readIRCPrefs (rootNode)
{
    var pref =
        Components.classes["@mozilla.org/preferences;1"].createInstance();
    if(!pref)
        throw ("Can't find pref component.");

    if (!rootNode)
        rootNode = "extensions.irc.";

    if (!rootNode.match(/\.$/))
        rootNode += ".";
    
    pref = pref.QueryInterface(Components.interfaces.nsIPref);

    CIRCNetwork.prototype.INITIAL_NICK =
        getCharPref (pref, rootNode + "nickname",
                     CIRCNetwork.prototype.INITIAL_NICK);
    CIRCNetwork.prototype.INITIAL_NAME =
        getCharPref (pref, rootNode + "username",
                     CIRCNetwork.prototype.INITIAL_NAME);
    CIRCNetwork.prototype.INITIAL_DESC =
        getCharPref (pref, rootNode + "desc",
                     CIRCNetwork.prototype.INITIAL_DESC);
    CIRCNetwork.prototype.INITIAL_CHANNEL =
        getCharPref (pref, rootNode + "channel",
                     CIRCNetwork.prototype.INITIAL_CHANNEL);

    client.STARTUP_NETWORK =
        getCharPref (pref, rootNode + "network", "");

    client.munger.enabled =
        getBoolPref (pref, rootNode + "munger", client.munger.enabled);

    client.ICONS_IN_TOOLBAR = 
        getBoolPref (pref, rootNode + "toolbar.icons", true);

    client.FLASH_WINDOW =
        getBoolPref (pref, rootNode + "notify.aggressive", true);

    client.DEFAULT_STYLE =
        getCharPref (pref, rootNode + "style.default", "output-default.css");

    client.USER_CSS_PRE =
        getCharPref (pref, rootNode + "style.user.pre", "");

    client.USER_CSS_POST =

        getCharPref (pref, rootNode + "style.user.post", "");

    client.MAX_MESSAGES = 
        getIntPref (pref, rootNode + "views.client.maxlines",
                    client.MAX_MESSAGES);

    CIRCChannel.prototype.MAX_MESSAGES =
        getIntPref (pref, rootNode + "views.channel.maxlines",
                    CIRCChannel.prototype.MAX_MESSAGES);

    CIRCChanUser.prototype.MAX_MESSAGES =
        getIntPref (pref, rootNode + "views.chanuser.maxlines",
                    CIRCChanUser.prototype.MAX_MESSAGES);
    
    var h = client.eventPump.getHook ("event-tracer");
    h.enabled =
        getBoolPref (pref, rootNode + "debug.tracer", h.enabled);
    
}

function getCharPref (prefObj, prefName, defaultValue)
{
    var e, rv;
    
    try
    {
        rv = prefObj.CopyCharPref (prefName);
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
        return prefObj.GetIntPref (prefName);
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
        return prefObj.GetBoolPref (prefName);
    }
    catch (e)
    {
        return defaultValue;
    }
    
}
