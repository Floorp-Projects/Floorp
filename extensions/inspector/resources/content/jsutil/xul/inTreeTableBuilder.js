/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/***************************************************************
* inTreeTableBuilder -------------------------------------------------
*  Automatically builds up a tree so that it will display a tabular
*  set of data with titled columns and optionally an icon for each row.
*  The tree that is supplied must begin with no children other than
*  a single template element.  The template is required.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xul/DNDUtils.js
****************************************************************/

//////////// global variables /////////////////////

var inTreeTableBuilderPartCount = 0;

//////////// global constants ////////////////////

////////////////////////////////////////////////////////////////////////////
//// class inTreeTableBuilder

function inTreeTableBuilder(aTree, aNameSpace, aArcName)
{
  this.tree = aTree;
  this.nameSpace = aNameSpace;
  this.arcName = aArcName;

  this.mColumns = [];
  this.mExtras = [];
}

inTreeTableBuilder.prototype =
{
  mTree: null,
  // datasource stuff
  mNameSpace: null,
  mArcName: null,
  mIsRefContainer: true,
  mIsContainer: false,

  // table structure stuff
  mIsIconic: false,
  mColumns: null,
  mSplitters: true,
  mItemFields: null,
  mItemAttrs: null,
  mAllowDND: false,
  mLastDragCol: null,
  mLastDragColWhere: null,

  ////////////////////////////////////////////////////////////////////////////
  //// properties

  // the xul tree node we will construct
  get tree() { return this.mTree },
  set tree(aVal) { this.mTree = aVal; aVal._treeBuilder = this },

  // the namespace to use for all fields
  get nameSpace() { return this.mNameSpace },
  set nameSpace(aVal) { this.mNameSpace = aVal },

  // the name of the arc to the container
  get arcName() { return this.mArcName },
  set arcName(aVal) { this.mArcName = aVal },

  // is the datasource ref an arc to a container, or a container itself
  get isRefContainer() { return this.mIsRefContainer },
  set isRefContainer(aVal) { this.mIsRefContainer = aVal },

  // is each row a potential container?
  get isContainer() { return this.mIsContainer },
  set isContainer(aVal) { this.mIsContainer = aVal },

  // place an icon before the first column of each row
  get isIconic() { return this.mIsIconic },
  set isIconic(aVal) { this.mIsIconic = aVal },

  // put splitters between the columns?
  get useSplitters() { return this.mSplitters },
  set useSplitters(aVal) { this.mSplitters = aVal },

  // extra attributes to set on the treeitem
  get itemAttributes() { return this.mItemAttrs },
  set itemAttributes(aVal) { this.mItemAttrs = aVal },

  // extra data fields to set on the treeitem
  get itemFields() { return this.mItemFields },
  set itemFields(aVal) { this.mItemFields = aVal },

  // extra data fields to set on the treeitem
  get allowDragColumns() { return this.mAllowDND },
  set allowDragColumns(aVal) { this.mAllowDND = aVal },

  ////////////////////////////////////////////////////////////////////////////
  //// event handlers

  get onColumnAdd() { return this.mOnColumnAdd },
  set onColumnAdd(aFn) { this.mOnColumnAdd = aFn },

  get onColumnRemove() { return this.mOnColumnRemove },
  set onColumnRemove(aFn) { this.mOnColumnRemove = aFn },

  ////////////////////////////////////////////////////////////////////////////
  //// initialization

  initialize: function()
  {
    this.initColumns();
    this.initTemplate();
  },

  initColumns: function()
  {
    // initialize the treehead
    var treehead = document.createElementNS(kXULNSURI, "treehead");
    var treerow = document.createElementNS(kXULNSURI, "treerow");
    this.mTreeHeadRow = treerow;

    treehead.appendChild(treerow);
    this.mTree.appendChild(treehead);

    // initialize the treecolgroup
    var colgroup = document.createElementNS(kXULNSURI, "treecolgroup");
    this.mColGroup = colgroup;
    this.mTree.appendChild(colgroup);

    this.initDND();
  },

  initTemplate: function()
  {
    var template = this.mTree.getElementsByTagNameNS(kXULNSURI, "template")[0];
    this.mTemplate = template;
    this.clearChildren(template);

    var rule = document.createElementNS(kXULNSURI, "rule");
    template.appendChild(rule);
    this.mRule = rule;

    this.createDefaultConditions();

    var bindings = document.createElementNS(kXULNSURI, "bindings");
    rule.appendChild(bindings);
    this.mBindings = bindings;

    this.createDefaultAction();
  },

  createDefaultConditions: function()
  {
    var conditions = document.createElementNS(kXULNSURI, "conditions");
    this.mRule.appendChild(conditions);

    var content = document.createElementNS(kXULNSURI, "content");
    content.setAttribute("uri", "?uri");
    conditions.appendChild(content);

    var triple = this.createTriple("?uri", this.mNameSpace+this.mArcName,
      this.mIsRefContainer ? "?table" : "?item");
    conditions.appendChild(triple);

    if (this.mIsRefContainer) {
      var member = this.createMember("?table", "?item");
      conditions.appendChild(member);
    }
  },

  createDefaultAction: function()
  {
    var action = document.createElementNS(kXULNSURI, "action");
    this.mRule.appendChild(action);

    var treechildren = this.createTemplatePart("treechildren");
    action.appendChild(treechildren);
    treechildren.setAttribute("flex", "1");
    var treeitem = this.createTemplatePart("treeitem");
    treechildren.appendChild(treeitem);
    treeitem.setAttribute("uri", "?item");
    this.mTreeItem = treeitem;
    var treerow = this.createTemplatePart("treerow");
    treeitem.appendChild(treerow);
    this.mTreeRow = treerow;

    // assign the item attributes
    if (this.mItemAttrs)
      for (key in this.mItemAttrs)
        treeitem.setAttribute(key, this.mItemAttrs[key]);
  },

  createDefaultBindings: function()
  {
    // assign the item fields
    var binding;
    if (this.mItemFields) {
      for (key in this.mItemFields) {
        binding = this.createBinding(this.mItemFields[key]);
        this.mBindings.appendChild(binding);
        this.mTreeItem.setAttribute(key, "?" + this.mItemFields[key]);
      }
    }
  },

  createTriple: function(aSubject, aPredicate, aObject)
  {
    var triple = document.createElementNS(kXULNSURI, "triple");
    triple.setAttribute("subject", aSubject);
    triple.setAttribute("predicate", aPredicate);
    triple.setAttribute("object", aObject);

    return triple;
  },

  createMember: function(aContainer, aChild)
  {
    var member = document.createElementNS(kXULNSURI, "member");
    member.setAttribute("container", aContainer);
    member.setAttribute("child", aChild);

    return member;
  },

  reset: function()
  {
    this.mColumns = [];
    this.mIsIconic = false;

    this.resetTree();
  },

  resetTree: function()
  {
    this.clearChildren(this.mColGroup);
    this.clearChildren(this.mTreeHeadRow);
    this.clearChildren(this.mBindings);
    this.clearChildren(this.mTreeRow);
    this.createDefaultBindings();
  },

  clearChildren: function(aEl)
  {
    while (aEl.childNodes.length)
      aEl.removeChild(aEl.lastChild);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// column drag and drop

  initDND: function()
  {
    if (this.mAllowDND) {
      // addEventListener doesn't work for dnd events, apparently... so we use the attributes
      this.addDNDListener(this.mTree, "ondragenter");
      this.addDNDListener(this.mTree, "ondragover");
      this.addDNDListener(this.mTree, "ondragexit");
      this.addDNDListener(this.mTree, "ondraggesture");
      this.addDNDListener(this.mTree, "ondragdrop");
    }
  },

  onDragEnter: function(aEvent)
  {
  },

  onDragOver: function(aEvent)
  {
    if (!DNDUtils.checkCanDrop("TreeTableBuilder/column-add"))
      return;

    var idx = this.getColumnIndexFromX(aEvent.clientX, 50);
    this.mColumnInsertIndex = idx;
    var kids = this.mColGroup.childNodes;
    var col = this.getColumnAt(idx);
    if (idx == -1)
      this.markColumnInsert(col, "after", idx);
    else
      this.markColumnInsert(col, "before", idx);
  },

  onDragExit: function(aEvent)
  {
  },

  onDragDrop: function(aEvent)
  {
    this.markColumnInsert(null);
    var dragService = XPCU.getService("@mozilla.org/widget/dragservice;1", "nsIDragService");
    var dragSession = dragService.getCurrentSession();

    if (!dragSession.isDataFlavorSupported("TreeTableBuilder/column-add"))
      return;

    var trans = XPCU.createInstance("@mozilla.org/widget/transferable;1", "nsITransferable");
    trans.addDataFlavor("TreeTableBuilder/column-add");

    dragSession.getData(trans, 0);
    var data = new Object();
    trans.getAnyTransferData ({}, data, {});

    var string = XPCU.QI(data.value, "nsISupportsWString");

    this.insertColumn(this.mColumnInsertIndex,
      {
        name: string.data,
        title: string.data,
        flex: 1
      });
    this.build();
    // if we rebuildContent during this thread, it will crash in the dnd service
    setTimeout(function(aTreeBuilder) { aTreeBuilder.buildContent() }, 1, this);

    // bug 56270 - dragSession.sourceDocument is null --
    // causes me to code this very temporary, very nasty hack
    // to tell columnsDialog.js about the drop
    if (this.mTree.onClientDrop) {
      this.mTree.onClientDrop();
    }
  },

  markColumnInsert: function (aColumn, aWhere, aIdx)
  {
    var col = this.mLastDragCol;
    var lastWhere = this.mLastDragColWhere;
    if (col)
      col.setAttribute("dnd-insert-"+lastWhere, "false");

    if (aWhere != "before" && aWhere != "after") {
      this.mLastDragCol = null;
      this.mLastDragColWhere = null;
      return;
    }

    col = aColumn;
    if (col) {
      this.mLastDragCol = col;
      this.mLastDragColWhere = aWhere;
      col.setAttribute("dnd-insert-"+aWhere, "true");
    }
  },

  getColumnIndexFromX: function(aX, aThresh)
  {
    var colgroup = this.mColGroup;
    var width = 0, cw;
    var col;
    var pct = aThresh/100;
    for (var i = 0; i < this.columnCount; ++i) {
      col = this.getColumnAt(i);
      cw = col.boxObject.width;
      width += cw;
      if (width-(cw*pct) > aX)
        return i;
    }

    return -1;
  },

  getColumnIndexFromHeader: function(aHeader)
  {
    var headers = this.mTreeHeadRow.childNodes;
    for (var i = 0; i < headers.length; ++i) {
      if (headers[i] == aHeader)
        return i;
    }
    return -1;
  },

  //// for drag-n-drop removal/arrangement of columns

  onDragGesture: function(aEvent)
  {
    var target = aEvent.target;
    if (target.parentNode == this.mTreeHeadRow) {
      var column = target.getAttribute("label");

      var idx = this.getColumnIndexFromHeader(target);
      if (idx == -1) return;
      this.mColumnDragging = idx;

      DNDUtils.invokeSession(target, ["TreeTableBuilder/column-remove"], [column]);
    }
  },

  addColumnDropTarget: function(aBox)
  {
    aBox._treeBuilderDropTarget = this;
    this.addDNDListener(aBox, "ondragover", "Target");
    this.addDNDListener(aBox, "ondragdrop", "Target");
  },

  removeColumnDropTarget: function(aBox)
  {
    aBox._treeBuilderDropTarget = this;
    this.removeDNDListener(aBox, "ondragover", "Target");
    this.removeDNDListener(aBox, "ondragdrop", "Target");
  },

  onDragOverTarget: function(aBox, aEvent)
  {
    DNDUtils.checkCanDrop("TreeTableBuilder/column-remove");
  },

  onDragDropTarget: function(aBox, aEvent)
  {
    this.removeColumn(this.mColumnDragging);
    this.build();
    // if we rebuildContent during this thread, it will crash in the dnd service
    setTimeout(function(aTreeBuilder) { aTreeBuilder.buildContent() }, 1, this);
  },

  // these are horrible hacks to compensate for the lack of true multiple
  // listener support for the DND events

  addDNDListener: function(aBox, aType, aModifier)
  {
   var js = "inTreeTableBuilder_"+aType+(aModifier?"_"+aModifier:"")+"(this, event);";

   var attr = aBox.getAttribute(aType);
   attr = attr ? attr : "";
   aBox.setAttribute(aType, attr + js);
  },

  removeDNDListener: function(aBox, aType, aModifier)
  {
   var js = "inTreeTableBuilder_"+aType+(aModifier?"_"+aModifier:"")+"(this, event);";

   var attr = aBox.getAttribute(aType);
   var idx = attr.indexOf(js);
   if (idx != -1)
     attr = attr.substr(0,idx) + attr.substr(idx+1);
   aBox.setAttribute(aType, attr);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// column properties

  addColumn: function(aData)
  {
    this.mColumns.push(aData);

    if (this.mOnColumnAdd)
      this.mOnColumnAdd(this.mColumns.length-1)
  },

  insertColumn: function(aIndex, aData)
  {
    var idx;
    if (aIndex == -1) {
      this.mColumns.push(aData);
      idx = this.mColumns.length-1;
    } else {
      this.mColumns.splice(aIndex, 0, aData);
      idx = aIndex;
    }

    if (this.mOnColumnAdd)
      this.mOnColumnAdd(idx);
  },

  removeColumn: function(aIndex)
  {
    this.mColumns.splice(aIndex, 1);

    if (this.mOnColumnRemove)
      this.mOnColumnRemove(aIndex);
  },

  getColumnAt: function(aIndex)
  {
    var idx;
    var kids = this.mColGroup.childNodes;
    if (aIndex == -1)
      idx = kids.length-1;
    else {
      // step by 2 to skip over splitters, if they are used
      var step = this.mSplitters ? 2 : 1;
      idx = aIndex*step;
    }
    return kids[idx]
  },

  get columnCount() { return this.mColumns.length },

  getColumnName: function(aIndex) { return this.mColumns[aIndex].name  },
  setColumnName: function(aIndex, aValue) { this.mColumns[aIndex].name = aValue },

  getColumnTitle: function(aIndex) { return this.mColumns[aIndex].title  },
  setColumnTitle: function(aIndex, aValue) { this.mColumns[aIndex].title = aValue },

  getColumnClassName: function(aIndex) { return this.mColumns[aIndex].className  },
  setColumnClassName: function(aIndex, aValue) { this.mColumns[aIndex].className = aValue },

  getColumnFlex: function(aIndex) { return this.mColumns[aIndex].flex  },
  setColumnFlex: function(aIndex, aValue) { this.mColumns[aIndex].flex = aValue },

  getExtras: function(aIndex) { return this.mColumns[aIndex].extras  },
  setExtras: function(aIndex, aValue) { this.mColumns[aIndex].extras = aExtras },

  getAttrs: function(aIndex) { return this.mColumns[aIndex].attrs  },
  setAttrs: function(aIndex, aValue) { this.mColumns[aIndex].attrs = aExtras },

  ////////////////////////////////////////////////////////////////////////////
  //// template building

  build: function(aBuildContent)
  {
    try {
      this.resetTree();
      this.buildColGroup();
      this.buildTreeHead();
      this.buildTemplate();
      if (aBuildContent)
        this.buildContent();
    } catch (ex) {
      debug("### ERROR - inTreeTableBuilder::build failed.\n" + ex);
    }

    //if ("dumpDOM" in window)
      //dumpDOM(this.mTemplate);

  },

  buildContent: function()
  {
    this.mTree.builder.rebuild();
  },

  buildColGroup: function()
  {
    var cols = this.mColumns;
    var colgroup = this.mColGroup;
    var col, val, split;
    for (var i = 0; i < cols.length; i++) {
      col = document.createElementNS(kXULNSURI, "treecol");
      col.setAttribute("id", "treecol-"+cols[i].name);
      col.setAttribute("persist", "width");
      val = cols[i].className;
      if (val)
        col.setAttribute("class", val);
      val = cols[i].flex;
      if (val)
        col.setAttribute("flex", val);

      colgroup.appendChild(col);

      if (this.mSplitters && i < cols.length-1) {
        split = document.createElementNS(kXULNSURI, "splitter");
        split.setAttribute("class", "tree-splitter");
        colgroup.appendChild(split);
      }
    }
  },

  buildTreeHead: function()
  {
    var cols = this.mColumns;
    var row = this.mTreeHeadRow;
    var cell, val;
    for (var i = 0; i < cols.length; i++) {
      cell = document.createElementNS(kXULNSURI, "treecell");
      cell.setAttribute("class", "treecell-header");
      val = cols[i].title;
      if (val)
        cell.setAttribute("label", val);
      row.appendChild(cell);
    }
  },

  buildTemplate: function()
  {
    var cols = this.mColumns;
    var bindings = this.mBindings;
    var row = this.mTreeRow;
    var cell, binding, val, extras, attrs, key, className;

    if (this.mIsIconic) {
      binding = this.createBinding("_icon");
      bindings.appendChild(binding);
    }

    for (var i = 0; i < cols.length; ++i) {
      val = cols[i].name;
      if (!val)
        throw "Column data is incomplete - missing name at index " + i + ".";

      cell = this.createTemplatePart("treecell");
      className = "";

      // build the icon data field
      if (i == 0 && this.mIsIconic) {
        className += "treecell-iconic ";
        cell.setAttribute("src", "?_icon");
      }

      // mark first node as a container, if necessary
      if (i == 0 && this.mIsContainer)
        className += "treecell-indent";

      // build the default value data field
      binding = this.createBinding(val);
      bindings.appendChild(binding);
      cell.setAttribute("label", "?" + val);

      cell.setAttribute("class", className);
      row.appendChild(cell);
    }
  },

  createBinding: function(aName)
  {
    var binding = document.createElementNS(kXULNSURI, "binding");
    binding.setAttribute("subject", "?item");
    binding.setAttribute("predicate", this.mNameSpace+aName);
    binding.setAttribute("object", "?" + aName);
    return binding;
  },

  createTemplatePart: function(aTagName)
  {
    var el = document.createElementNS(kXULNSURI, aTagName);
    el.setAttribute("id", "templatePart"+inTreeTableBuilderPartCount);
    inTreeTableBuilderPartCount++;
    return el;
  }

};

function inTreeTableBuilder_ondraggesture(aTree, aEvent)
{
  return aTree._treeBuilder.onDragGesture(aEvent);
}

function inTreeTableBuilder_ondragenter(aTree, aEvent)
{
  return aTree._treeBuilder.onDragEnter(aEvent);
}

function inTreeTableBuilder_ondragover(aTree, aEvent)
{
  return aTree._treeBuilder.onDragOver(aEvent);
}

function inTreeTableBuilder_ondragexit(aTree, aEvent)
{
  return aTree._treeBuilder.onDragExit(aEvent);
}

function inTreeTableBuilder_ondragdrop(aTree, aEvent)
{
  return aTree._treeBuilder.onDragDrop(aEvent);
}

function inTreeTableBuilder_ondragover_Target(aBox, aEvent)
{
  return aBox._treeBuilderDropTarget.onDragOverTarget(aBox, aEvent);
}

function inTreeTableBuilder_ondragdrop_Target(aBox, aEvent)
{
  return aBox._treeBuilderDropTarget.onDragDropTarget(aBox, aEvent);
}
