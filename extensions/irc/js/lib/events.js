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
 * The Original Code is JSIRC Library.
 *
 * The Initial Developer of the Original Code is
 * New Dimensions Consulting, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@ndcico.com, original author
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

/*
 * event class
 */

function CEvent (set, type, destObject, destMethod)
{

    this.set = set;
    this.type = type;
    this.destObject = destObject;
    this.destMethod = destMethod;
    this.hooks = new Array();

}

/*
 * event pump
 */
function CEventPump (eventsPerStep)
{

    /* event routing stops after this many levels, safety valve */
    this.MAX_EVENT_DEPTH = 50;
    this.eventsPerStep = eventsPerStep;
    this.queue = new Array();
    this.hooks = new Array();
    
}

CEventPump.prototype.onHook = 
function ep_hook(e, hooks)
{
    var h;
    
    if (typeof hooks == "undefined")
        hooks = this.hooks;
    
  hook_loop:
    for (h = hooks.length - 1; h >= 0; h--)
    {
        if (!hooks[h].enabled ||
            !matchObject (e, hooks[h].pattern, hooks[h].neg))
            continue hook_loop;

        e.hooks.push (hooks[h]);
        var rv = hooks[h].f(e);
        if ((typeof rv == "boolean") &&
            (rv == false))
        {
            dd ("hook #" + h + " '" + 
                ((typeof hooks[h].name != "undefined") ? hooks[h].name :
                 "") + "' stopped hook processing.");
            return true;
        }
    }

    return false;
}

CEventPump.prototype.addHook = 
function ep_addhook(pattern, f, name, neg, enabled, hooks)
{
    if (typeof hooks == "undefined")
        hooks = this.hooks;
    
    if (typeof f != "function")
        return false;

    if (typeof enabled == "undefined")
        enabled = true;
    else
        enabled = Boolean(enabled);

    neg = Boolean(neg);

    var hook = {
        pattern: pattern,
        f: f,
        name: name,
        neg: neg,
        enabled: enabled
    };
    
    hooks.push(hook);

    return hook;

}

CEventPump.prototype.getHook = 
function ep_gethook(name, hooks)
{
    if (typeof hooks == "undefined")
        hooks = this.hooks;

    for (var h in hooks)
        if (hooks[h].name.toLowerCase() == name.toLowerCase())
            return hooks[h];

    return null;

}

CEventPump.prototype.removeHookByName = 
function ep_remhookname(name, hooks)
{
    if (typeof hooks == "undefined")
        hooks = this.hooks;

    for (var h in hooks)
        if (hooks[h].name.toLowerCase() == name.toLowerCase())
        {
            arrayRemoveAt (hooks, h);
            return true;
        }

    return false;

}

CEventPump.prototype.removeHookByIndex = 
function ep_remhooki(idx, hooks)
{
    if (typeof hooks == "undefined")
        hooks = this.hooks;

    return arrayRemoveAt (hooks, idx);

}

CEventPump.prototype.addEvent = 
function ep_addevent (e)
{

    e.queuedAt = new Date();
    arrayInsertAt(this.queue, 0, e);
    return true;
    
}

CEventPump.prototype.routeEvent = 
function ep_routeevent (e)
{
    var count = 0;

    this.currentEvent = e;
        
    e.level = 0;
    while (e.destObject)
    {
        e.level++;
        this.onHook (e);
        var destObject = e.destObject;
        e.currentObject = destObject;
        e.destObject = (void 0);
        
        switch (typeof destObject[e.destMethod])
        {
            case "function":
                if (1)
                    try
                    {
                        destObject[e.destMethod] (e);
                    }
                    catch (ex)
                    {
                        if (typeof ex == "string")
                        {
                            dd ("Error routing event " + e.set + "." + 
                                e.type + ": " + ex);
                        }
                        else
                        {
                            dd ("Error routing event " + e.set + "." + 
                                e.type + ": " + dumpObjectTree(ex) +
                                " in " + e.destMethod + "\n" + ex);
                            if ("stack" in ex)
                                dd(ex.stack);
                        }
                    }
                else
                    destObject[e.destMethod] (e);

                if (count++ > this.MAX_EVENT_DEPTH)
                    throw "Too many events in chain";
                break;

            case "undefined":
                //dd ("** " + e.destMethod + " does not exist.");
                break;

            default:
                dd ("** " + e.destMethod + " is not a function.");
        }

        if ((e.type != "event-end") && (!e.destObject))
        {
            e.lastSet = e.set;
            e.set = "eventpump";
            e.lastType = e.type;
            e.type = "event-end";
            e.destMethod = "onEventEnd";
            e.destObject = this;
        }

    }

    delete this.currentEvent;

    return true;
    
}

CEventPump.prototype.stepEvents = 
function ep_stepevents()
{
    var i = 0;
    
    while (i < this.eventsPerStep)
    {
        var e = this.queue.pop();
        if (!e || e.type == "yield")
            break;
        this.routeEvent (e);
        i++;
    }

    return true;
    
}
