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


/*
 * BasicOView provides functionality of outliner whose elements have no children.
 * Usage:
 * var myOutliner = new BasicOView()
 * myOutliner.setColumnNames (["col 1", "col 2"]);
 * myOutliner.data = [["row 1, col 1", "row 1, col 2"],
 *                    ["row 2, col 1", "row 2, col 2"]];
 * { override get*Properties, etc, as suits your purpose. }
 *
 * outlinerBoxObject.view = myOutliner;
 * 
 * You'll need to make the appropriate myOutliner.outliner.invalidate calls
 * when myOutliner.data changes.
 */

function BasicOView()
{}

/* functions *you* should call to initialize and maintain the outliner state */

/* scroll the line specified by |line| to the center of the outliner */
BasicOView.prototype.centerLine =
function bov_ctrln (line)
{
    var first = this.outliner.getFirstVisibleRow();
    var last = this.outliner.getLastVisibleRow();
    this.scrollToRow(line - total / 2);
}

/* call this to set the association between column names and data columns */
BasicOView.prototype.setColumnNames =
function bov_setcn (aryNames)
{
    this.columnNames = new Object();
    for (var i = 0; i < aryNames.length; ++i)
        this.columnNames[aryNames[i]] = i;
}

/*
 * functions the outliner will call to retrieve the list state (nsIOutlinerView.)
 */

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
