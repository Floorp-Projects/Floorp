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
    console.windows.hookedWindows = new Array();

    var enumerator = console.windowWatcher.getWindowEnumerator();
    while (enumerator.hasMoreElements())
    {
        var win = enumerator.getNext();
        if (win.location.href != "chrome://venkman/content/venkman.xul")
        {
            console.onWindowOpen(win);
            console.onWindowLoad();
        }
    }
}

function destroyHandlers()
{
    console.windowWatcher.unregisterNotification (console.wwObserver);
    while (console.windows.hookedWindows.length)
    {
        var win = console.windows.hookedWindows.pop();
        win.removeEventListener ("load", console.onWindowLoad, false);
        win.removeEventListener ("unload", console.onWindowUnload, false);
    }
}

console.onWindowOpen =
function con_winopen (win)
{
    if ("ChromeWindow" in win && win instanceof win.ChromeWindow &&
        (win.location.href == "about:blank" || win.location.href == ""))
    {
        //dd ("not loaded yet?");
        setTimeout (con_winopen, 100, win);
        return;
    }
    
    //dd ("window opened: " + win); // + ", " + getInterfaces(win));
    console.windows.appendChild (new WindowRecord(win, ""));
    console.windows.hookedWindows.push(win);
    win.addEventListener ("load", console.onWindowLoad, false);
    win.addEventListener ("unload", console.onWindowUnload, false);
    console.scriptsView.freeze();
}

console.onWindowLoad =
function con_winload (e)
{
    console.scriptsView.thaw();
}

console.onWindowUnload =
function con_winunload (e)
{
    console.scriptsView.thaw();
}

console.onWindowClose =
function con_winunload (win)
{
    //dd ("window closed: " + win);
    if (win.location.href != "chrome://venkman/content/venkman.xul")
    {
        var winRecord = console.windows.locateChildByWindow(win);
        if (!ASSERT(winRecord, "onWindowClose: Can't find window record."))
            return;
        console.windows.removeChildAtIndex(winRecord.childIndex);
        var idx = arrayIndexOf(console.windows.hookedWindows, win);
        if (idx != -1)
            arrayRemoveAt(console.windows.hookedWindows, idx);
    }
    console.scriptsView.freeze();
}

console.onDebugTrap =
function con_ondt ()
{
    var frame = setCurrentFrameByIndex(0);
    var type = console.trapType;

    var frameRec = console.stackView.stack.childData[0];
    console.pushStatus (getMsg(MSN_STATUS_STOPPED, [frameRec.functionName,
                                                    frameRec.location]));
    if (type != jsdIExecutionHook.TYPE_INTERRUPTED ||
        console._lastStackDepth != console.frames.length)
    {
        display (formatFrame(frame));
    }

    displaySource (frame.script.fileName, frame.line, 
                   (type == jsdIExecutionHook.TYPE_INTERRUPTED) ? 0 : 2);
    
    console._lastStackDepth = console.frames.length;

    console.stackView.restoreState();
    enableDebugCommands()

}

console.onDebugContinue =
function con_ondc ()
{
    console.popStatus();
    console.sourceView.tree.invalidate();
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
        window.alert (getMsg (MSN_ERR_STARTUP, formatException(ex)));
    }
}

console.onClose =
function con_onclose (e)
{
    if (typeof console == "object" && "frames" in console)
    {
        if (confirm(MSG_QUERY_CLOSE))
        {
            console.__exitAfterContinue__ = true;
            dispatch ("cont");
        }
        return false;
    }

    return true;
}

console.onUnload =
function con_unload (e)
{
    dd ("Application venkman, 'JavaScript Debugger' unloading.");

    destroy();
    return true;
}

console.onFrameChanged =
function con_fchanged (currentFrame, currentFrameIndex)
{
    var stack = console.stackView.stack;
    
    if (currentFrame)
    {
        if (currentFrame.isNative)
            return;
        
        var frameRecord = stack.childData[currentFrameIndex];
        var vr = frameRecord.calculateVisualRow();
        console.stackView.selectedIndex = vr;
        console.stackView.scrollTo (vr, 0);
        var containerRec = console.scripts[currentFrame.script.fileName];
        if (containerRec)
        {
            dispatch ("find-url-soft", {url: containerRec.fileName,
                      rangeStart: currentFrame.script.baseLineNumber,
                      rangeEnd: currentFrame.script.baseLineNumber + 
                                currentFrame.script.lineExtent - 1,
                      lineNumber: currentFrame.line});
        }
        else
        {
            dd ("frame from unknown source");
        }
    }
    else
    {
        stack.close();
        stack.childData = new Array();
        stack.hide();
    }
}

console.onInputCompleteLine =
function con_icline (e)
{    
    if (console.inputHistory.length == 0 || console.inputHistory[0] != e.line)
        console.inputHistory.unshift (e.line);
    
    if (console.inputHistory.length > console.prefs["input.history.max"])
        console.inputHistory.pop();
    
    console.lastHistoryReferenced = -1;
    console.incompleteLine = "";
    
    var ev = {isInteractive: true, initialEvent: e};
    dispatch (e.line, ev, CMD_CONSOLE);
}

console.onSingleLineKeypress =
function con_slkeypress (e)
{
    var w;
    var newOfs;
    
    switch (e.keyCode)
    {
        case 13:
            if (!e.target.value)
                return;
            e.line = e.target.value;
            console.onInputCompleteLine (e);
            e.target.value = "";
            break;

        case 38: /* up */
            if (console.lastHistoryReferenced == -2)
            {
                console.lastHistoryReferenced = -1;
                e.target.value = console.incompleteLine;
            }
            else if (console.lastHistoryReferenced <
                     console.inputHistory.length - 1)
            {
                e.target.value =
                    console.inputHistory[++console.lastHistoryReferenced];
            }
            break;

        case 40: /* down */
            if (console.lastHistoryReferenced > 0)
                e.target.value =
                    console.inputHistory[--console.lastHistoryReferenced];
            else if (console.lastHistoryReferenced == -1)
            {
                e.target.value = "";
                console.lastHistoryReferenced = -2;
            }
            else
            {
                console.lastHistoryReferenced = -1;
                e.target.value = console.incompleteLine;
            }
            break;
            
        case 33: /* pgup */
            w = window.frames[0];
            newOfs = w.pageYOffset - (w.innerHeight / 2);
            if (newOfs > 0)
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, 0);
            break;
            
        case 34: /* pgdn */
            w = window.frames[0];
            newOfs = w.pageYOffset + (w.innerHeight / 2);
            if (newOfs < (w.innerHeight + w.pageYOffset))
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, (w.innerHeight + w.pageYOffset));
            break;

        case 9: /* tab */
            e.preventDefault();
            console.onTabCompleteRequest(e);
            break;       

        default:
            console.lastHistoryReferenced = -1;
            console.incompleteLine = e.target.value;
            break;
    }
}

console.onProjectSelect =
function con_projsel (e)
{
    if (console.projectView.selectedIndex == -1)
        return;
    
    console.scriptsView.selectedIndex = -1;
    console.stackView.selectedIndex   = -1;
    console.sourceView.selectedIndex  = -1;
    
    var rowIndex = console.projectView.selectedIndex;
    if (rowIndex == -1 || rowIndex > console.projectView.rowCount)
        return;
    var row =
        console.projectView.childData.locateChildByVisualRow(rowIndex);
    if (!row)
    {
        ASSERT (0, "bogus row index " + rowIndex);
        return;
    }

    if (row instanceof BPRecord)
        dispatch ("find-bp", {breakpointRec: row});
    else if (row instanceof FileRecord || row instanceof WindowRecord)
        dispatch ("find-url", {url: row.url});
}
            
console.onStackSelect =
function con_stacksel (e)
{
    if (console.stackView.selectedIndex == -1)
        return;

    console.scriptsView.selectedIndex = -1;
    console.projectView.selectedIndex = -1;
    console.sourceView.selectedIndex  = -1;

    var rowIndex = console.stackView.selectedIndex;
    if (rowIndex == -1 || rowIndex > console.stackView.rowCount)
        return;
    var row =
        console.stackView.childData.locateChildByVisualRow(rowIndex);
    if (!row)
    {
        ASSERT (0, "bogus row index " + rowIndex);
        return;
    }

    var source;
    
    var sourceView = console.sourceView;
    if (row instanceof FrameRecord)
    {
        dispatch ("frame", {frameIndex: row.childIndex});
    }
    else if (row instanceof ValueRecord && row.jsType == jsdIValue.TYPE_OBJECT)
    {
        if (row.parentRecord instanceof FrameRecord &&
            row == row.parentRecord.scopeRec)
            return;
        dispatch ("find-creator", {jsdValue: row.value});
    }
}

console.onScriptSelect =
function con_scptsel (e)
{
    if (console.scriptsView.selectedIndex == -1)
        return;

    console.projectView.selectedIndex = -1;
    console.stackView.selectedIndex   = -1;
    console.sourceView.selectedIndex  = -1;

    var rowIndex = console.scriptsView.selectedIndex;

    if (rowIndex == -1 || rowIndex > console.scriptsView.rowCount)
    {
        dd ("row out of bounds");
        return;
    }
    
    var row =
        console.scriptsView.childData.locateChildByVisualRow(rowIndex);
    ASSERT (row, "bogus row");

    if (row instanceof ScriptRecord)
        dispatch ("find-script", {scriptRec: row});
    else if (row instanceof ScriptContainerRecord)
        dispatch ("find-url", {url: row.fileName});
}

console.onScriptClick =
function con_scptclick (e)
{
    if (e.originalTarget.localName == "treecol")
    {
        /* resort by column */
        var rowIndex = new Object();
        var colID = new Object();
        var childElt = new Object();
        
        var obo = console.scriptsView.tree;
        obo.getCellAt(e.clientX, e.clientY, rowIndex, colID, childElt);
        var prop;
        switch (colID.value)
        {
            case "script-name":
                prop = "functionName";
                break;
            case "script-line-start":
                prop = "baseLineNumber";
                break;
            case "script-line-extent":
                prop = "lineExtent";
                break;
        }

        var scriptsRoot = console.scriptsView.childData;
        var dir = (prop == scriptsRoot._share.sortColumn) ?
            scriptsRoot._share.sortDirection * -1 : 1;
        dd ("sort direction is " + dir);
        scriptsRoot.setSortColumn (prop, dir);
    }
}

console.onSourceSelect =
function con_sourcesel (e)
{
    if (console.sourceView.selectedIndex == -1)
        return;
    
    console.scriptsView.selectedIndex = -1;
    console.stackView.selectedIndex   = -1;
    console.projectView.selectedIndex = -1;
}

console.onSourceClick =
function con_sourceclick (e)
{
    var target = e.originalTarget;
    
    if (target.localName == "treechildren")
    {
        var row = new Object();
        var colID = new Object();
        var childElt = new Object();
        
        var tree = console.sourceView.tree;
        tree.getCellAt(e.clientX, e.clientY, row, colID, childElt);
        if (row.value == -1)
          return;
        
        colID = colID.value;
        row = row.value;
        
        if (colID == "breakpoint-col")
        {
            if ("onMarginClick" in console.sourceView.childData)
                console.sourceView.childData.onMarginClick (e, row + 1);
        }
    }

}

console.onTabCompleteRequest =
function con_tabcomplete (e)
{
    var selStart = e.target.selectionStart;
    var selEnd   = e.target.selectionEnd;            
    var v        = e.target.value;
    
    if (selStart != selEnd) 
    {
        /* text is highlighted, just move caret to end and exit */
        e.target.selectionStart = e.target.selectionEnd = v.length;
        return;
    }

    var firstSpace = v.indexOf(" ");
    if (firstSpace == -1)
        firstSpace = v.length;

    var pfx;
    var d;
    
    if ((selStart <= firstSpace))
    {
        /* The cursor is positioned before the first space, so we're completing
         * a command
         */
        var partialCommand = v.substring(0, firstSpace).toLowerCase();
        var cmds = console.commandManager.listNames(partialCommand, CMD_CONSOLE);

        if (!cmds)
            /* partial didn't match a thing */
            display (getMsg(MSN_NO_CMDMATCH, partialCommand), MT_ERROR);
        else if (cmds.length == 1)
        {
            /* partial matched exactly one command */
            pfx = cmds[0];
            if (firstSpace == v.length)
                v =  pfx + " ";
            else
                v = pfx + v.substr (firstSpace);
            
            e.target.value = v;
            e.target.selectionStart = e.target.selectionEnd = pfx.length + 1;
        }
        else if (cmds.length > 1)
        {
            /* partial matched more than one command */
            d = new Date();
            if ((d - console._lastTabUp) <= console.prefs["input.dtab.time"])
                display (getMsg (MSN_CMDMATCH,
                                 [partialCommand, cmds.join(MSG_COMMASP)]));
            else
                console._lastTabUp = d;
            
            pfx = getCommonPfx(cmds);
            if (firstSpace == v.length)
                v =  pfx;
            else
                v = pfx + v.substr (firstSpace);
            
            e.target.value = v;
            e.target.selectionStart = e.target.selectionEnd = pfx.length;
            
        }
                
    }

}

window.onresize =
function ()
{
    console.scrollDown();
}

