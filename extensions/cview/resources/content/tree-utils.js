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
 * scroll the source so |line| is at either the top, center, or bottom
 * of the view, delepding on the value of |align|.
 *
 * line is the one based target line.
 * if align is negative, the line will be scrolled to the top, if align is
 * zero the line will be centered, and if align is greater than 0 the line
 * will be scrolled to the bottom.  0 is the default.
 */
BasicOView.prototype.scrollTo =
function bov_scrollto (line, align)
{
    var headerRows = 1;
    
    var first = this.outliner.getFirstVisibleRow();
    var last  = this.outliner.getLastVisibleRow();
    var viz   = last - first - headerRows; /* total number of visible rows */

    /* all rows are visible, nothing to scroll */
    if (first == 0 && last > this.rowCount)
    {
        dd ("scrollTo: view does not overflow");
        return;
    }
    
    /* outliner lines are 0 based, we accept one based lines, deal with it */
    --line;

    /* safety clamp */
    if (line < 0)
        line = 0;
    if (line > this.rowCount)
        line = this.rowCount;    

    if (align < 0)
    {
        if (line > this.rowCount - viz) /* overscroll, can't put a row from */
            line = this.rowCount - viz; /* last page at the top. */
        this.outliner.scrollToRow(line);
        return;
    }
    else if (align > 0)
    {
        if (line < viz) /* underscroll, can't put a row from the first page at */
            line = 0;   /* the bottom. */
        else
            line = line - total_viz + headerRows;
        
        this.outliner.scrollToRow(line);
    }
    else
    {
        var half_viz = viz / 2;
        /* lines past this line can't be centered without causing the outliner
         * to show more rows than we have. */
        var lastCenterable = this.rowCount - half_viz;
        if (line > lastCenterable)
            line = lastCenterable;
        /* lines before this can't be centered without causing the outliner
         * to attempt to display negative rows. */
        else if (line < half_viz)
            line = half_viz;
        this.outliner.scrollToRow(line - half_viz);
    }                    
}       

/*
 * functions the outliner will call to retrieve the list state (nsIOutlinerView.)
 */

BasicOView.prototype.rowCount = 0;

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

BasicOView.prototype.isSeparator =
function bov_iscontainer (index)
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

/*
 * record for the TreeOView.  these things take care of keeping the TreeOView
 * properly informed of changes in value and child count.  you shouldn't have
 * to maintain tree state at all.
 *
 * |share| should be an otherwise empty object to store cache data.
 * you should use the same object as the |share| for the TreeOView that you
 * indend to contain these records.
 *
 */
function TreeOViewRecord(share)
{
    this._share = share;
    this.visualFootprint = 1;
    this.isHidden = true; /* records are considered hidden until they are
                           * inserted into a live tree */
}

/*
 * walk the parent tree to find our tree container.  return null if there is
 * none
 */
TreeOViewRecord.prototype.findContainerTree =
function tovr_gettree ()
{
    var parent = this.parentRecord
    
    while (parent)
    {
        if ("_treeView" in parent)
            return parent._treeView;
        parent = parent.parentRecord
    }

    return null;
}

/* count the number of parents, not including the root node */
TreeOViewRecord.prototype.__defineGetter__("level", tovr_getLevel);
function tovr_getLevel ()
{
    var rv = 0;
    var parentRecord = this.parentRecord;
    while ("parentRecord" in parentRecord &&
           (parentRecord = parentRecord.parentRecord)) ++rv;
    return rv;
}

/*
 * associates a property name on this record, with a column in the tree.  This
 * method will set up a get/set pair for the property name you specify which
 * will take care of updating the tree when the value changes.  DO NOT try
 * to change your mind later.  Do not attach a different name to the same colID,
 * and do no rename the colID.  You have been warned.
 */
TreeOViewRecord.prototype.setColumnPropertyName =
function tovr_setcol (colID, propertyName)
{
    function tovr_getValueShim ()
    {
        return this._colValues[colID];
    }
    function tovr_setValueShim (newValue)
    {
        this._colValues[colID] = newValue;
        /* XXX this.invalidate(); */
        return newValue;
    }

    if (!("_colValues" in this))
        this._colValues = new Object();
    
    if (typeof propertyName == "function")
    {
        this._colValues.__defineGetter__(colID, propertyName);
    }
    else
    {
        this.__defineGetter__(propertyName, tovr_getValueShim);
        this.__defineSetter__(propertyName, tovr_setValueShim);
    }
}

/*
 * set the default sort column and resort.
 */
TreeOViewRecord.prototype.setSortColumn =
function tovr_setcol (colID, dir)
{
    //dd ("setting sort column to " + colID);
    this._share.sortColumn = colID;
    this._share.sortDirection = (typeof dir == "undefined") ? 1 : dir;
    this.resort();
}

/*
 * set the default sort direction.  1 is ascending, -1 is descending, 0 is no
 * sort.  setting this to 0 will *not* recover the natural insertion order,
 * it will only affect newly added items.
 */
TreeOViewRecord.prototype.setSortDirection =
function tovr_setdir (dir)
{
    this._share.sortDirection = dir;
}

/*
 * invalidate this row in the outliner
 */
TreeOViewRecord.prototype.invalidate =
function tovr_invalidate()
{
    var tree = this.findContainerTree();
    if (tree)
    {
        var row = this.calculateVisualRow();
        if (row != -1)
            tree.outliner.invalidateRow(row);
    }
}

/*
 * invalidate any data in the cache.
 */
TreeOViewRecord.prototype.invalidateCache =
function tovr_killcache()
{
    this._share.rowCache = new Object();
    this._share.lastComputedIndex = -1;
    this._share.lastIndexOwner = null;
}

/*
 * default comparator function for sorts.  if you want a custom sort, override
 * this method.  We declare tovr_sortcmp as a top level function, instead of
 * a function expression so we can refer to it later.
 */
TreeOViewRecord.prototype.sortCompare = tovr_sortcmp;
function tovr_sortcmp (a, b)
{
    var sc = a._share.sortColumn;
    var sd = a._share.sortDirection;    
    
    a = a[sc];
    b = b[sc];

    if (a < b)
        return -1 * sd;
    
    if (a > b)
        return 1 * sd;
    
    return 0;
}

/*
 * this method will cause all child records to be resorted.  any records
 * with the default sortCompare method will be sorted by the colID passed to
 * setSortColumn.
 *
 * the local parameter is used internally to control whether or not the 
 * sorted rows are invalidated.  don't use it yourself.
 */
TreeOViewRecord.prototype.resort =
function tovr_resort (leafSort)
{
    if (!("childData" in this) || this.childData.length < 1 ||
        (this.childData[0].sortCompare == tovr_sortcmp &&
         !("sortColumn" in this._share) || this._share.sortDirection == 0))
    {
        /* if we have no children, or we have the default sort compare and no
         * sort flags, then just exit */
        return;
    }

    this.childData.sort(this.childData[0].sortCompare);
    
    for (var i = 0; i < this.childData.length; ++i)
    {
        this.childData[i].childIndex = i;
        if ("isContainerOpen" in this.childData[i] &&
            this.childData[i].isContainerOpen)
            this.childData[i].resort(true);
        else
            this.childData[i].sortIsInvalid = true;
    }
    
    if (!leafSort)
    {
        this.invalidateCache();
        var tree = this.findContainerTree();
        if (tree && tree.outliner)
        {
            var rowIndex = this.calculateVisualRow();
            /*
            dd ("invalidating " + rowIndex + " - " +
                (rowIndex + this.visualFootprint - 1));
            */
            tree.outliner.invalidateRange (rowIndex,
                                           rowIndex + this.visualFootprint - 1);
        }
    }
    /*
    else
        dd("not a leafSort");
    */
    delete this.sortIsInvalid;
}
    
/*
 * call this to indicate that this node may have children at one point.  make
 * sure to call it before adding your first child.
 */
TreeOViewRecord.prototype.reserveChildren =
function tovr_rkids ()
{
    if (!("childData" in this))
        this.childData = new Array();
    if (!("isContainerOpen" in this))
        this.isContainerOpen = false;
}

/*
 * add a child to the end of the child list for this record.  takes care of 
 * updating the tree as well.
 */
TreeOViewRecord.prototype.appendChild =
function tovr_appchild (child)
{
    if (!(child instanceof TreeOViewRecord))
        throw Components.results.NS_ERROR_INVALID_PARAM;

    var changeStart = (this.childData.length > 0) ?
        this.childData[this.childData.length - 1].calculateVisualRow() :
        this.calculateVisualRow();
    
    child.isHidden = false;
    child.parentRecord = this;
    child.childIndex = this.childData.length;
    this.childData.push(child);
    
    if ("isContainerOpen" in this && this.isContainerOpen)
    {
        if (this.calculateVisualRow() >= 0)
        {
            this.resort(true);  /* resort, don't invalidate.  we're going to do
                                 * that in the onVisualFootprintChanged call. */
        }
        this.onVisualFootprintChanged(changeStart, child.visualFootprint);
    }
}

/*
 * add a list of children to the end of the child list for this record.
 * faster than multiple appendChild() calls.
 */
TreeOViewRecord.prototype.appendChildren =
function tovr_appchild (children, skipResort)
{
    var changeStart = (this.childData.length > 0) ?
        this.childData[this.childData.length - 1].calculateVisualRow() :
        this.calculateVisualRow();

    var start = this.childData.length;
    var delta = 0;
    var len = children.length;

    for (var i = 0; i <  len; ++i)
    {
        var child = children[i];
        child.isHidden = false;
        child.parentRecord = this;
        this.childData[start + i] = child;
        child.childIndex = i;
        delta += child.visualFootprint;
    }
    
    if ("isContainerOpen" in this && this.isContainerOpen)
    {
        if (!skipResort && this.calculateVisualRow() >= 0)
        {
            this.resort(true);  /* resort, don't invalidate.  we're going to do
                                 * that in the onVisualFootprintChanged call. */
        }
        this.onVisualFootprintChanged(changeStart, delta);
    }
}

/*
 * remove a child from this record. updates the tree too.  DONT call this with
 * an index not actually contained by this record.
 */
TreeOViewRecord.prototype.removeChildAtIndex =
function tovr_remchild (index)
{
    for (var i = index + 1; i < this.childData.length; ++i)
        --this.childData[i].childIndex;
    
    var fpDelta = -this.childData[index].visualFootprint;
    var changeStart = this.childData[index].calculateVisualRow();
    this.childData[index].childIndex = -1;
    delete this.childData[index].parentRecord;
    arrayRemoveAt (this.childData, index);
    this.invalidateCache();
    this.onVisualFootprintChanged (changeStart, fpDelta);
}

/*
 * hide this record and all descendants.
 */
TreeOViewRecord.prototype.hide =
function tovr_hide ()
{
    if (this.isHidden)
        return;

    /* get the row before hiding */
    var row = this.calculateVisualRow();
    this.invalidateCache();
    this.isHidden = true;
    /* go right to the parent so we don't muck with our own visualFoorptint
     * record, we'll need it to be correct if we're ever unHidden. */
    this.parentRecord.onVisualFootprintChanged (row, -this.visualFootprint);
}

/*
 * unhide this record and all descendants.
 */
TreeOViewRecord.prototype.unHide =
function tovr_uhide ()
{
    if (!this.isHidden)
        return;

    this.isHidden = false;
    this.invalidateCache();
    var row = this.calculateVisualRow();
    this.parentRecord.onVisualFootprintChanged (row, this.visualFootprint);
}

/*
 * open this record, exposing it's children.  DONT call this method if the record
 * has no children.
 */
TreeOViewRecord.prototype.open =
function tovr_open ()
{
    if (this.isContainerOpen)
        return;

    if ("onPreOpen" in this)
        this.onPreOpen();
    
    this.isContainerOpen = true;
    var delta = 0;
    for (var i = 0; i < this.childData.length; ++i)
    {
        if (!this.childData[i].isHidden)
            delta += this.childData[i].visualFootprint;
    }

    this.resort(true);
    this.visualFootprint += delta;
    if ("parentRecord" in this)
        this.parentRecord.onVisualFootprintChanged(this.calculateVisualRow(),
                                                   delta);
}

/*
 * close this record, hiding it's children.  DONT call this method if the record
 * has no children, or if it is already closed.
 */
TreeOViewRecord.prototype.close =
function tovr_close ()
{
    if (!this.isContainerOpen)
        return;
    
    this.isContainerOpen = false;
    var delta = 1 - this.visualFootprint;
    this.visualFootprint += delta;
    if (this.parentRecord)
        this.parentRecord.onVisualFootprintChanged(this.calculateVisualRow(),
                                                   delta);
    if (this.onPostClose)
        this.onPostClose();
}

/*
 * called when a node above this one grows or shrinks.  we need to adjust
 * our own visualFootprint to match the change, and pass the message on.
 */
TreeOViewRecord.prototype.onVisualFootprintChanged =
function tovr_vpchange (start, amount)
{
    /* if we're not hidden, but this notification came from a hidden node 
     * (start == -1), ignore it, it doesn't affect us. */
    if (start == -1 && !this.isHidden)
    {
        
        //dd ("vfp change (" + amount + ") from hidden node ignored.");
        return;
    }
    
    this.visualFootprint += amount;

    if ("parentRecord" in this)
        this.parentRecord.onVisualFootprintChanged(start, amount);
}

/*
 * calculate the "visual" row for this record.  If the record isn't actually
 * visible return -1.
 * eg.
 * Name        Visual Row
 * node1       0
 *   node11    1
 *   node12    2
 * node2       3
 *   node21    4
 * node3       5
 */
TreeOViewRecord.prototype.calculateVisualRow =
function tovr_calcrow ()
{
    /* if this is the second time in a row that someone asked us, fetch the last
     * result from the cache. */
    if (this._share.lastIndexOwner == this)
        return this._share.lastComputedIndex;

    var vrow;

        /* if this is an uninserted or hidden node, or... */
    if (!("parentRecord" in this) || (this.isHidden) ||
        /* if parent isn't open, or... */
        (!this.parentRecord.isContainerOpen) ||
        /* parent isn't visible */
        ((vrow = this.parentRecord.calculateVisualRow()) == -1))
    {
        /* then we're not visible, return -1 */
        //dd ("cvr: returning -1");
        return -1;
    }

    /* parent is the root node XXX parent is not visible */
    if (vrow == null)
        vrow = 0;
    else
    /* parent is not the root node, add one for the space they take up. */
        ++vrow;

    /* add in the footprint for all of the earlier siblings */
    for (var i = 0; i < this.childIndex; ++i)
    {
        if (!this.parentRecord.childData[i].isHidden)
            vrow += this.parentRecord.childData[i].visualFootprint;
    }

    /* save this calculation to the cache. */
    this._share.lastIndexOwner = this;
    this._share.lastComputedIndex = vrow;

    //@DEBUG-cvr dd ("cvr: returning " + vrow);
    return vrow;
}

/*
 * locates the child record for the visible row |targetRow|.  DO NOT call this
 * with a targetRow less than this record's visual row, or greater than this
 * record's visual row + the number of visible children it has.
 */
TreeOViewRecord.prototype.locateChildByVisualRow =
function tovr_find (targetRow, myRow)
{
    if (targetRow in this._share.rowCache)
        return this._share.rowCache[targetRow];

    else if (0) {
        /* XXX take this out later */
        if (typeof myRow == "undefined")
            myRow = this.calculateVisualRow();
        else
        {
            ASSERT (myRow == this.calculateVisualRow(), "someone lied to me, " +
                    myRow + " != " + this.calculateVisualRow());
        }
    
        if (targetRow < myRow || targetRow > myRow + this.visualFootprint)
        {
            ASSERT (0, "I don't contain visual row " + targetRow + ", only " +
                    myRow + "..." + (myRow + this.visualFootprint));
            return null;
        }
    }

    /* if this is true, we *are* the index */
    if (targetRow == myRow)
        return (this._share.rowCache[targetRow] = this);

    /* otherwise, we've got to search the kids */
    var childStart = myRow; /* childStart represents the starting visual row
                             * for the child we're examining. */
    for (var i = 0; i < this.childData.length; ++i)
    {
        var child = this.childData[i];
        /* ignore hidden children */
        if (child.isHidden)
            continue;
        /* if this kid is the targetRow, we're done */
        if (childStart == targetRow)
            return (this._share.rowCache[targetRow] = child);
        /* if this kid contains the index, ask *it* to find the record */
        else if (targetRow <= childStart + child.visualFootprint) {
            /* this *has* to succeed */
            var rv = child.locateChildByVisualRow(targetRow, childStart + 1);
            //XXXASSERT (rv, "Can't find a row that *has* to be there.");
            /* don't cache this, the previous call to locateChildByVisualRow
             * just did. */
            return rv;
        }

        /* otherwise, get ready to ask the next kid */
        childStart += child.visualFootprint;
    }

    if (0) {
    /* XXX take this out later */
    ASSERT (0, "locateChildByVisualRow() failed.  Asked for row " + targetRow +
            ", record only contains " + myRow + "..." +
            (myRow + this.visualFootprint));
    }
    return null;
}   

/* TOLabelRecords can be used to drop a label into an arbitrary place in an
 * arbitrary tree.  normally, specializations of TreeOViewRecord are tied to
 * a specific tree because of implementation details.  TOLabelRecords are
 * specially designed (err, hacked) to work around these details.  this makes
 * them slower, but more generic.
 *
 * we set up a getter for _share that defers to the parent object.  this lets
 * TOLabelRecords work in any tree.
 */
function TOLabelRecord (columnName, label, property)
{
    this.setColumnPropertyName (columnName, "label");
    this.label = label;
    this.property = property;
}

TOLabelRecord.prototype = new TreeOViewRecord (null);

TOLabelRecord.prototype.__defineGetter__("_share", tolr_getshare);
function tolr_getshare()
{
    if (this.parentRecord)
        return this.parentRecord._share;
    else
        ASSERT (0, "TOLabelRecord cannot be the root of a visible tree.");
}

/* TORootRecord is used internally by TreeOView, you probably don't need to make
 * any of these */ 
function TORootRecord (tree, share)
{
    this._share = share;
    this._treeView = tree;
    this.visualFootprint = 0;
    this.isHidden = false;
    this.reserveChildren();
    this.isContainerOpen = true;
}

/* no cache passed in here, we set it in the TORootRecord contructor instead. */
TORootRecord.prototype = new TreeOViewRecord (null);

TORootRecord.prototype.open =
TORootRecord.prototype.close =
function torr_notimplemented()
{
    /* don't do this on a root node */
}

TORootRecord.prototype.calculateVisualRow =
function torr_calcrow ()
{
    return null;
}

TORootRecord.prototype.resort =
function torr_resort ()
{
    if (!("childData" in this) || this.childData.length < 1 ||
        (this.childData[0].sortCompare == tovr_sortcmp &&
         !("sortColumn" in this._share) || this._share.sortDirection == 0))
    {
        /* if we have no children, or we have the default sort compare but we're
         * missing a sort flag, then just exit */
        return;
    }
    
    this.childData.sort(this.childData[0].sortCompare);
    
    for (var i = 0; i < this.childData.length; ++i)
    {
        this.childData[i].childIndex = i;
        if ("isContainerOpen" in this.childData[i] &&
            this.childData[i].isContainerOpen)
            this.childData[i].resort(true);
        else
            this.childData[i].sortIsInvalid = true;
    }
    
    if ("_treeView" in this && "outliner" in this._treeView)
    {
        /*
        dd ("root node: invalidating 0 - " + this.visualFootprint +
            " for sort");
        */
        this.invalidateCache();
        this._treeView.outliner.invalidateRange (0, this.visualFootprint);
    }
}

TORootRecord.prototype.locateChildByVisualRow =
function torr_find (targetRow)
{
    if (targetRow in this._share.rowCache)
        return this._share.rowCache[targetRow];

    var childStart = -1; /* childStart represents the starting visual row
                          * for the child we're examining. */
    for (var i = 0; i < this.childData.length; ++i)
    {
        var child = this.childData[i];
        /* ignore hidden children */
        if (child.isHidden)
            continue;
        /* if this kid is the targetRow, we're done */
        if (childStart == targetRow)
            return (this._share.rowCache[targetRow] = child);
        /* if this kid contains the index, ask *it* to find the record */
        else if (targetRow <= childStart + child.visualFootprint) {
            /* this *has* to succeed */
            var rv = child.locateChildByVisualRow(targetRow, childStart + 1);
            //XXXASSERT (rv, "Can't find a row that *has* to be there.");
            /* don't cache this, the previous call to locateChildByVisualRow
             * just did. */
            return rv;
        }

        /* otherwise, get ready to ask the next kid */
        childStart += child.visualFootprint;
    }

    return null;
}

TORootRecord.prototype.onVisualFootprintChanged =
function torr_vfpchange (start, amount)
{
    this.invalidateCache();
    this.visualFootprint += amount;
    if ("_treeView" in this && "outliner" in this._treeView)
    {
        if (amount != 0)
            this._treeView.outliner.rowCountChanged (start, amount);
        else
            this._treeView.outliner.invalidateRow (start);
    }
}

/*
 * TreeOView provides functionality of outliner whose elements have multiple
 * levels of children.
 */

function TreeOView(share)
{
    this.childData = new TORootRecord(this, share);
    this.childData.invalidateCache();
}

/* functions *you* should call to initialize and maintain the outliner state */

/* scroll the line specified by |line| to the center of the outliner */
TreeOView.prototype.centerLine =
function bov_ctrln (line)
{
    var first = this.outliner.getFirstVisibleRow();
    var last = this.outliner.getLastVisibleRow();
    this.scrollToRow(line - total / 2);
}

/*
 * functions the outliner will call to retrieve the list state (nsIOutlinerView.)
 */

TreeOView.prototype.__defineGetter__("rowCount", tov_getRowCount);
function tov_getRowCount ()
{
    return this.childData.visualFootprint;
}

TreeOView.prototype.isContainer =
function tov_isctr (index)
{
    var row = this.childData.locateChildByVisualRow (index);
    /*
    ASSERT(row, "bogus row");
    var rv = Boolean(row && row.childData);
    dd ("isContainer: row " + index + " returning " + rv);
    return rv;
    */

    return Boolean(row && row.childData);
}

TreeOView.prototype.__defineGetter__("selectedIndex", tov_getsel);
function tov_getsel()
{
    if (this.outliner.selection.getRangeCount() < 1)
        return -1;

    var min = new Object();
    this.outliner.selection.getRangeAt(0, min, {});
    return min.value;
}

TreeOView.prototype.__defineSetter__("selectedIndex", tov_setsel);
function tov_setsel(i)
{
    this.outliner.selection.timedSelect (i, 500);
    return i;
}

TreeOView.prototype.scrollTo = BasicOView.prototype.scrollTo;

TreeOView.prototype.isContainerOpen =
function tov_isctropen (index)
{
    var row = this.childData.locateChildByVisualRow (index);
    /*
    ASSERT(row, "bogus row");
    var rv = Boolean(row && row.isContainerOpen);
    dd ("isContainerOpen: row " + index + " returning " + rv);
    return rv;
    */
    return row && row.isContainerOpen;
}

TreeOView.prototype.toggleOpenState =
function tov_toggleopen (index)
{
    var row = this.childData.locateChildByVisualRow (index);
    //ASSERT(row, "bogus row");
    if (row)
    {
        if (row.isContainerOpen)
            row.close();
        else
            row.open();
    }
}

TreeOView.prototype.isContainerEmpty =
function tov_isctrempt (index)
{
    var row = this.childData.locateChildByVisualRow (index);
    /*
    ASSERT(row, "bogus row");
    var rv = Boolean(row && (row.childData.length == 0));
    dd ("isContainerEmpty: row " + index + " returning " + rv);
    return rv;
    */
    return !row || !row.childData;
}

TreeOView.prototype.getParentIndex =
function tov_getpi (index)
{
    var row = this.childData.locateChildByVisualRow (index);
    /*
    ASSERT(row, "bogus row " + index);
    var rv = row.parentRecord.calculateVisualRow();
    dd ("getParentIndex: row " + index + " returning " + rv);
    return rv;
    */
    return row.parentRecord.calculateVisualRow();
}

TreeOView.prototype.hasNextSibling =
function tov_hasnxtsib (rowIndex, afterIndex)
{
    var row = this.childData.locateChildByVisualRow (rowIndex);
    /*
    ASSERT(row, "bogus row");
    rv = Boolean(row.childIndex < row.parentRecord.childData.length - 1);
    dd ("hasNextSibling: row " + rowIndex + ", after " + afterIndex +
        " returning " + rv);
    return rv;
    */
    return row.childIndex < row.parentRecord.childData.length - 1;
}

TreeOView.prototype.getLevel =
function tov_getlvl (index)
{
    var row = this.childData.locateChildByVisualRow (index);
    /*
    ASSERT(row, "bogus row");
    var rv = row.level;
    dd ("getLevel: row " + index + " returning " + rv);
    return rv;
    */
    if (!row)
        return 0;
    
    return row.level;
}

TreeOView.prototype.getCellText =
function tov_getcelltxt (index, colID)
{
    var row = this.childData.locateChildByVisualRow (index);
    //ASSERT(row, "bogus row " + index);
    if (row._colValues)
        return row._colValues[colID];
    else
        return;
}

TreeOView.prototype.getCellProperties =
function tov_cellprops (row, colID, properties)
{}

TreeOView.prototype.getColumnProperties =
function tov_colprops (colID, elem, properties)
{}

TreeOView.prototype.getRowProperties =
function tov_rowprops (index, properties)
{}

TreeOView.prototype.isSeparator =
function tov_isseparator (index)
{
    return false;
}

TreeOView.prototype.isSorted =
function tov_issorted (index)
{
    return false;
}

TreeOView.prototype.canDropOn =
function tov_dropon (index)
{
    var row = this.childData.locateChildByVisualRow (index);
    //ASSERT(row, "bogus row " + index);
    return (row && ("canDropOn" in row) && row.canDropOn());
}

TreeOView.prototype.canDropBeforeAfter =
function tov_dropba (index, before)
{
    var row = this.childData.locateChildByVisualRow (index);
    //ASSERT(row, "bogus row " + index);
    return (row && ("canDropBeforeAfter" in row) &&
            row.canDropBeforeAfter(before));
}

TreeOView.prototype.drop =
function tov_drop (index, orientation)
{
    var row = this.childData.locateChildByVisualRow (index);
    //ASSERT(row, "bogus row " + index);
    return (row && ("drop" in row) && row.drop(orientation));
}

TreeOView.prototype.setOutliner =
function tov_seto (outliner)
{
    this.outliner = outliner;
}

TreeOView.prototype.cycleHeader =
function tov_cyclehdr (colID, elt)
{
}

TreeOView.prototype.selectionChanged =
function tov_selchg ()
{
}

TreeOView.prototype.cycleCell =
function tov_cyclecell (row, colID)
{
}

TreeOView.prototype.isEditable =
function tov_isedit (row, colID)
{
    return false;
}

TreeOView.prototype.setCellText =
function tov_setct (row, colID, value)
{
}

TreeOView.prototype.performAction =
function tov_pact (action)
{
}

TreeOView.prototype.performActionOnRow =
function tov_pactrow (action)
{
}

TreeOView.prototype.performActionOnCell =
function tov_pactcell (action)
{
}
