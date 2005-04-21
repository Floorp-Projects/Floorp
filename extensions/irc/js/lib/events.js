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
    /* When there are this many 'used' items in a queue, always clean up. At
     * this point it is MUCH more effecient to remove a block than a single
     * item (i.e. removing 1000 is much much faster than removing 1 item 1000
     * times [1]).
     */
    this.FORCE_CLEANUP_PTR = 1000;
    /* If there are less than this many items in a queue, clean up. This keeps
     * the queue empty normally, and is not that ineffecient [1].
     */
    this.MAX_AUTO_CLEANUP_LEN = 100;
    this.eventsPerStep = eventsPerStep;
    this.queue = new Array();
    this.queuePointer = 0;
    this.bulkQueue = new Array();
    this.bulkQueuePointer = 0;
    this.hooks = new Array();

    /* [1] The delay when removing items from an array (with unshift or splice,
     * and probably most operations) is NOT perportional to the number of items
     * being removed, instead it is proportional to the number of items LEFT.
     * Because of this, it is better to only remove small numbers of items when
     * the queue is small (MAX_AUTO_CLEANUP_LEN), and when it is large remove
     * only large chunks at a time (FORCE_CLEANUP_PTR), reducing the number of
     * resizes being done.
     */
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
    this.queue.push(e);
    return true;
}

CEventPump.prototype.addBulkEvent =
function ep_addevent (e)
{
    e.queuedAt = new Date();
    this.bulkQueue.push(e);
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
    var st, en, e;

    st = new Date();
    while (i < this.eventsPerStep)
    {
        if (this.queuePointer >= this.queue.length)
            break;

        e = this.queue[this.queuePointer++];

        if (e.type == "yield")
            break;

        this.routeEvent(e);
        i++;
    }
    while (i < this.eventsPerStep)
    {
        if (this.bulkQueuePointer >= this.bulkQueue.length)
            break;

        e = this.bulkQueue[this.bulkQueuePointer++];

        if (e.type == "yield")
            break;

        this.routeEvent(e);
        i++;
    }
    en = new Date();

    // i == number of items handled this time.
    // We only want to do this if we handled at least 25% of our step-limit
    // and if we have a sane interval between st and en (not zero).
    if ((i * 4 >= this.eventsPerStep) && (en - st > 0))
    {
        // Calculate the number of events that can be processed in 400ms.
        var newVal = (400 * i) / (en - st);

        // If anything skews it majorly, limit it to a minimum value.
        if (newVal < 10)
            newVal = 10;

        // Adjust the step-limit based on this "target" limit, but only do a 
        // 25% change (underflow filter).
        this.eventsPerStep += Math.round((newVal - this.eventsPerStep) / 4);
    }

    // Clean up if we've handled a lot, or the queue is small.
    if ((this.queuePointer >= this.FORCE_CLEANUP_PTR) ||
        (this.queue.length <= this.MAX_AUTO_CLEANUP_LEN))
    {
        this.queue.splice(0, this.queuePointer);
        this.queuePointer = 0;
    }

    // Clean up if we've handled a lot, or the queue is small.
    if ((this.bulkQueuePointer >= this.FORCE_CLEANUP_PTR) ||
        (this.bulkQueue.length <= this.MAX_AUTO_CLEANUP_LEN))
    {
        this.bulkQueue.splice(0, this.bulkQueuePointer);
        this.bulkQueuePointer = 0;
    }

    return true;

}
