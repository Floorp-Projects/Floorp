function readIRCPrefs (rootNode)
{
    var pref =
        Components.classes["component://netscape/preferences"].createInstance();
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
        return prefObj.CopyCharPref (prefName);
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
