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

function formatRecord (rec, indent)
{
    var str = "";
    
    for (var i in rec._colValues)
        str += rec._colValues[i] + ", ";
    
    str += "[";
    
    str += rec.calculateVisualRow() + ", ";
    str += rec.childIndex + ", ";
    str += rec.level + ", ";
    str += rec.visualFootprint + ", ";
    str += rec.isContainerOpen + ", ";
    str += rec.isHidden + "]";
    
    dd (indent + str);
}

function formatBranch (rec, indent)
{
    for (var i = 0; i < rec.childData.length; ++i)
    {
        formatRecord (rec.childData[i], indent);
        if (rec.childData[i].childData)
            formatBranch(rec.childData[i], indent + "  ");
    }
}

function dtree(tree)
{
    formatBranch(tree, "");
}

function initOutliners()
{
    const ATOM_CTRID = "@mozilla.org/atom-service;1";
    const nsIAtomService = Components.interfaces.nsIAtomService;

    var atomsvc =
        Components.classes[ATOM_CTRID].getService(nsIAtomService);

    console.sourceView.atomCurrent = atomsvc.getAtom("current-line");
    console.sourceView.atomBreakpoint = atomsvc.getAtom("breakpoint");
    console.sourceView.atomFunctionStart = atomsvc.getAtom("func-start");
    console.sourceView.atomFunctionLine = atomsvc.getAtom("func-line");
    console.sourceView.atomFunctionEnd = atomsvc.getAtom("func-end");
    console.sourceView.atomFunctionAfter = atomsvc.getAtom("func-after");

    var outliner = document.getElementById("source-outliner");
    outliner.outlinerBoxObject.view = console.sourceView;

    console.scriptsView.childData.setSortColumn("baseLineNumber");
    console.scriptsView.groupFiles  = true;
    console.scriptsView.atomUnknown = atomsvc.getAtom("ft-unk");
    console.scriptsView.atomHTML    = atomsvc.getAtom("ft-html");
    console.scriptsView.atomJS      = atomsvc.getAtom("ft-js");
    console.scriptsView.atomXUL     = atomsvc.getAtom("ft-xul");
    console.scriptsView.atomXML     = atomsvc.getAtom("ft-xml");
    console.scriptsView.atomGuessed = atomsvc.getAtom("fn-guessed");
    console.scriptsView.atomBreakpoint = atomsvc.getAtom("has-bp");

    outliner = document.getElementById("script-list-outliner");
    outliner.outlinerBoxObject.view = console.scriptsView;

    outliner = document.getElementById("stack-outliner");
    outliner.outlinerBoxObject.view = console.stackView;

    console.stackView.atomStack    = atomsvc.getAtom("w-stack");
    console.stackView.atomFrame    = atomsvc.getAtom("w-frame");
    console.stackView.atomVoid     = atomsvc.getAtom("w-void");
    console.stackView.atomNull     = atomsvc.getAtom("w-null");
    console.stackView.atomBool     = atomsvc.getAtom("w-bool");
    console.stackView.atomInt      = atomsvc.getAtom("w-int");
    console.stackView.atomDouble   = atomsvc.getAtom("w-double");
    console.stackView.atomString   = atomsvc.getAtom("w-string");
    console.stackView.atomFunction = atomsvc.getAtom("w-function");
    console.stackView.atomObject   = atomsvc.getAtom("w-object");

    console.stackView.stack.property = console.stackView.atomStack;
    console.stackView.stack.reserveChildren();
    console.stackView.childData.appendChild (console.stackView.stack);
    console.stackView.stack.hide();

    outliner = document.getElementById("project-outliner");
    outliner.outlinerBoxObject.view = console.projectView;
    
    console.projectView.atomBlacklist   = atomsvc.getAtom("pj-blacklist");
    console.projectView.atomBLItem      = atomsvc.getAtom("pj-bl-item");
    console.projectView.atomBreakpoints = atomsvc.getAtom("pj-breakpoints");
    console.projectView.atomBreakpoint  = atomsvc.getAtom("pj-breakpoint");
    
    console.blacklist.property = console.projectView.atomBlacklist;
    console.blacklist.reserveChildren();
    //console.projectView.childData.appendChild (console.blacklist);

    console.breakpoints.property = console.projectView.atomBreakpoints;
    console.breakpoints.reserveChildren();
    console.projectView.childData.appendChild (console.breakpoints);

    BLRecord.prototype.property = console.projectView.atomBreakpoint;
    BPRecord.prototype.property = console.projectView.atomBLItem;
    
}

function destroyOutliners()
{
    console.sourceView.outliner.view = null;
    console.scriptsView.outliner.view = null;
    console.stackView.outliner.view = null;
    console.projectView.outliner.view = null;
}

console.sourceView = new BasicOView();

console.sourceView._scrollTo = BasicOView.prototype.scrollTo;

console.sourceView.scrollTo =
function sv_scrollto (line, align)
{
    if (!("childData" in this) || !this.childData.isLoaded)
    {
        /* the source hasn't been loaded yet, store line/align for processing
         * when the load is done. */
        this.pendingScroll = [line, align];
        return;
    }
    this._scrollTo(line, align);
}

/*
 * pass in a ScriptContainer to be displayed on this outliner
 */
console.sourceView.displaySource =
function sov_dsource (source)
{
    if ("childData" in this && source == this.childData)
        return;
    
    function tryAgain (result)
    {
        if (result == Components.results.NS_OK)
            console.sourceView.displaySource(source);
        else
        {
            dd ("source load failed: '" + source.fileName + "'");
        }
    }

    /* save the current position before we change to another source */
    if ("childData" in this)
        this.childData.lastTopRow = this.outliner.getFirstVisibleRow() + 1;
    
    if (!source)
    {
        delete this.childData;
        this.rowCount = 0;
        this.outliner.rowCountChanged(0, 0);
        this.outliner.invalidate();
        return;
    }
    
    /* if the source for this record isn't loaded yet, load it and call ourselves
     * back after */
    if (!source.isLoaded)
    {
        disableReloadCommand();
        /* clear the view while we wait for the source */
        delete this.childData;
        this.rowCount = 0;
        this.outliner.rowCountChanged(0, 0);
        this.outliner.invalidate();
        /* load the source, call the tryAgain function when it's done. */
        source.loadSource(tryAgain);
        return;
    }

    enableReloadCommand();
    this.childData = source;    
    this.rowCount = source.sourceText.length;
    this.tabString = leftPadString ("", source.tabWidth, " ");
    this.outliner.rowCountChanged(0, this.rowCount);
    this.outliner.invalidate();

    var hdr = document.getElementById("source-line-text");
    hdr.setAttribute ("label", source.fileName);

    if ("pendingScroll" in this)
    {
        this.scrollTo (this.pendingScroll[0], this.pendingScroll[1]);
        delete this.pendingScroll;
    }
    
    if ("pendingSelect" in this)
    {
        console.sourceView.selection.timedSelect (this.pendingSelect, 500);
        delete this.pendingSelect;
    }
}

/*
 * "soft" scroll to a line number in the current source.  soft, in this
 * case, means that if the target line somewhere in the center of the
 * source view already, then we can just exit.  otherwise, we'll center on the
 * target line.  this is used when single stepping through source, when constant
 * one-line scrolls would be distracting.
 *
 * the line parameter is one based.
 */
console.sourceView.softScrollTo =
function sv_lscroll (line)
{
    if (!("childData" in this) || !this.childData.isLoaded)
    {
        /* the source hasn't been loaded yet, queue the scroll for later. */
        this.pendingScroll = [line, 0];
        return;
    }

    var first = this.outliner.getFirstVisibleRow();
    var last = this.outliner.getLastVisibleRow();
    var fuzz = 2;
    if (line < (first + fuzz) || line > (last - fuzz))
        this.scrollTo (line, 0);
}    

/* nsIOutlinerView */
console.sourceView.getRowProperties =
function sv_rowprops (row, properties)
{
    if (row == console.stopLine - 1 && 
        console.stopFile == this.childData.fileName)
    {
        properties.AppendElement(this.atomCurrent);
    }

}

/* nsIOutlinerView */
console.sourceView.getCellProperties =
function sv_cellprops (row, colID, properties)
{
    if (!this.childData.isLoaded)
        return;

    var line = this.childData.sourceText[row];
    if (!line)
        return;
    
    if (colID == "breakpoint-col" && 
        this.childData.sourceText[row].bpRecord)
        properties.AppendElement(this.atomBreakpoint);
    
    if (line.functionLine)
    {
        properties.AppendElement(this.atomFunctionLine);
        if (line.functionStart)
            properties.AppendElement(this.atomFunctionStart);
        if (linefunctionEnd)
            properties.AppendElement(this.atomFunctionEnd);
    } else if (row > 0 && this.childData.sourceText[row - 1].functionEnd)
        properties.AppendElement(this.atomFunctionAfter);
}

/* nsIOutlinerView */
console.sourceView.getCellText =
function sv_getcelltext (row, colID)
{    
    if (!this.childData.isLoaded || 
        row < 0 || row > this.childData.sourceText.length)
        return "";
    
    switch (colID)
    {
        case "source-line-text":
            return this.childData.sourceText[row].replace(/\t/g, this.tabString);

        case "source-line-number":
            return row + 1;
            
        default:
            return "";
    }
}

var scriptShare = new Object();

function SourceRecord(fileName)
{
    this.setColumnPropertyName ("script-name", "displayName");
    this.fileName = fileName;
    this.tabWidth = console.prefs["sourcetext.tab.width"];
    var sov = console.scriptsView;
    this.fileType = sov.atomUnknown;
    this.shortName = this.fileName;
    this.group = 4;
    this.bpcount = 0;

    var ary = this.fileName.match(/\/([^\/?]+)(\?|$)/);
    if (ary)
    {
        this.shortName = ary[1];
        ary = this.shortName.match (/\.(js|html|xul|xml)$/i);
        if (ary)
        {
            switch (ary[1].toLowerCase())
            {
                case "js":
                    this.fileType = sov.atomJS;
                    this.group = 0;
                    break;
                    
                case "html":
                    this.group = 1;
                    this.fileType = sov.atomHTML;
                    break;
                    
                case "xul":
                    this.group = 2;
                    this.fileType = sov.atomXUL;
                    break;
                    
                case "xml":
                    this.group = 3;
                    this.fileType = sov.atomXML;
                    break;
            }
        }
    }
    
    this.displayName = this.shortName;
}

SourceRecord.prototype = new TreeOViewRecord(scriptShare);

SourceRecord.prototype.isLoaded = false;

SourceRecord.prototype.sortCompare =
function sr_compare (a, b)
{
    if (console.scriptsView.groupFiles)
    {
        if (a.group < b.group)
            return -1;
    
        if (a.group > b.group)
            return 1;
    }
    
    if (a.displayName < b.displayName)
        return -1;

    if (a.displayName > b.displayName)
        return 1;
    
    return 0;
}

SourceRecord.prototype.locateChildByScript =
function sr_locate (script)
{
    for (var i = 0; i < this.childData.length; ++i)
        if (script == this.childData[i].script)
            return this.childData[i];

    return null;
}

SourceRecord.prototype.makeCurrent =
function sr_makecur ()
{
    if (!("childData" in console.sourceView) ||
        console.sourceView.childData != this)
    {
        console.sourceView.displaySource(this);
        if ("lastTopRow" in this)
            console.sourceView.scrollTo (this.lastTopRow, -1);
        else
            console.sourceView.scrollTo (0, -1);
    }
}

SourceRecord.prototype.reloadSource =
function sr_reloadsrc (cb)
{
    var sourceRec = this;
    var needRedisplay = (console.sourceView.childData == this);
    var topLine = (!needRedisplay) ? 0 :
        console.sourceView.outliner.getFirstVisibleRow() + 1;
    
    function reloadCB (status)
    {
        if (status == Components.results.NS_OK && needRedisplay)
        {
            console.sourceView.displaySource(sourceRec);
            console.sourceView.scrollTo(topLine);
        }
        if (typeof cb == "function")
            cb(status);
    }

    if (needRedisplay)
    {
        console.sourceView.displaySource(null);
    }
    
    if (this.isLoaded)
    {
        this.isLoaded = false;
        this.loadSource(reloadCB);
    }
    else
    {
        this.loadSource(cb);
    }
}

SourceRecord.prototype.loadSource =
function sr_loadsrc (cb)
{
    if (this.isLoaded)
    {
        /* if we're loaded, callback right now, and return. */
        cb (Components.results.NS_OK);
        return;
    }
    if ("isLoading" in this)
    {
        /* if we're in the process of loading, make a note of the callback, and
         * return. */
        if (!("extraCallbacks" in this))
            this.extraCallbacks = new Array();
        this.extraCallbacks.push (cb);
        return;
    }
    
    var observer = {
        onComplete: function oncomplete (data, url, status) {
            function callall (status)
            {
                cb (status);
                while (sourceRec.extraCallbacks)
                {
                    cb = sourceRec.extraCallbacks.pop();
                    cb (status);
                    if (sourceRec.extraCallbacks.length < 1)
                        delete sourceRec.extraCallbacks;
                }
            }
            
            delete this.isLoading;
            
            if (status != Components.results.NS_OK)
            {
                callall (status);
                return;
            }
            
            sourceRec.isLoaded = true;
            var ary = data.split(/\r\n|\n|\r/m);
            for (var i = 0; i < ary.length; ++i)
            {
                /* We use "new String" here so we can decorate the source line
                 * with attributes used while displaying the source, like
                 * "breakpoint", "breakable", and "warning".
                 * The replace() strips control characters, we leave the tabs in
                 * so we can expand them to a per-file width before actually
                 * displaying them.
                 */
                ary[i] = new String(ary[i].replace(/[\x0-\x8]|[\xA-\x1A]/g, ""));
            }
            sourceRec.sourceText = ary;
            ary = ary[0].match (/tab-?width*:\s*(\d+)/i);
            if (ary)
                sourceRec.tabWidth = ary[1];

            for (i = 0; i < sourceRec.childData.length; ++i)
            {
                sourceRec.childData[i].guessFunctionName();
            }

            for (i = 0; i < console.breakpoints.childData.length; ++i)
            {
                var bpr = console.breakpoints.childData[i];
                if (bpr.fileName == sourceRec.fileName &&
                    sourceRec.sourceText[bpr.line - 1])
                {
                    if (bpr.functionName == "anonymous" &&
                        bpr.scriptRecords[0])
                    {
                        bpr.functionName = bpr.scriptRecords[0].functionName;
                        bpr.invalidate();
                    }    
                    sourceRec.sourceText[bpr.line - 1].bpRecord = bpr;
                }
            }
            
            console.scriptsView.outliner.invalidate();
            callall(status);
        }
    };

    var ex;
    var sourceRec = this;
    this.isLoading = true;
    try
    {
        var src = loadURLNow(this.fileName);
        observer.onComplete (src, url, Components.results.NS_OK);
    }
    catch (ex)
    {
        /* if we can't load it now, try to load it later */
        loadURLAsync (this.fileName, observer);
    }

    delete this.isLoading;
}

SourceRecord.prototype.setFullNameMode =
function scr_setmode (flag)
{
    if (flag)
        this.displayName = this.fileName;
    else
        this.displayName = this.shortName;
}

function ScriptRecord(script) 
{
    if (!(script instanceof jsdIScript))
        throw new BadMojo (ERR_INVALID_PARAM, "value");

    this.setColumnPropertyName ("script-name", "functionName");
    this.setColumnPropertyName ("script-line-start", "baseLineNumber");
    this.setColumnPropertyName ("script-line-extent", "lineExtent");
    this.functionName = (script.functionName) ? script.functionName :
        MSG_VAL_TLSCRIPT;
    this.baseLineNumber = script.baseLineNumber;
    this.lineExtent = script.lineExtent;
    this.script = script;
}

ScriptRecord.prototype = new TreeOViewRecord(scriptShare);

ScriptRecord.prototype.makeCurrent =
function sr_makecur ()
{
    console.sourceView.displaySource(this.parentRecord);
    console.sourceView.scrollTo (this.baseLineNumber - 2, -1);
    if (this.parentRecord.isLoaded)
        console.sourceView.selection.timedSelect (this.baseLineNumber - 1, 500);
    else
        console.sourceView.pendingSelect = this.baseLineNumber - 1;
}

ScriptRecord.prototype.containsLine =
function sr_containsl (line)
{
    if (this.script.baseLineNumber <= line && 
        this.script.baseLineNumber + this.script.lineExtent > line)
        return true;
    
    return false;
}

ScriptRecord.prototype.isLineExecutable =
function sr_isexecutable (line)
{
    var pc = this.script.lineToPc (line);
    return (line == this.script.pcToLine (pc));
}

ScriptRecord.prototype.__defineGetter__ ("bpcount", sr_getbpcount);
function sr_getbpcount ()
{
    if (!("_bpcount" in this))
        return 0;

    return this._bpcount;
}

ScriptRecord.prototype.__defineSetter__ ("bpcount", sr_setbpcount);
function sr_setbpcount (value)
{
    var delta;
    
    if ("_bpcount" in this)
    {
        if (value == this._bpcount)
            return value;
        delta = value - this._bpcount;
    }
    else
        delta = value;

    this._bpcount = value;
    this.invalidate();
    this.parentRecord.bpcount += delta;
    this.parentRecord.invalidate();
    return value;
}

ScriptRecord.prototype.guessFunctionName =
function sr_guessname ()
{
    if (this.functionName != "anonymous")
        return;
    var scanText = "";
    var targetLine = this.script.baseLineNumber;
    
    /* scan at most 3 lines before the function definition */
    switch (targetLine - 3)
    {
        case -2: /* target line is the first line, nothing before it */
            break;

        case -1: /* target line is the second line, one line before it */ 
            scanText = 
                String(this.parentRecord.sourceText[targetLine - 2]);
            break;
        case 0:  /* target line is the third line, two before it */
            scanText =
                String(this.parentRecord.sourceText[targetLine - 3]) + 
                String(this.parentRecord.sourceText[targetLine - 2]);
            break;            
        default: /* target line is the fourth or higher line, three before it */
            scanText += 
                String(this.parentRecord.sourceText[targetLine - 4]) + 
                String(this.parentRecord.sourceText[targetLine - 3]) +
                String(this.parentRecord.sourceText[targetLine - 2]);
            break;
    }

    scanText += String(this.parentRecord.sourceText[targetLine - 1]);
    
    scanText = scanText.substring(0, scanText.lastIndexOf ("function"));
    var ary = scanText.match (/(\w+)\s*[:=]\s*$/);
    if (ary)
    {
        this.functionName = getMsg(MSN_FMT_GUESSEDNAME, ary[1]);
        this.isGuessedName = true;
        var wv = console.stackView;
        if (wv.stack.childData)
        {
            /* if we've got a stack trace, search it to see if any frames
             * contain this script.  if so, update the function name */
            for (var i = 0; i < wv.stack.childData.length; ++i)
            {
                var cd = wv.stack.childData[i];
                if (cd.frame.script == this.script)
                {
                    cd.functionName = this.functionName;
                    wv.outliner.invalidateRow(cd.calculateVisualRow());
                }
            }
        }
    }
    else
        dd ("unable to guess function name based on text ``" + scanText + "''");
}            
    
console.scriptsView = new TreeOView(scriptShare);

console.scriptsView.fullNameMode = false;

console.scriptsView.setFullNameMode =
function scv_setmode (flag)
{
    this.fullNameMode = flag;
    for (var i = 0; i < this.childData.length; ++i)
        this.childData[i].setFullNameMode (flag);
}

console.scriptsView.getCellProperties =
function scv_getcprops (index, colID, properties)
{
    var row;
    if ((row = this.childData.locateChildByVisualRow (index, 0)))
    {
        if (row.fileType && colID == "script-name")
            properties.AppendElement (row.fileType);
        if (row.isGuessedName && colID == "script-name")
            properties.AppendElement (this.atomGuessed);
        if (row.bpcount > 0)
            properties.AppendElement (this.atomBreakpoint);
    }
}

var stackShare = new Object();

function FrameRecord (frame)
{
    if (!(frame instanceof jsdIStackFrame))
        throw new BadMojo (ERR_INVALID_PARAM, "value");

    this.setColumnPropertyName ("stack-col-0", "functionName");
    this.setColumnPropertyName ("stack-col-2", "location");

    var fn = frame.script.functionName;
    var sourceRec = console.scripts[frame.script.fileName];
    if (sourceRec)
    {
        this.location = sourceRec.shortName + ":" + frame.line;
        var scriptRec = sourceRec.locateChildByScript(frame.script);
        if (!scriptRec)
            dd ("no scriptrec");
        else if (fn == "anonymous")
            fn = scriptRec.functionName;
    }
    else
        dd ("no sourcerec");
    
    this.functionName = fn;
    this.frame = frame;
    this.reserveChildren();
    this.scopeRec = new ValueRecord (frame.scope, MSG_WORD_SCOPE, "");
    this.appendChild (this.scopeRec);
    this.thisRec = new ValueRecord (frame.thisValue, MSG_WORD_THIS, "");
    this.property = console.stackView.atomFrame;
    this.appendChild (this.thisRec);
}

FrameRecord.prototype = new TreeOViewRecord (stackShare);

function ValueRecord (value, name, flags)
{
    if (!(value instanceof jsdIValue))
        throw new BadMojo (ERR_INVALID_PARAM, "value", String(value));
 
    this.setColumnPropertyName ("stack-col-0", "displayName");
    this.setColumnPropertyName ("stack-col-1", "displayType");
    this.setColumnPropertyName ("stack-col-2", "displayValue");
    this.setColumnPropertyName ("stack-col-3", "displayFlags");    
    this.displayName = name;
    this.displayFlags = flags;
    this.value = value;
    this.refresh();
}

ValueRecord.prototype = new TreeOViewRecord (stackShare);

ValueRecord.prototype.hiddenFunctionCount = 0;
ValueRecord.prototype.showFunctions = false;

ValueRecord.prototype.resort =
function cr_resort()
{
    /*
     * we want to override the prototype's resort() method with this empty one
     * because we take care of the sorting business ourselves in onPreOpen()
     */
}

ValueRecord.prototype.refresh =
function vr_refresh ()
{
    var sizeDelta = 0;
    var lastType = this.jsType;
    this.jsType = this.value.jsType;
    
    if (0 && lastType != this.jsType && lastType == jsdIValue.TYPE_FUNCTION)
    {
        /* we changed from a non-function to a function */
        --this.hiddenFunctionCount;
        ++sizeDelta;
    }
    
    if (this.jsType != jsdIValue.TYPE_OBJECT && this.childData)
    {
        /* if we're not an object but we have child data, then we must have just
         * turned into something other than an object. */
        dd ("we're not an object anymore!");
        delete this.childData;
        this.isContainerOpen = false;
        sizeDelta = 1 - this.visualFootprint;
    }
    
    var wv = console.stackView;
    switch (this.jsType)
    {
        case jsdIValue.TYPE_VOID:
            this.displayValue = MSG_TYPE_VOID
            this.displayType  = MSG_TYPE_VOID;
            this.property     = wv.atomVoid;
            break;
        case jsdIValue.TYPE_NULL:
            this.displayValue = MSG_TYPE_NULL;
            this.displayType  = MSG_TYPE_NULL;
            this.property     = wv.atomNull;
            break;
        case jsdIValue.TYPE_BOOLEAN:
            this.displayValue = this.value.stringValue;
            this.displayType  = MSG_TYPE_BOOLEAN;
            this.property     = wv.atomBool;
            break;
        case jsdIValue.TYPE_INT:
            this.displayValue = this.value.intValue;
            this.displayType  = MSG_TYPE_INT;
            this.property     = wv.atomInt;
            break;
        case jsdIValue.TYPE_DOUBLE:
            this.displayValue = this.value.doubleValue;
            this.displayType  = MSG_TYPE_DOUBLE;
            this.property     = wv.atomDouble;
            break;
        case jsdIValue.TYPE_STRING:
            var strval = this.value.stringValue.quote();
            if (strval.length > MAX_STR_LEN)
                strval = getMsg(MSN_FMT_LONGSTR, strval.length);
            this.displayValue = strval;
            this.displayType  = MSG_TYPE_STRING;
            this.property     = wv.atomString;
            break;
        case jsdIValue.TYPE_FUNCTION:
            this.displayType  = MSG_TYPE_FUNCTION;
            this.displayValue = (this.value.isNative) ? MSG_WORD_NATIVE :
                MSG_WORD_SCRIPT;
            this.property = wv.atomFunction;
            break;
        case jsdIValue.TYPE_OBJECT:
            this.value.refresh();
            var ctor = this.value.jsClassName;
            if (ctor == "Object")
            {
                if (this.value.jsConstructor)
                    ctor = this.value.jsConstructor.jsFunctionName;
            }
            /*
            else if (ctor == "XPCWrappedNative_NoHelper")
            {
                ctor = MSG_CLASS_XPCOBJ;
            }
            */

            this.displayValue = "{" + ctor + ":" + this.value.propertyCount +
                "}";

            this.displayType = MSG_TYPE_OBJECT;
            this.property = wv.atomObject;
            /* if we had children, and were open before, then we need to descend
             * and refresh our children. */
            if ("childData" in this && this.childData.length > 0)
            {
                var rc = 0;
                rc = this.refreshChildren();
                sizeDelta += rc;
                //dd ("refreshChildren returned " + rc);
                this.visualFootprint += rc;
            }
            else
            {
                this.childData = new Array();
                this.isContainerOpen = false;
            }
            break;
            

        default:
            ASSERT (0, "invalid value");
    }

    //dd ("refresh returning " + sizeDelta);
    return sizeDelta;

}

ValueRecord.prototype.refreshChildren =
function vr_refreshkids ()
{
    /* XXX add identity check to see if we are a totally different object */
    /* if we now have more properties than we used to, we're going to have
     * to close any children we may have open, because we can't tell where the
     * new property is in any efficient way. */
    if (this.lastPropertyCount < this.value.propertyCount)
    {
        this.onPreOpen();
        return (this.childData.length + 1) - this.visualFootprint;
    }

    /* otherwise, we had children before.  we've got to update each of them
     * in turn. */
    var sizeDelta    = 0; /* total change in size */
    var idx          = 0; /* the new position of the child in childData */
    var deleteCount  = 0; /* number of children we've lost */
    var specialProps = 0; /* number of special properties in this object */

    for (var i = 0; i < this.childData.length; ++i)
    {
        //dd ("refreshing child #" + i);
        var name = this.childData[i]._colValues["stack-col-0"];
        var value;
        switch (name)
        {
            case MSG_VAL_PARENT:
                /* "special" property, doesn't actually exist
                 * on the object */
                value = this.value.jsParent;
                specialProps++;
                break;
            case MSG_VAL_PROTO:
                /* "special" property, doesn't actually exist
                 * on the object */
                value = this.value.jsPrototype;
                specialProps++;
                break;
            default:
                var prop = this.value.getProperty(name);
                if (prop)
                    value = prop.value;
                break;
        }
        
        if (value)
        {
            if (this.showFunctions || value.jsType != jsdIValue.TYPE_FUNCTION)
            {
                /* if this property still has a value, sync it in its (possibly)
                 * new position in the childData array, and refresh it */
                this.childData[idx] = this.childData[i];
                this.childData[idx].childIndex = idx;
                this.childData[idx].value = value;
                sizeDelta += this.childData[idx].refresh();
                ++idx;
                value = null;
            }
            else
            {
                /* if we changed from a non-function to a function, and we're in
                 * "hide function" mode, we need to consider this child deleted
                 */
                ++this.hiddenFunctionCount;
                ++deleteCount;
                sizeDelta -= this.childData[i].visualFootprint;
            }
        }
        else
        {
            /* if the property isn't here anymore, make a note of
             * it */
            ++deleteCount;
            sizeDelta -= this.childData[i].visualFootprint;
        }
    }
    
    /* if we've deleted some kids, adjust the length of childData to
     * match */
    if (deleteCount != 0)
        this.childData.length -= deleteCount;
    
    if ((this.childData.length + this.hiddenFunctionCount - specialProps) !=
        this.value.propertyCount)
    {
        /* if the two lengths *don't* match, then we *must* be in
         * a state where the user added and deleted the same
         * number of properties.  if this is the case, then
         * everything we just did was a totally
         * useless waste of time.  throw it out and re-init
         * whatever children we have.  see the "THESE COMMENTS"
         * comments above for the description of what we're doing
         * here. */
        this.onPreOpen();
        sizeDelta = (this.childData.length + 1) - this.visualFootprint;
    }

    return sizeDelta;
}

ValueRecord.prototype.onPreOpen =
function vr_create()
{
    if (this.value.jsType != jsdIValue.TYPE_OBJECT)
        return;
    
    function vr_compare (a, b)
    {
        aType = a.value.jsType;
        bType = b.value.jsType;
        
        if (aType < bType)
            return -1;
        
        if (aType > bType)
            return 1;
        
        aVal = a.displayName;
        bVal = b.displayName;
        
        if (aVal < bVal)
            return -1;
        
        if (aVal > bVal)
            return 1;
        
        return 0;
    }
    
    this.childData = new Array();
    this.isContainerOpen = false;
    
    var p = new Object();
    this.value.getProperties (p, {});
    this.lastPropertyCount = p.value.length;
    /* we'll end up with the 0 from the prototype */
    delete this.hiddenFunctionCount;
    for (var i = 0; i < p.value.length; ++i)
    {
        var prop = p.value[i];
        if (this.showFunctions ||
            prop.value.jsType != jsdIValue.TYPE_FUNCTION)
        {
            this.childData.push(new ValueRecord(prop.value,
                                                prop.name.stringValue,
                                                formatFlags(prop.flags)));
        }
        else
        {
            ++this.hiddenFunctionCount;
        }
    }

    this.childData.sort (vr_compare);

    if (this.value.jsPrototype)
        this.childData.unshift (new ValueRecord(this.value.jsPrototype,
                                                MSG_VAL_PROTO));

    if (this.value.jsParent)
        this.childData.unshift (new ValueRecord(this.value.jsParent,
                                                MSG_VAL_PARENT));
    
    for (i = 0; i < this.childData.length; ++i)
    {
        var cd = this.childData[i];
        cd.parentRecord = this;
        cd.childIndex = i;
        cd.isHidden = false;
    }
}

ValueRecord.prototype.onPostClose =
function vr_destroy()
{
    this.childData = new Array();
}

console.stackView = new TreeOView(stackShare);

console.stackView.refresh =
function wv_refresh()
{
    var sk = this.stack;
    var delta = 0;
    
    for (var i = 0; i < sk.childData.length; ++i)
    {
        var frame = sk.childData[i];
        var thisDelta = 0;
        for (var j = 0; j < frame.childData.length; ++j)
            thisDelta += frame.childData[j].refresh();
        /* if the container isn't open, we still have to update the children,
         * but we don't care about any visual footprint changes */
        if (frame.isContainerOpen)
        {
            frame.visualFootprint += thisDelta;
            delta += thisDelta;
        }
    }

    sk.visualFootprint += delta;
    this.childData.visualFootprint += delta;
    this.childData.invalidateCache();
    this.outliner.rowCountChanged (0, sk.visualFootprint);
    this.outliner.invalidate();
}

console.stackView.getCellProperties =
function sv_cellprops (index, colID, properties)
{
    if (colID != "stack-col-0")
        return;
    
    var row = this.childData.locateChildByVisualRow(index);
    if (row)
    {
        if (row.getProperties)
            return row.getProperties (properties);

        if (row.property)
            return properties.AppendElement (row.property);
    }
}

console.stackView.stack = new TOLabelRecord ("stack-col-0", MSG_CALL_STACK);

var projectShare = new Object();

console.projectView = new TreeOView(projectShare);

console.projectView.getCellProperties =
function pv_cellprops (index, colID, properties)
{
    if (colID != "project-col-0")
        return;
    
    var row = this.childData.locateChildByVisualRow(index);
    if (row)
    {
        if (row.getProperties)
            return row.getProperties (properties);

        if (row.property)
            return properties.AppendElement (row.property);
    }
}

console.blacklist = new TOLabelRecord ("project-col-0", MSG_BLACKLIST);

console.blacklist.addItem =
function bl_additem (fileName, functionName, startLine, endLine)
{
    var len = this.childData.length;
    for (var i = 0; i < len; ++i)
    {
        var cd = this.childData[i];
        if (startLine == cd.startLine && endLine == cd.endLine &&
            fileName == cd.fileName)
            return cd;
    }
    
    var rec = new BLRecord (fileName, functionName, startLine, endLine);
    this.appendChild (rec);
    return rec;
}

console.blacklist.isSourceBlacklisted =
function bl_islisted (fileName, line)
{
    var len = this.childData.length;
    for (var i = 0; i < len; ++i)
    {
        var cd = this.childData[i];
        if (line >= cd.startLine && line <= cd.endLine &&
            fileName == cd.fileName && cd.enabled == true)
            return true;
    }
    
    return false;
}

function BLRecord (fileName, functionName, startLine, endLine)
{
    this.setColumnPropertyName ("project-col-0", "fileName");
    this.setColumnPropertyName ("project-col-1", "functionName");
    this.setColumnPropertyName ("project-col-2", "startLine");
    this.setColumnPropertyName ("project-col-3", "endLine");
    this.fileName = fileName;
    this.functionName = functionName;
    this.startLine = startLine;
    this.endLine = endLine;
    this.enabled = true;
}

console.breakpoints = new TOLabelRecord ("project-col-0", MSG_BREAK_REC);

console.breakpoints.locateChildByFileLine =
function bpt_findfl (fileName, line)
{
    for (var i = 0; i < this.childData.length; ++i)
    {
        var child = this.childData[i];
        if (child.line == line &&
            child.fileName == fileName)
            return child;
    }

    return null;
}

function BPRecord (fileName, line)
{
    var record = this;
    function getMatchLength ()
    {
        return record.scriptRecords.length;
    }
        
    this.scriptRecords = new Array();
    this.fileName = fileName;
    this._enabled = true;
    this.stop = true;

    this.setColumnPropertyName ("project-col-0", "shortName");
    this.setColumnPropertyName ("project-col-2", "functionName");
    this.setColumnPropertyName ("project-col-1", "line");
    this.setColumnPropertyName ("project-col-3", getMatchLength);

    var ary = fileName.match(/\/([^\/?]+)(\?|$)/);
    if (ary)
        this.shortName = ary[1];
    else
        this.shortName = fileName;
    this.line = line;
    this.functionName = MSG_VAL_UNKNOWN;
}

BPRecord.prototype = new TreeOViewRecord(projectShare);

BPRecord.prototype.__defineGetter__ ("scriptMatches", bpr_getmatches);
function bpr_getmatches ()
{
    return this.scriptRecords.length;
}

BPRecord.prototype.__defineGetter__ ("enabled", bpr_getenabled);
function bpr_getenabled ()
{
    return this._enabled;
}

BPRecord.prototype.__defineSetter__ ("enabled", bpr_setenabled);
function bpr_setenabled (state)
{
    if (state == this._enabled)
        return;
    
    var delta = (state) ? +1 : -1;
    
    for (var i = 0; i < this.scriptRecords.length; ++i)
    {
        this.scriptRecords[i].bpcount += delta;
        var script = this.scriptRecords[i].script;
        var pc = script.lineToPc(this.line);
        if (state)
            script.setBreakpoint(pc);
        else
            script.clearBreakpoint(pc);
    }
    this._enabled = state;
}

BPRecord.prototype.matchesScriptRecord =
function bpr_matchrec (scriptRec)
{
    return (scriptRec.script.fileName.indexOf(this.fileName) != -1 &&
            scriptRec.containsLine(this.line) &&
            scriptRec.isLineExecutable(this.line));
}

BPRecord.prototype.addScriptRecord =
function bpr_addscript (scriptRec)
{
    for (var i = 0; i < this.scriptRecords.length; ++i)
        if (this.scriptRecords[i] == scriptRec)
            return;

    if (this._enabled)
    {
        var pc = scriptRec.script.lineToPc(this.line);
        scriptRec.script.setBreakpoint(pc);
    }
    
    this.functionName = scriptRec.functionName;
    ++(scriptRec.bpcount);
    
    this.scriptRecords.push(scriptRec);
}

BPRecord.prototype.removeScriptRecord =
function bpr_remscript (scriptRec)
{
    for (var i = 0; i < this.scriptRecords.length; ++i)
        if (this.scriptRecords[i] == scriptRec)
        {
            --(this.scriptRecords[i].bpcount);
            arrayRemoveAt(this.scriptRecords, i);
            return;
        }
}
