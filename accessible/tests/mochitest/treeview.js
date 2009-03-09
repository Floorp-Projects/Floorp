function inTreeView() { }

inTreeView.prototype =
{
  mRowCount: 0,
  mTree: null,
  mData: {},

  get rowCount()
  {
    return this.mRowCount;
  },
  setTree: function setTree(aTree)
  {
    this.mTree = aTree;
  },
  getCellText: function getCellText(aRow, aCol)
  {
    var key = String(aRow)  + aCol.id;
    if (key in this.mData)
      return this.mData[key];
    
    return "hello";
  },
  getRowProperties: function getRowProperties(aIndex, aProperties) {},
  getCellProperties: function getCellProperties(aIndex, aCol, aProperties) {},
  getColumnProperties: function getColumnProperties(aCol, aProperties) {},
  getParentIndex: function getParentIndex(aRowIndex) { },
  hasNextSibling: function hasNextSibling(aRowIndex, aAfterIndex) { },
  getLevel: function getLevel(aIndex) {},
  getImageSrc: function getImageSrc(aRow, aCol) {},
  getProgressMode: function getProgressMode(aRow, aCol) {},
  getCellValue: function getCellValue(aRow, aCol) {},
  isContainer: function isContainer(aIndex) {},
  isContainerOpen: function isContainerOpen(aIndex) {},
  isContainerEmpty: function isContainerEmpty(aIndex) {},
  isSeparator: function isSeparator(aIndex) {},
  isSorted: function isSorted() {},
  toggleOpenState: function toggleOpenState(aIndex) {},
  selectionChanged: function selectionChanged() {},
  cycleHeader: function cycleHeader(aCol) {},
  cycleCell: function cycleCell(aRow, aCol) {},
  isEditable: function isEditable(aRow, aCol) {},
  isSelectable: function isSelectable(aRow, aCol) {},
  setCellValue: function setCellValue(aRow, aCol, aValue) {},
  setCellText: function setCellText(aRow, aCol, aValue) { },
  performAction: function performAction(aAction) {},
  performActionOnRow: function performActionOnRow(aAction, aRow) {},
  performActionOnCell: function performActionOnCell(aAction, aRow, aCol) {}
};
