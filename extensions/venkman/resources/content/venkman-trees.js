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

function initTrees()
{
    const ATOM_CTRID = "@mozilla.org/atom-service;1";
    const nsIAtomService = Components.interfaces.nsIAtomService;

    var atomsvc =
        Components.classes[ATOM_CTRID].getService(nsIAtomService);

    console.sourceView.atomCurrent        = atomsvc.getAtom("current-line");
    console.sourceView.atomHighlightStart = atomsvc.getAtom("highlight-start");
    console.sourceView.atomHighlightRange = atomsvc.getAtom("highlight-range");
    console.sourceView.atomHighlightEnd   = atomsvc.getAtom("highlight-end");
    console.sourceView.atomBreakpoint     = atomsvc.getAtom("breakpoint");
    console.sourceView.atomFBreakpoint    = atomsvc.getAtom("future-breakpoint");
    console.sourceView.atomCode           = atomsvc.getAtom("code");
    console.sourceView.atomPrettyPrint    = atomsvc.getAtom("prettyprint");
    console.sourceView.atomWhitespace     = atomsvc.getAtom("whitespace");

    var tree = document.getElementById("source-tree");
    tree.treeBoxObject.view = console.sourceView;

    console.scriptsView.childData.setSortColumn("baseLineNumber");
    console.scriptsView.groupFiles  = true;
    console.scriptsView.atomUnknown = atomsvc.getAtom("ft-unk");
    console.scriptsView.atomHTML    = atomsvc.getAtom("ft-html");
    console.scriptsView.atomJS      = atomsvc.getAtom("ft-js");
    console.scriptsView.atomXUL     = atomsvc.getAtom("ft-xul");
    console.scriptsView.atomXML     = atomsvc.getAtom("ft-xml");
    console.scriptsView.atomGuessed = atomsvc.getAtom("fn-guessed");
    console.scriptsView.atomBreakpoint = atomsvc.getAtom("has-bp");

    tree = document.getElementById("script-list-tree");
    tree.treeBoxObject.view = console.scriptsView;
    tree.setAttribute ("ondraggesture",
                           "nsDragAndDrop.startDrag(event, " +
                           "console.scriptsView);");

    tree = document.getElementById("stack-tree");
    tree.treeBoxObject.view = console.stackView;

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

    tree = document.getElementById("project-tree");
    tree.treeBoxObject.view = console.projectView;
    
    console.projectView.atomBlacklist   = atomsvc.getAtom("pj-blacklist");
    console.projectView.atomBLItem      = atomsvc.getAtom("pj-bl-item");
    console.projectView.atomWindows     = atomsvc.getAtom("pj-windows");
    console.projectView.atomWindow      = atomsvc.getAtom("pj-window");
    console.projectView.atomFiles       = atomsvc.getAtom("pj-files");
    console.projectView.atomFile        = atomsvc.getAtom("pj-file");
    console.projectView.atomBreakpoints = atomsvc.getAtom("pj-breakpoints");
    console.projectView.atomBreakpoint  = atomsvc.getAtom("pj-breakpoint");
    
    console.blacklist.property = console.projectView.atomBlacklist;
    console.blacklist.reserveChildren();
    //console.projectView.childData.appendChild (console.blacklist);

    console.windows.property = console.projectView.atomWindows;
    console.windows.reserveChildren();
    console.projectView.childData.appendChild (console.windows);

    console.breakpoints.property = console.projectView.atomBreakpoints;
    console.breakpoints.reserveChildren();
    console.projectView.childData.appendChild (console.breakpoints);

    WindowRecord.prototype.property = console.projectView.atomWindow;
    FileContainerRecord.prototype.property = console.projectView.atomFiles;
    FileRecord.prototype.property = console.projectView.atomFile;
    BPRecord.prototype.property = console.projectView.atomBreakpoint;
    BLRecord.prototype.property = console.projectView.atomBLItem;    
}

function destroyTrees()
{
    console.sourceView.tree.view = null;
    console.scriptsView.tree.view = null;
    console.stackView.tree.view = null;
    console.projectView.tree.view = null;
}

console.sourceView = new BasicOView();

console.sourceView.details = null;
console.sourceView.prettyPrint = false;

console.sourceView.LINE_BREAKABLE  = 1;
console.sourceView.LINE_BREAKPOINT = 2;

console.sourceView._scrollTo = BasicOView.prototype.scrollTo;
console.sourceView.scrollTo =
function sv_scrollto (line, align)
{
    if (!("childData" in this))
        return;

    if (!this.childData.isLoaded)
    {
        /* the source hasn't been loaded yet, store line/align for processing
         * when the load is done. */
        this.childData.pendingScroll = line;
        this.childData.pendingScrollType = align;
        return;
    }
    this._scrollTo(line, align);
}

/*
 * pass in a SourceText to be displayed on this tree
 */
console.sourceView.displaySourceText =
function sv_dsource (sourceText)
{
    if ("childData" in this && sourceText == this.childData)
        return;
    
    function tryAgain (result)
    {
        if (result == Components.results.NS_OK)
            console.sourceView.displaySourceText(sourceText);
        else
        {
            dd ("source load failed: '" + sourceText.fileName + "'");
        }
    }

    /* save the current position before we change to another source */
    if ("childData" in this)
    {
        this.childData.pendingScroll = this.tree.getFirstVisibleRow() + 1;
        this.childData.pendingScrollType = -1;
    }
    
    if (!sourceText)
    {
        delete this.childData;
        this.rowCount = 0;
        this.tree.rowCountChanged(0, 0);
        this.tree.invalidate();
        return;
    }
    
    /* if the source for this record isn't loaded yet, load it and call ourselves
     * back after */
    if (!sourceText.isLoaded)
    {
        disableReloadCommand();
        /* clear the view while we wait for the source text */
        delete this.childData;
        this.rowCount = 0;
        this.tree.rowCountChanged(0, 0);
        this.tree.invalidate();
        /* load the source text, call the tryAgain function when it's done. */
        sourceText.pendingScroll = 0;
        sourceText.pendingScrollType = -1;
        sourceText.loadSource(tryAgain);
        return;
    }

    enableReloadCommand();
    this.childData = sourceText;
    this.rowCount = sourceText.lines.length;
    this.tabString = leftPadString ("", sourceText.tabWidth, " ");
    this.tree.rowCountChanged(0, this.rowCount);
    this.tree.invalidate();

    var hdr = document.getElementById("source-line-text");
    hdr.setAttribute ("label", sourceText.fileName);

    if ("pendingScroll" in this.childData)
    {
        this.scrollTo (this.childData.pendingScroll,
                       this.childData.pendingScrollType);
        delete this.childData.pendingScroll;
        delete this.childData.pendingScrollType;
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
    if (!("childData" in this))
        return;
    
    if (!this.childData.isLoaded)
    {
        /* the source hasn't been loaded yet, queue the scroll for later. */
        this.childData.pendingScroll = line;
        this.childData.pendingScrollType = 0;
        return;
    }

    delete this.childData.pendingScroll;
    delete this.childData.pendingScrollType;

    var first = this.tree.getFirstVisibleRow();
    var last = this.tree.getLastVisibleRow();
    var fuzz = 2;
    if (line < (first + fuzz) || line > (last - fuzz))
        this.scrollTo (line, 0);
    else
        this.tree.invalidate(); /* invalidate to show the new currentLine if
                                     * we don't have to scroll. */

}    

/**
 * Create a context object for use in the sourceView context menu.
 */
console.sourceView.getContext =
function sv_getcx(cx)
{
    if (!cx)
        cx = new Object();

    var sourceText = this.childData;
    cx.fileName = sourceText.fileName;
    cx.lineIsExecutable = null;
    var selection = this.tree.selection;
    var row = selection.currentIndex;

    if (row != -1)
    {
        cx.lineNumber = selection.currentIndex + 1;
        if ("lineMap" in sourceText && sourceText.lineMap[row])
            cx.lineIsExecutable = true;
        if (typeof sourceText.lines[row] == "object" &&
            "bpRecord" in sourceText.lines[row])
        {
            cx.breakpointRec = sourceText.lines[row].bpRecord;
            cx.breakpointIndex = cx.breakpointRec.childIndex;
        }
    }
    else
        dd ("no currentIndex");
    
    var rangeCount = this.tree.selection.getRangeCount();
    if (rangeCount > 0 && !("lineNumberList" in cx))
    {
        cx.lineNumberList = new Array();
    }
    
    for (var range = 0; range < rangeCount; ++range)
    {
        var min = new Object();
        var max = new Object();
        this.tree.selection.getRangeAt(range, min, max);
        min = min.value;
        max = max.value;

        for (row = min; row <= max; ++row)
        {
            cx.lineNumberList.push (row + 1);
            if (range == 0 && row == min &&
                "lineMap" in sourceText && sourceText.lineMap[row])
            {
                cx.lineIsExecutable = true;
            }
            if (typeof sourceText.lines[row] == "object" &&
                "bpRecord" in sourceText.lines[row])
            {
                var sourceLine = sourceText.lines[row];
                if (!("breakpointRecList" in cx))
                    cx.breakpointRecList = new Array();
                cx.breakpointRecList.push(sourceLine.bpRecord);
                if (!("breakpointIndexList" in cx))
                    cx.breakpointIndexList = new Array();
                cx.breakpointIndexList.push(sourceLine.bpRecord.childIndex);
            }
        }
    }
    return cx;
}    

/* nsITreeView */
console.sourceView.getRowProperties =
function sv_rowprops (row, properties)
{
    if ("frames" in console)
     {
        if (((!this.prettyPrint && row == console.stopLine - 1) ||
             (this.prettyPrint && row == console.pp_stopLine - 1)) &&
            console.stopFile == this.childData.fileName && this.details)
        {
            properties.AppendElement(this.atomCurrent);
        }
    }
}

/* nsITreeView */
console.sourceView.getCellProperties =
function sv_cellprops (row, colID, properties)
{
    if (!("childData" in this) || !this.childData.isLoaded ||
        row < 0 || row >= this.childData.lines.length)
        return;

    var line = this.childData.lines[row];
    if (!line)
        return;
    
    if (colID == "breakpoint-col")
    {
        if (this.prettyPrint)
            properties.AppendElement(this.atomPrettyPrint);
        if (typeof this.childData.lines[row] == "object" &&
            "bpRecord" in this.childData.lines[row])
        {
            if (this.childData.lines[row].bpRecord.scriptRecords.length)
                properties.AppendElement(this.atomBreakpoint);
            else
                properties.AppendElement(this.atomFBreakpoint);
        }
        else if ("lineMap" in this.childData && row in this.childData.lineMap &&
                 this.childData.lineMap[row] & this.LINE_BREAKABLE)
        {
            properties.AppendElement(this.atomCode);
        }
        else
        {
            properties.AppendElement(this.atomWhitespace);
        }
    }
    
    if ("highlightStart" in console)
    {
        var atom;
        if (row == console.highlightStart)
        {
            atom = this.atomHighlightStart;
        }
        else if (row == console.highlightEnd)
        {
            atom = this.atomHighlightEnd;
        }
        else if (row > console.highlightStart && row < console.highlightEnd)
        {
            atom = this.atomHighlightRange;
        }
        
        if (atom && console.highlightFile == this.childData.fileName)
        {
            properties.AppendElement(atom);
        }
    }

    if ("frames" in console)
    {
        if (((!this.prettyPrint && row == console.stopLine - 1) ||
             (this.prettyPrint && row == console.pp_stopLine - 1)) &&
            console.stopFile == this.childData.fileName)
        {
            properties.AppendElement(this.atomCurrent);
        }
    }
}

/* nsITreeView */
console.sourceView.getCellText =
function sv_getcelltext (row, colID)
{    
    if (!this.childData.isLoaded || 
        row < 0 || row > this.childData.lines.length)
        return "";
    
    switch (colID)
    {
        case "source-line-text":
            return this.childData.lines[row].replace(/\t/g, this.tabString);

        case "source-line-number":
            return row + 1;
            
        default:
            return "";
    }
}

var scriptShare = new Object();

function ScriptContainerRecord(fileName)
{
    this.setColumnPropertyName ("script-name", "displayName");
    this.setColumnPropertyValue ("script-line-start", "");
    this.setColumnPropertyValue ("script-line-extent", "");
    this.fileName = fileName;
    var sov = console.scriptsView;
    this.fileType = sov.atomUnknown;
    this.shortName = this.fileName;
    this.group = 4;
    this.bpcount = 0;

    this.shortName = getFileFromPath(this.fileName);
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
    
    this.displayName = this.shortName;
}

ScriptContainerRecord.prototype = new TreeOViewRecord(scriptShare);

ScriptContainerRecord.prototype.onDragStart =
function scr_dragstart (e, transferData, dragAction)
{        
    transferData.data = new TransferData();
    transferData.data.addDataForFlavour("text/x-venkman-file", this.fileName);
    transferData.data.addDataForFlavour("text/x-moz-url", this.fileName);
    transferData.data.addDataForFlavour("text/unicode", this.fileName);
    transferData.data.addDataForFlavour("text/html",
                                        "<a href='" + this.fileName +
                                        "'>" + this.fileName + "</a>");
    return true;
}    

ScriptContainerRecord.prototype.appendScriptRecord =
function scr_addscript(scriptRec)
{
    this.appendChild (scriptRec);
}

ScriptContainerRecord.prototype.__defineGetter__ ("sourceText", scr_gettext);
function scr_gettext ()
{
    if (!("_sourceText" in this))
        this._sourceText = new SourceText (this, this.fileName);
    return this._sourceText;
}

ScriptContainerRecord.prototype.sortCompare =
function scr_compare (a, b)
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

ScriptContainerRecord.prototype.locateChildByScript =
function scr_locate (script)
{
    for (var i = 0; i < this.childData.length; ++i)
        if (script == this.childData[i].script)
            return this.childData[i];

    return null;
}

ScriptContainerRecord.prototype.guessFunctionNames =
function scr_guessnames (sourceText)
{
    for (var i = 0; i < this.childData.length; ++i)
    {
        this.childData[i].guessFunctionName(sourceText);
    }
    console.scriptsView.tree.invalidate();
}

function ScriptRecord(script) 
{
    if (!(script instanceof jsdIScript))
        throw new BadMojo (ERR_INVALID_PARAM, "script");

    this.setColumnPropertyName ("script-name", "functionName");
    this.setColumnPropertyName ("script-line-start", "baseLineNumber");
    this.setColumnPropertyName ("script-line-extent", "lineExtent");
    this.functionName = (script.functionName) ? script.functionName :
        MSG_VAL_TLSCRIPT;
    this.baseLineNumber = script.baseLineNumber;
    this.lineExtent = script.lineExtent;
    this.script = script;

    this.jsdurl = "jsd:sourcetext?url=" + escape(this.script.fileName) + 
        "&base=" + this.baseLineNumber + "&" + "extent=" + this.lineExtent +
        "&name=" + this.functionName;
}

ScriptRecord.prototype = new TreeOViewRecord(scriptShare);

ScriptRecord.prototype.onDragStart =
function sr_dragstart (e, transferData, dragAction)
{        
    var fileName = this.script.fileName;
    transferData.data = new TransferData();
    transferData.data.addDataForFlavour("text/x-jsd-url", this.jsdurl);
    transferData.data.addDataForFlavour("text/x-moz-url", fileName);
    transferData.data.addDataForFlavour("text/unicode", fileName);
    transferData.data.addDataForFlavour("text/html",
                                        "<a href='" + fileName +
                                        "'>" + fileName + "</a>");
    return true;
}

ScriptRecord.prototype.containsLine =
function sr_containsl (line)
{
    if (this.script.baseLineNumber <= line && 
        this.script.baseLineNumber + this.lineExtent > line)
        return true;
    
    return false;
}

ScriptRecord.prototype.__defineGetter__ ("sourceText", sr_getsource);
function sr_getsource ()
{
    if (!("_sourceText" in this))
        this._sourceText = new PPSourceText(this);
    return this._sourceText;
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
function sr_guessname (sourceText)
{
    var targetLine = this.script.baseLineNumber;
    var sourceLines = sourceText.lines;
    if (targetLine > sourceLines)
    {
        dd ("not enough source to guess function at line " + targetLine);
        return;
    }
    
    if (this.functionName == MSG_VAL_TLSCRIPT)
    {
        if (sourceLines[targetLine].search(/\WsetTimeout\W/) != -1)
            this.functionName = MSD_VAL_TOSCRIPT;
        else if (sourceLines[targetLine].search(/\WsetInterval\W/) != -1)
            this.functionName = MSD_VAL_IVSCRIPT;        
        else if (sourceLines[targetLine].search(/\Weval\W/) != -1)
            this.functionName = MSD_VAL_EVSCRIPT;
        return;
    }
    
    if (this.functionName != "anonymous")
        return;
    var scanText = "";
    
    /* scan at most 3 lines before the function definition */
    switch (targetLine - 3)
    {
        case -2: /* target line is the first line, nothing before it */
            break;

        case -1: /* target line is the second line, one line before it */ 
            scanText = 
                String(sourceLines[targetLine - 2]);
            break;
        case 0:  /* target line is the third line, two before it */
            scanText =
                String(sourceLines[targetLine - 3]) + 
                String(sourceLines[targetLine - 2]);
            break;            
        default: /* target line is the fourth or higher line, three before it */
            scanText += 
                String(sourceLines[targetLine - 4]) + 
                String(sourceLines[targetLine - 3]) +
                String(sourceLines[targetLine - 2]);
            break;
    }

    scanText += String(sourceLines[targetLine - 1]);
    
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
                    wv.tree.invalidateRow(cd.calculateVisualRow());
                }
            }
        }
    }
    else
        dd ("unable to guess function name based on text ``" + scanText + "''");
}            
    
console.scriptsView = new TreeOView(scriptShare);

console.scriptsView.onDragStart = Prophylactic(console.scriptsView,
                                               scv_dstart);
function scv_dstart (e, transferData, dragAction)
{
    var row = new Object();
    var colID = new Object();
    var childElt = new Object();

    this.tree.getCellAt(e.clientX, e.clientY, row, colID, childElt);
    if (!colID.value)
        return false;
    
    row = this.childData.locateChildByVisualRow (row.value);
    var rv = false;
    if (row && ("onDragStart" in row))
        rv = row.onDragStart (e, transferData, dragAction);

    return rv;
}

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
        if ("fileType" in row && colID == "script-name")
            properties.AppendElement (row.fileType);
        if ("isGuessedName" in row && colID == "script-name")
            properties.AppendElement (this.atomGuessed);
        if (row.bpcount > 0)
            properties.AppendElement (this.atomBreakpoint);
    }
}

/**
 * Create a context object for use in the scriptsView context menu.
 */
console.scriptsView.getContext =
function scv_getcx(cx)
{
    if (!cx)
        cx = new Object();

    var selection = this.tree.selection;
    var row = selection.currentIndex;
    var rec = this.childData.locateChildByVisualRow (row);
    var firstRec = rec;
    
    if (!rec)
    {
        dd ("no record at currentIndex " + row);
        return cx;
    }
    
    cx.target = rec;
    
    if (rec instanceof ScriptContainerRecord)
    {
        cx.url = cx.fileName = rec.fileName;
        cx.scriptRec = rec.childData[0];
        cx.scriptRecList = rec.childData;
    }
    else if (rec instanceof ScriptRecord)
    {
        cx.scriptRec = rec;
        cx.lineNumber = rec.script.baseLineNumber;
        cx.rangeStart = cx.lineNumber;
        cx.rangeEnd   = rec.script.lineExtent + cx.lineNumber;
    }

    var rangeCount = this.tree.selection.getRangeCount();
    if (rangeCount > 0 && !("lineNumberList" in cx))
    {
        cx.lineNumberList = new Array();
    }
    
    if (rangeCount > 0)
    {
        cx.urlList = cx.fileNameList = new Array();
        if (firstRec instanceof ScriptRecord)
            cx.scriptRecList  = new Array();
        cx.lineNumberList = new Array();
        cx.rangeStartList = new Array();
        cx.rangeEndList   = new Array();        
    }
    
    for (var range = 0; range < rangeCount; ++range)
    {
        var min = new Object();
        var max = new Object();
        this.tree.selection.getRangeAt(range, min, max);
        min = min.value;
        max = max.value;

        for (row = min; row <= max; ++row)
        {
            rec = this.childData.locateChildByVisualRow(row);
            if (rec instanceof ScriptContainerRecord)
            {
                cx.fileNameList.push (rec.fileName);
            }
            else if (rec instanceof ScriptRecord)
            {
                //cx.fileNameList.push (rec.script.fileName);
                if (firstRec instanceof ScriptRecord)
                    cx.scriptRecList.push (rec);
                cx.lineNumberList.push (rec.script.baseLineNumber);
                cx.rangeStartList.push (rec.script.baseLineNumber);
                cx.rangeEndList.push (rec.script.lineExtent +
                                      rec.script.baseLineNumber);
            }
        }
    }
    return cx;
}    

var stackShare = new Object();

function FrameRecord (frame)
{
    if (!(frame instanceof jsdIStackFrame))
        throw new BadMojo (ERR_INVALID_PARAM, "value");

    this.setColumnPropertyName ("stack-col-0", "functionName");
    this.setColumnPropertyName ("stack-col-2", "location");

    var fn = frame.functionName;
    if (!fn)
        fn = MSG_VAL_TLSCRIPT;

    if (!frame.isNative)
    {
        var sourceRec = console.scripts[frame.script.fileName];
        if (sourceRec)
        {
            this.location = sourceRec.shortName + ":" + frame.line;
            var scriptRec = sourceRec.locateChildByScript(frame.script);
            if (fn == "anonymous")
                fn = scriptRec.functionName;
        }
        else
            dd ("no sourcerec");
    }
    else
    {
        this.location = MSG_URL_NATIVE;
    }
    
    this.functionName = fn;
    this.frame = frame;
    this.reserveChildren();
    if (frame.scope)
    {
        this.scopeRec = new ValueRecord (frame.scope, MSG_WORD_SCOPE, "");
        this.appendChild (this.scopeRec);
    }
    if (frame.thisValue)
    {
        this.thisRec = new ValueRecord (frame.thisValue, MSG_WORD_THIS, "");
        this.appendChild (this.thisRec);
    }
    this.property = console.stackView.atomFrame;
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
    this.jsType = null;
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
    
    if (this.jsType != jsdIValue.TYPE_OBJECT && "childData" in this)
    {
        /* if we're not an object but we have child data, then we must have just
         * turned into something other than an object. */
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

console.stackView.restoreState =
function sv_restore ()
{
    function restoreBranch (target, source)
    {
        for (var i in source)
        {
            if (typeof source[i] == "object")
            {
                var name = source[i].name;
                var len = target.length;
                for (var j = 0; j < len; ++j)
                {
                    if (target[j]._colValues["stack-col-0"] == name &&
                        "childData" in target[j])
                    {
                        //dd ("opening " + name);
                        target[j].open();
                        restoreBranch (target[j].childData, source[i]);
                    }
                }
            }
        }
    }

    if ("savedState" in this) {
        this.freeze();
        restoreBranch (this.stack.childData, this.savedState);
        this.thaw();
        this.scrollTo (this.savedState.firstVisible, -1);
    }
}

console.stackView.saveState =
function sv_save ()
{
    function saveBranch (target, source)
    {
        var len = source.length;
        for (var i = 0; i < len; ++i)
        {
            if (source[i].isContainerOpen)
            {
                target[i] = new Object();
                target[i].name = source[i]._colValues["stack-col-0"];
                saveBranch (target[i], source[i].childData);
            }
        }
    }
        
    this.savedState = new Object();
    this.savedState.firstVisible = this.tree.getFirstVisibleRow() + 1;
    saveBranch (this.savedState, this.stack.childData);
    //dd ("saved as\n" + dumpObjectTree(this.savedState, 10));
}

/**
 * Create a context object for use in the stackView context menu.
 */
console.stackView.getContext =
function sv_getcx(cx)
{
    if (!cx)
        cx = new Object();

    var selection = this.tree.selection;

    var rec = this.childData.locateChildByVisualRow(selection.currentIndex);
    
    if (!rec)
    {
        dd ("no current index.");
        return cx;
    }

    cx.target = rec;
    
    if (rec instanceof FrameRecord)
    {
        cx.frameIndex = rec.childIndex;
    }
    else if (rec instanceof ValueRecord)
    {
        cx.jsdValue = rec.value;
    }

    var rangeCount = this.tree.selection.getRangeCount();
    if (rangeCount > 0)
        cx.jsdValueList = new Array();
    
    for (var range = 0; range < rangeCount; ++range)
    {
        var min = new Object();
        var max = new Object();
        this.tree.selection.getRangeAt(range, min, max);
        for (var i = min; i < max; ++i)
        {
            rec = this.childData.locateChildByVisualRow(i);
            if (rec instanceof ValueRecord)
                cx.jsdValueList.push(rec.value);
        }
    }

    return cx;
}    

console.stackView.refresh =
function sv_refresh()
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
    this.tree.rowCountChanged (0, sk.visualFootprint);
    this.tree.invalidate();
}

console.stackView.getCellProperties =
function sv_cellprops (index, colID, properties)
{
    if (colID != "stack-col-0")
        return null;
    
    var row = this.childData.locateChildByVisualRow(index);
    if (row)
    {
        if ("getProperties" in row)
            return row.getProperties (properties);

        if (row.property)
            return properties.AppendElement (row.property);
    }

    return null;
}

console.stackView.stack = new TOLabelRecord ("stack-col-0", MSG_CALL_STACK,
                                             ["stack-col-1"]);

var projectShare = new Object();

console.projectView = new TreeOView(projectShare);

console.projectView.getCellProperties =
function pv_cellprops (index, colID, properties)
{
    if (colID != "project-col-0")
        return null;
    
    var row = this.childData.locateChildByVisualRow(index);
    if (row)
    {
        if ("getProperties" in row)
            return row.getProperties (properties);

        if ("property" in row)
            return properties.AppendElement (row.property);
    }

    return null;
}

/**
 * Create a context object for use in the projectView context menu.
 */
console.projectView.getContext =
function pv_getcx(cx)
{
    if (!cx)
        cx = new Object();

    var selection = this.tree.selection;

    var rec = this.childData.locateChildByVisualRow(selection.currentIndex);
    
    if (!rec)
    {
        dd ("no current index.");
        return cx;
    }

    cx.target = rec;
    
    if (rec instanceof TOLabelRecord)
    {
        if (rec.label == MSG_BREAK_REC)
            cx.breakpointLabel = true;
        return cx;
    }

    if (rec instanceof BPRecord)
    {
        cx.breakpointRec = rec;
        cx.url = cx.fileName = rec.fileName;
        cx.lineNumber = rec.line;
        cx.breakpointIndex = rec.childIndex;
    }
    else if (rec instanceof WindowRecord || rec instanceof FileRecord)
    {
        cx.url = cx.fileName = rec.url;
    }
    
    var rangeCount = this.tree.selection.getRangeCount();
    if (rangeCount > 0)
    {
        cx.breakpointRecList = new Array();
        cx.breakpointIndexList = new Array();
        cx.fileList = cx.urlList = new Array();
    }
    
    for (var range = 0; range < rangeCount; ++range)
    {
        var min = new Object();
        var max = new Object();
        this.tree.selection.getRangeAt(range, min, max);
        min = min.value;
        max = max.value;
        for (var i = min; i <= max; ++i)
        {
            rec = this.childData.locateChildByVisualRow(i);
            if (rec instanceof BPRecord)
            {
                cx.breakpointRecList.push(rec);
                cx.breakpointIndexList.push(rec.childIndex);
                cx.fileList.push (rec.fileName);
            }
            else if (rec instanceof WindowRecord || rec instanceof FileRecord)
            {
                cx.fileList.push (rec.url);
            }    
        }
    }

    return cx;
}
    
console.blacklist = new TOLabelRecord ("project-col-0", MSG_BLACKLIST,
                                       ["project-col-1", "project-col-2", 
                                        "project-col-3", "project-col-4"]);

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

console.windows = new TOLabelRecord ("project-col-0", MSG_WINDOW_REC,
                                     ["project-col-1", "project-col-2", 
                                      "project-col-3", "project-col-4"]);

console.windows.locateChildByWindow =
function win_find (win)
{
    for (var i = 0; i < this.childData.length; ++i)
    {
        var child = this.childData[i];
        if (child.window == win)
            return child;
    }

    return null;
}

function WindowRecord (win, baseURL)
{
    function none() { return ""; };
    this.setColumnPropertyName ("project-col-0", "shortName");
    this.setColumnPropertyName ("project-col-1", none);
    this.setColumnPropertyName ("project-col-2", none);
    this.setColumnPropertyName ("project-col-3", none);

    this.window = win;
    this.url = win.location.href;
    if (this.url.search(/^\w+:/) == -1 && "url")
    {
        this.url = baseURL + url;
        this.baseURL = baseURL;
    }
    else
    {
        this.baseURL = getPathFromURL(this.url);
    }
    
    this.shortName = getFileFromPath (this.url);
    
    this.reserveChildren();
    this.filesRecord = new FileContainerRecord();
}

WindowRecord.prototype = new TreeOViewRecord(projectShare);

WindowRecord.prototype.onPreOpen =
function wr_preopen()
{   
    this.childData = new Array();
    console.projectView.freeze();

    this.appendChild(this.filesRecord);    

    var framesLength = this.window.frames.length;
    for (var i = 0; i < framesLength; ++i)
    {
        this.appendChild(new WindowRecord(this.window.frames[i].window,
                                          this.baseURL));
    }

    console.projectView.thaw();
}
    
function FileContainerRecord ()
{
    function files() { return MSG_FILES_REC; }
    function none() { return ""; }
    this.setColumnPropertyName ("project-col-0", files);
    this.setColumnPropertyName ("project-col-1", none);
    this.setColumnPropertyName ("project-col-2", none);
    this.setColumnPropertyName ("project-col-3", none);
    this.reserveChildren();
}

FileContainerRecord.prototype = new TreeOViewRecord(projectShare);

FileContainerRecord.prototype.onPreOpen =
function fcr_preopen ()
{
    if (!this.parentRecord)
        return;
    
    this.childData = new Array();
    var doc = this.parentRecord.window.document;
    var nodeList = doc.getElementsByTagName("script");
    
    console.projectView.freeze();
    for (var i = 0; i < nodeList.length; ++i)
    {
        var url = nodeList.item(i).getAttribute("src");
        if (url)
        {
            if (url.search(/^\w+:/) == -1)
                url = getPathFromURL (this.parentRecord.url) + url;
            this.appendChild(new FileRecord(url));
        }
    }
    console.projectView.thaw();
}

function FileRecord (url)
{
    function none() { return ""; }
    this.setColumnPropertyName ("project-col-0", "shortName");
    this.setColumnPropertyName ("project-col-1", none);
    this.setColumnPropertyName ("project-col-2", none);
    this.setColumnPropertyName ("project-col-3", none);

    this.url = url;
    this.shortName = getFileFromPath(url);
}

FileRecord.prototype = new TreeOViewRecord(projectShare);

console.breakpoints = new TOLabelRecord ("project-col-0", MSG_BREAK_REC,
                                         ["project-col-1", "project-col-2", 
                                          "project-col-3", "project-col-4"]);

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
        var pc = script.lineToPc(this.line, PCMAP_SOURCETEXT);
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
            scriptRec.script.isLineExecutable(this.line, PCMAP_SOURCETEXT));
}

BPRecord.prototype.addScriptRecord =
function bpr_addscript (scriptRec)
{
    for (var i = 0; i < this.scriptRecords.length; ++i)
        if (this.scriptRecords[i] == scriptRec)
            return;

    if (this._enabled)
    {
        var pc = scriptRec.script.lineToPc(this.line, PCMAP_SOURCETEXT);
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

BPRecord.prototype.hasScriptRecord =
function bpr_addscript (scriptRec)
{
    for (var i = 0; i < this.scriptRecords.length; ++i)
        if (this.scriptRecords[i] == scriptRec)
            return true;

    return false;
}
