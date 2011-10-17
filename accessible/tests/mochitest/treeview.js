function nsTableTreeView(aRowCount)
{
  this.__proto__ = new nsTreeView();

  for (var idx = 0; idx < aRowCount; idx++)
    this.mData.push(new treeItem("row" + String(idx) + "_"));
}

function nsTreeTreeView()
{
  this.__proto__ = new nsTreeView();
  
  this.mData = [
    new treeItem("row1"),
    new treeItem("row2_", true, [new treeItem("row2.1_"), new treeItem("row2.2_")]),
    new treeItem("row3_", false, [new treeItem("row3.1_"), new treeItem("row3.2_")]),
    new treeItem("row4")
  ];
}

function nsTreeView()
{
  this.mTree = null;
  this.mData = [];
}

nsTreeView.prototype =
{
  //////////////////////////////////////////////////////////////////////////////
  // nsITreeView implementation

  get rowCount()
  {
    return this.getRowCountIntl(this.mData);
  },
  setTree: function setTree(aTree)
  {
    this.mTree = aTree;
  },
  getCellText: function getCellText(aRow, aCol)
  {
    var data = this.getDataForIndex(aRow);
    if (aCol.id in data.colsText)
      return data.colsText[aCol.id];

    return data.text + aCol.id;
  },
  getCellValue: function getCellValue(aRow, aCol)
  {
    var data = this.getDataForIndex(aRow);
    return data.value;
  },
  getRowProperties: function getRowProperties(aIndex, aProperties) {},
  getCellProperties: function getCellProperties(aIndex, aCol, aProperties)
  {
    if (!aCol.cycler)
      return;

    var data = this.getDataForIndex(aIndex);
    var atom = this.mCyclerStates[data.cyclerState];
    aProperties.AppendElement(atom);
  },
  getColumnProperties: function getColumnProperties(aCol, aProperties) {},
  getParentIndex: function getParentIndex(aRowIndex)
  {
    var info = this.getInfoByIndex(aRowIndex);
    return info.parentIndex;
  },
  hasNextSibling: function hasNextSibling(aRowIndex, aAfterIndex) { },
  getLevel: function getLevel(aIndex)
  {
    var info = this.getInfoByIndex(aIndex);
    return info.level;
  },
  getImageSrc: function getImageSrc(aRow, aCol) {},
  getProgressMode: function getProgressMode(aRow, aCol) {},
  isContainer: function isContainer(aIndex)
  {
    var data = this.getDataForIndex(aIndex);
    return data.open != undefined;
  },
  isContainerOpen: function isContainerOpen(aIndex)
  {
    var data = this.getDataForIndex(aIndex);
    return data.open;
  },
  isContainerEmpty: function isContainerEmpty(aIndex)
  {
    var data = this.getDataForIndex(aIndex);
    return data.open == undefined;
  },
  isSeparator: function isSeparator(aIndex) {},
  isSorted: function isSorted() {},
  toggleOpenState: function toggleOpenState(aIndex)
  {
    var data = this.getDataForIndex(aIndex);

    data.open = !data.open;
    var rowCount = this.getRowCountIntl(data.children);

    if (data.open) 
      this.mTree.rowCountChanged(aIndex + 1, rowCount);
    else
      this.mTree.rowCountChanged(aIndex + 1, -rowCount);
  },
  selectionChanged: function selectionChanged() {},
  cycleHeader: function cycleHeader(aCol) {},
  cycleCell: function cycleCell(aRow, aCol)
  {
    var data = this.getDataForIndex(aRow);
    data.cyclerState = (data.cyclerState + 1) % 3;

    this.mTree.invalidateCell(aRow, aCol);
  },
  isEditable: function isEditable(aRow, aCol)
  {
    return true;
  },
  isSelectable: function isSelectable(aRow, aCol) {},
  setCellText: function setCellText(aRow, aCol, aValue)
  {
    var data = this.getDataForIndex(aRow);
    data.colsText[aCol.id] = aValue;
  },
  setCellValue: function setCellValue(aRow, aCol, aValue)
  {
    var data = this.getDataForIndex(aRow);
    data.value = aValue;

    this.mTree.invalidateCell(aRow, aCol);
  },
  performAction: function performAction(aAction) {},
  performActionOnRow: function performActionOnRow(aAction, aRow) {},
  performActionOnCell: function performActionOnCell(aAction, aRow, aCol) {},

  //////////////////////////////////////////////////////////////////////////////
  // public implementation

  appendItem: function appendItem(aText)
  {
    this.mData.push(new treeItem(aText));
  },

  //////////////////////////////////////////////////////////////////////////////
  // private implementation

  getDataForIndex: function getDataForIndex(aRowIndex)
  {
    return this.getInfoByIndex(aRowIndex).data;
  },

  getInfoByIndex: function getInfoByIndex(aRowIndex)
  {
    var info = {
      data: null,
      parentIndex: -1,
      level: 0,
      index: -1
    };

    this.getInfoByIndexIntl(aRowIndex, info, this.mData, 0);
    return info;
  },

  getRowCountIntl: function getRowCountIntl(aChildren)
  {
    var rowCount = 0;
    for (var childIdx = 0; childIdx < aChildren.length; childIdx++) {
      rowCount++;

      var data = aChildren[childIdx];      
      if (data.open)
        rowCount += this.getRowCountIntl(data.children);
    }

    return rowCount;
  },

  getInfoByIndexIntl: function getInfoByIndexIntl(aRowIdx, aInfo,
                                                  aChildren, aLevel)
  {
    var rowIdx = aRowIdx;
    for (var childIdx = 0; childIdx < aChildren.length; childIdx++) {
      var data = aChildren[childIdx];

      aInfo.index++;

      if (rowIdx == 0) {
        aInfo.data = data;
        aInfo.level = aLevel;
        return -1;
      }

      if (data.open) {
        var parentIdx = aInfo.index;
        rowIdx = this.getInfoByIndexIntl(rowIdx - 1, aInfo, data.children,
                                         aLevel + 1);

        if (rowIdx == -1) {
          if (aInfo.parentIndex == -1)
            aInfo.parentIndex = parentIdx;
          return 0;
        }
      } else {
        rowIdx--;
      }
    }

    return rowIdx;
  },

  mCyclerStates: [
    createAtom("cyclerState1"),
    createAtom("cyclerState2"),
    createAtom("cyclerState3")
  ]
};

function treeItem(aText, aOpen, aChildren)
{
  this.text = aText;
  this.colsText = {};
  this.open = aOpen;
  this.value = "true";
  this.cyclerState = 0;
  if (aChildren)
    this.children = aChildren;
}

function createAtom(aName)
{
  return Components.classes["@mozilla.org/atom-service;1"]
    .getService(Components.interfaces.nsIAtomService).getAtom(aName);
}
