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

function initHandlers()
{
    function wwObserve (subject, topic, data)
    {
        //dd ("wwObserver::Observe " + subject + ", " + topic);
        if (topic == "domwindowopened")
            console.onWindowOpen (subject);
        else
            console.onWindowClose (subject);
    };

    console.wwObserver = {observe: wwObserve};
    console.windowWatcher.registerNotification (console.wwObserver);
    console.hookedWindows = new Array();

    var enumerator = console.windowWatcher.getWindowEnumerator();
    while (enumerator.hasMoreElements())
    {
        var win = enumerator.getNext();
        if (!isWindowFiltered(win))
        {
            console.onWindowOpen(win);
            console.onWindowLoad();
        }
    }
}

function destroyHandlers()
{
    console.windowWatcher.unregisterNotification (console.wwObserver);
    while (console.hookedWindows.length)
    {
        var win = console.hookedWindows.pop();
        win.removeEventListener ("load", console.onWindowLoad, false);
        win.removeEventListener ("unload", console.onWindowUnload, false);
    }
}

function isWindowFiltered (window)
{
    var href = window.location.href;
    var rv = ((href.search (/^chrome:\/\/venkman\//) != -1 &&
              href.search (/test/) == -1) ||
              (console.prefs["enableChromeFilter"] &&
               href.search (/navigator.xul($|\?)/) == -1));
    //dd ("isWindowFiltered " + window.location.href + ", returning " + rv);
    return rv;
}

console.onWindowOpen = 
function con_winopen (win)
{
    if ("ChromeWindow" in win && win instanceof win.ChromeWindow &&
        (win.location.href == "about:blank" || win.location.href == ""))
    {
        setTimeout (con_winopen, 0, win);
        return;
    }   

    if (isWindowFiltered(win))
        return;
    
    //dd ("window opened: " + win + " ``" + win.location + "''");
    console.hookedWindows.push(win);
    dispatch ("hook-window-opened", {window: win});
    win.addEventListener ("load", console.onWindowLoad, false);
    win.addEventListener ("unload", console.onWindowUnload, false);
    //console.scriptsView.freeze();
}

console.onWindowLoad =
function con_winload (e)
{
    dispatch ("hook-window-loaded", {event: e});
}

console.onWindowUnload =
function con_winunload (e)
{
    dispatch ("hook-window-unloaded", {event: e});
    //    dd (dumpObjectTree(e));
}

console.onWindowClose =
function con_winclose (win)
{
    if (isWindowFiltered(win))
        return;

    //dd ("window closed: " + win + " ``" + win.location + "''");
    var i = arrayIndexOf(console.hookedWindows, win);
    ASSERT (i != console.hookedWindows.length,
            "WARNING: Can't find hook window for closed window " + i + ".");
    arrayRemoveAt(console.hookedWindows, i);
    dispatch ("hook-window-closed", {window: win});
    //console.scriptsView.freeze();
}

console.onLoad =
function con_load (e)
{
    var ex;
    
    dd ("Application venkman, 'JavaScript Debugger' loaded.");

    try
    {
        init();
    }
    catch (ex)
    {
        if ("bundleList" in console)
            window.alert (getMsg (MSN_ERR_STARTUP, formatException(ex)));
        else
            window.alert (formatException(ex));
        console.startupException = ex;
    }
}

console.onClose =
function con_onclose (e)
{
    dd ("onclose");
    
    if (typeof console != "object" || "startupException" in console)
        return true;
    
    dd ("onclose: dispatching");
    
    return dispatch ("hook-venkman-query-exit");
}

console.onUnload =
function con_unload (e)
{
    dd ("Application venkman, 'JavaScript Debugger' unloading.");

    if (typeof console != "object")
        return;

    dispatch ("hook-venkman-exit");
    destroy();
}

console.onMouseOver =
function con_mouseover (e)
{
    var element = e.originalTarget;
    if (!("_lastElement" in console))
        console._lastElement = null;
    
    while (element)
    {
        if (element == console._lastElement)
            return;
        
        if ("getAttribute" in element)
        {
            var status = element.getAttribute ("venkmanstatustext");
            if (status)
            {
                console.status = status;
                console._lastElement = element;
                return;
            }
        }
        
        if ("localName" in element && element.localName == "floatingview")
        {
            console.status = console.viewManager.computeLocation (element);
            console._lastElement = element;
            return;
        }

        element = element.parentNode;
    }
}

window.onresize =
function ()
{
    dispatch ("hook-window-resized", { window: window });
    //  console.scrollDown();
}

