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

function initOutliners()
{
    const ATOM_CTRID = "@mozilla.org/atom-service;1";
    const nsIAtomService = Components.interfaces.nsIAtomService;

    var atomsvc =
        Components.classes[ATOM_CTRID].getService(nsIAtomService);

    console._sourceOutlinerView.atomCurrent = atomsvc.getAtom("current-line");
    console._sourceOutlinerView.atomBreakpoint = atomsvc.getAtom("breakpoint");
    console._sourceOutliner = document.getElementById("source-outliner");
    console._sourceOutliner.outlinerBoxObject.view = console._sourceOutlinerView;

    console._stackOutlinerView.atomCurrent = 
        atomsvc.getAtom("current-frame-flag");
    console._stackOutliner = document.getElementById("call-stack-outliner");
    console._stackOutliner.outlinerBoxObject.view = console._stackOutlinerView;

    console._scriptsOutliner = document.getElementById("script-list-outliner");
    console._scriptsOutliner.outlinerBoxObject.view =
        console._scriptsOutlinerView;
}

function BasicOView()
{}

BasicOView.prototype.rowCount = 0;
BasicOView.prototype.selection = null;

BasicOView.prototype.getCellProperties =
function bov_cellprops (row, colID, properties)
{}

BasicOView.prototype.getColumnProperties =
function bov_colprops (colID, elem, properties)
{}

BasicOView.prototype.getRowProperties =
function bov_rowprops (index, properties)
{}

BasicOView.prototype.isContainer =
function bov_isctr (index)
{
    return false;
}

BasicOView.prototype.isContainerOpen =
function bov_isctropen (index)
{
    return false;
}

BasicOView.prototype.isContainerEmpty =
function bov_isctrempt (index)
{
    return false;
}

BasicOView.prototype.isSorted =
function bov_issorted (index)
{
    return false;
}

BasicOView.prototype.canDropOn =
function bov_dropon (index)
{
    return false;
}

BasicOView.prototype.canDropBeforeAfter =
function bov_dropba (index, before)
{
    return false;
}

BasicOView.prototype.drop =
function bov_drop (index, orientation)
{
    return false;
}

BasicOView.prototype.getParentIndex =
function bov_getpi (index)
{
    return 0;
}

BasicOView.prototype.hasNextSibling =
function bov_hasnxtsib (rowIndex, afterIndex)
{
    return (afterIndex < (this.rowCount - 1));
}

BasicOView.prototype.getLevel =
function bov_getlvl (index)
{
    return 0;
}

BasicOView.prototype.centerLine =
function bov_ctrln (line)
{
    var first = this.outliner.getFirstVisibleRow();
    var last = this.outliner.getLastVisibleRow();
    this.scrollToRow(line - total / 2);
}

BasicOView.prototype.setCurrentLine =
function bov_setcl (line)
{
    if (this.currentLine)
    {
        if (line == this.currentLine)
            return;
        var lastLine = this.currentLine;
        if (line)
            this.currentLine = line;
        else
            delete this.currentLine;
        this.outliner.invalidateRow(lastLine - 1);
    }
    else
        if (line)
            this.currentLine = line;
    
    var first = this.outliner.getFirstVisibleRow();
    var last = this.outliner.getLastVisibleRow();

    if (first == 0 && last == this.rowCount)
        /* all rows are visible, nothing to scroll */
        return;
    
    var total = last - first;
    var one_qtr = total / 4;
    
    if (line < (first - one_qtr) || line > (last - one_qtr))
    {
        /* current line lies outside the middle half of the screen,
         * recenter around the line. */
        var half = total / 2;
        if (line < half)
            this.outliner.scrollToRow(0);
        else
        {
            var newFirst = line;
            if (newFirst != first)
                this.outliner.scrollToRow(line - half);
        }
    }

    this.outliner.invalidateRow(line - 1);
}
    
BasicOView.prototype.setColumnNames =
function bov_setcn (aryNames)
{
    this.columnNames = new Object();
    for (var i = 0; i < aryNames.length; ++i)
        this.columnNames[aryNames[i]] = i;
}

BasicOView.prototype.getCellText =
function bov_getcelltxt (row, colID)
{
    if (!this.columnNames)
        return "";
    
    var col = this.columnNames[colID];
    
    if (typeof col == "undefined")
        return "";
    
    return this.data[row][col];
}

BasicOView.prototype.setOutliner =
function bov_seto (outliner)
{
    this.outliner = outliner;
}

BasicOView.prototype.toggleOpenState =
function bov_toggleopen (index)
{
}

BasicOView.prototype.cycleHeader =
function bov_cyclehdr (colID, elt)
{
}

BasicOView.prototype.selectionChanged =
function bov_selchg ()
{
}

BasicOView.prototype.cycleCell =
function bov_cyclecell (row, colID)
{
}

BasicOView.prototype.isEditable =
function bov_isedit (row, colID)
{
    return false;
}

BasicOView.prototype.setCellText =
function bov_setct (row, colID, value)
{
}

BasicOView.prototype.performAction =
function bov_pact (action)
{
}

BasicOView.prototype.performActionOnRow =
function bov_pactrow (action)
{
}

BasicOView.prototype.performActionOnCell =
function bov_pactcell (action)
{
}

console._sourceOutlinerView = new BasicOView();

console._sourceOutlinerView.getRowProperties =
function sov_rowprops (row, properties)
{
    if (row == this.currentLine - 1)
        properties.AppendElement(this.atomCurrent);
}

console._sourceOutlinerView.getCellProperties =
function sov_cellprops (row, colID, properties)
{
    if (colID == "breakpoint-col" && this.sourceArray[row].isBreakpoint)
        properties.AppendElement(this.atomBreakpoint);
}

console._sourceOutlinerView.getCellText =
function sov_getcelltext (row, colID)
{    
    if (!(this.sourceArray instanceof Array))
        return "";
    
    switch (colID)
    {
        case "source-line-text":
            return this.sourceArray[row];

        case "source-line-number":
            return row + 1;
            
        default:
            return "";
    }
}

console._sourceOutlinerView.setSourceArray =
function sov_setsrcary (sourceArray, url)
{
    this.url = url;
    
    if (sourceArray != this.sourceArray)
    {
        this.sourceArray = sourceArray;
        this.rowCount = sourceArray.length;
        delete this.lastCurrentLine;
        this.outliner.rowCountChanged(0, sourceArray.length);
        this.outliner.invalidate();
    }
}

console._stackOutlinerView = new BasicOView();

console._stackOutlinerView.rowCount = 1;

console._stackOutlinerView.getRowProperties =
function sov_rowprops (row, properties)
{
    if (row == this.currentFrame)
        properties.AppendElement(this.atomCurrent);
}

console._stackOutlinerView.getCellProperties =
function sov_cellprops (row, colID, properties)
{
    if (row == this.currentFrame && colID == "current-frame")
        properties.AppendElement(this.atomCurrent);
    
}

console._stackOutlinerView.getCellText =
function sov_getcelltext (row, colID)
{    
    if (!this.frames)
        return (row == 0 && colID == "function-name") ? MSG_VAL_NA : "";
    
    switch (colID)
    {
        case "function-name":
            return this.frames[row].script.functionName;

        case "line-number":
            return this.frames[row].line;
            
        case "file-name":
            return this.frames[row].script.fileName;

        default:
            return "";
    }
}

console._stackOutlinerView.setStack =
function skov_setstack (frames)
{
    delete this.currentFrame;

    if (frames)
    {
        this.frames = frames;
        this.rowCount = frames.length;
        this.outliner.rowCountChanged(0, frames.length);
        this.outliner.invalidate();
    }
    else
    {
        delete this.frames;
        this.rowCount = 1;
        this.outliner.rowCountChanged(0, 0);
        this.outliner.invalidate();
    }
}

console._stackOutlinerView.setCurrentFrame =
function skov_setcframe (index)
{
    if (typeof this.currentFrame != "undefined")
    {
        var oldIndex = this.currentFrame;
        this.currentFrame = index;
        this.outliner.invalidateRow (oldIndex);
    }
    else
        this.currentFrame = index;
    
    this.outliner.invalidateRow (index);
}

console._scriptsOutlinerView = new BasicOView();

console._scriptsOutlinerView.shortMode = true;
console._scriptsOutlinerView.rowCount = 0;

console._scriptsOutlinerView.getCellText =
function scov_getcelltext (row, colID)
{    
    if (!this.scripts)
        return;
    
    switch (colID)
    {
        case "script-file-name":
            return (this.shortMode) ? this.scripts[row].shortName :
                this.scripts[row].fileName;
            
        case "script-count":
            return this.scripts[row].scriptCount;

        default:
            return "";
    }
}

console._scriptsOutlinerView.refreshScripts =
function scov_refresh (e)
{
    this.scripts = new Array();
        
    for (var p in this.scriptRecords)
    {        
        if (p)
        {
            var shortname = p;
            var ary = p.match (/\/([^\/]*)$/);
            if (ary)
                shortname = ary[1];
            this.scripts.push ({fileName: p, shortName: shortname,
                                scriptCount: this.scriptRecords[p].length});
        }
    }
    
    function compareFile (a, b)
    {
        if (a.fileName > b.fileName)
            return 1;
        else if (a.fileName == b.fileName)
            return 0;
        return -1;
    }

    function compareShort (a, b)
    {
        if (a.shortName > b.shortName)
            return 1;
        else if (a.shortName == b.shortName)
            return 0;
        return -1;
    }

    this.scripts.sort((this.shortMode) ? compareShort : compareFile);
    
    this.rowCount = this.scripts.length;
    this.outliner.rowCountChanged(0, this.scripts.length);
    this.outliner.invalidate();
}

console._scriptsOutlinerView.toggleColumnMode =
function scov_tcolmode (e)
{
    if (!this.shortMode)
        this.shortMode = true;
    else
        delete this.shortMode;

    this.refreshScripts();
}

console._scriptsOutlinerView.setScripts =
function scov_setstack (scripts)
{    
    if (scripts)
    {
        this.scriptRecords = scripts;
        this.refreshScripts();
    }
    else
    {
        delete this.scripts;
        this.rowCount = 0;
        this.outliner.rowCountChanged(0, 0);
        this.outliner.invalidate();
    }
}
