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

function readIRCPrefs (rootNode)
{
    if (!getPriv("UniversalXPConnect"))
        return false;

    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);

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
    CIRCNetwork.prototype.INITIAL_CHANNEL =
        getCharPref (pref, rootNode + "channel",
                     CIRCNetwork.prototype.INITIAL_CHANNEL);

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
    var e;
    
    try
    {
        return prefObj.getCharPref (prefName);
    }
    catch (e)
    {
        return defaultValue;
    }
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
