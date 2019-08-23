/**
 * Helper method to start a single XUL tree test.
 */
function loadXULTreeAndDoTest(aDoTestFunc, aTreeID, aTreeView) {
  var doTestFunc = aDoTestFunc ? aDoTestFunc : gXULTreeLoadContext.doTestFunc;
  var treeID = aTreeID ? aTreeID : gXULTreeLoadContext.treeID;
  var treeView = aTreeView ? aTreeView : gXULTreeLoadContext.treeView;

  let treeNode = getNode(treeID);

  gXULTreeLoadContext.queue = new eventQueue();
  gXULTreeLoadContext.queue.push({
    treeNode,

    eventSeq: [new invokerChecker(EVENT_REORDER, treeNode)],

    invoke() {
      this.treeNode.view = treeView;
    },

    getID() {
      return "Load XUL tree " + prettyName(treeID);
    },
  });
  gXULTreeLoadContext.queue.onFinish = function() {
    SimpleTest.executeSoon(doTestFunc);
    return DO_NOT_FINISH_TEST;
  };
  gXULTreeLoadContext.queue.invoke();
}

/**
 * Analogy of addA11yLoadEvent, nice helper to load XUL tree and start the test.
 */
function addA11yXULTreeLoadEvent(aDoTestFunc, aTreeID, aTreeView) {
  gXULTreeLoadContext.doTestFunc = aDoTestFunc;
  gXULTreeLoadContext.treeID = aTreeID;
  gXULTreeLoadContext.treeView = aTreeView;

  addA11yLoadEvent(loadXULTreeAndDoTest);
}

function nsTableTreeView(aRowCount) {
  this.__proto__ = new nsTreeView();

  for (var idx = 0; idx < aRowCount; idx++) {
    this.mData.push(new treeItem("row" + String(idx) + "_"));
  }
}

function nsTreeTreeView() {
  this.__proto__ = new nsTreeView();

  this.mData = [
    new treeItem("row1"),
    new treeItem("row2_", true, [
      new treeItem("row2.1_"),
      new treeItem("row2.2_"),
    ]),
    new treeItem("row3_", false, [
      new treeItem("row3.1_"),
      new treeItem("row3.2_"),
    ]),
    new treeItem("row4"),
  ];
}

function nsTreeView() {
  this.mTree = null;
  this.mData = [];
}

nsTreeView.prototype = {
  // ////////////////////////////////////////////////////////////////////////////
  // nsITreeView implementation

  get rowCount() {
    return this.getRowCountIntl(this.mData);
  },
  setTree: function setTree(aTree) {
    this.mTree = aTree;
  },
  getCellText: function getCellText(aRow, aCol) {
    var data = this.getDataForIndex(aRow);
    if (aCol.id in data.colsText) {
      return data.colsText[aCol.id];
    }

    return data.text + aCol.id;
  },
  getCellValue: function getCellValue(aRow, aCol) {
    var data = this.getDataForIndex(aRow);
    return data.value;
  },
  getRowProperties: function getRowProperties(aIndex) {
    return "";
  },
  getCellProperties: function getCellProperties(aIndex, aCol) {
    if (!aCol.cycler) {
      return "";
    }

    var data = this.getDataForIndex(aIndex);
    return this.mCyclerStates[data.cyclerState];
  },
  getColumnProperties: function getColumnProperties(aCol) {
    return "";
  },
  getParentIndex: function getParentIndex(aRowIndex) {
    var info = this.getInfoByIndex(aRowIndex);
    return info.parentIndex;
  },
  hasNextSibling: function hasNextSibling(aRowIndex, aAfterIndex) {},
  getLevel: function getLevel(aIndex) {
    var info = this.getInfoByIndex(aIndex);
    return info.level;
  },
  getImageSrc: function getImageSrc(aRow, aCol) {},
  isContainer: function isContainer(aIndex) {
    var data = this.getDataForIndex(aIndex);
    return data.open != undefined;
  },
  isContainerOpen: function isContainerOpen(aIndex) {
    var data = this.getDataForIndex(aIndex);
    return data.open;
  },
  isContainerEmpty: function isContainerEmpty(aIndex) {
    var data = this.getDataForIndex(aIndex);
    return data.open == undefined;
  },
  isSeparator: function isSeparator(aIndex) {},
  isSorted: function isSorted() {},
  toggleOpenState: function toggleOpenState(aIndex) {
    var data = this.getDataForIndex(aIndex);

    data.open = !data.open;
    var rowCount = this.getRowCountIntl(data.children);

    if (data.open) {
      this.mTree.rowCountChanged(aIndex + 1, rowCount);
    } else {
      this.mTree.rowCountChanged(aIndex + 1, -rowCount);
    }
  },
  selectionChanged: function selectionChanged() {},
  cycleHeader: function cycleHeader(aCol) {},
  cycleCell: function cycleCell(aRow, aCol) {
    var data = this.getDataForIndex(aRow);
    data.cyclerState = (data.cyclerState + 1) % 3;

    this.mTree.invalidateCell(aRow, aCol);
  },
  isEditable: function isEditable(aRow, aCol) {
    return true;
  },
  setCellText: function setCellText(aRow, aCol, aValue) {
    var data = this.getDataForIndex(aRow);
    data.colsText[aCol.id] = aValue;
  },
  setCellValue: function setCellValue(aRow, aCol, aValue) {
    var data = this.getDataForIndex(aRow);
    data.value = aValue;

    this.mTree.invalidateCell(aRow, aCol);
  },

  // ////////////////////////////////////////////////////////////////////////////
  // public implementation

  appendItem: function appendItem(aText) {
    this.mData.push(new treeItem(aText));
  },

  // ////////////////////////////////////////////////////////////////////////////
  // private implementation

  getDataForIndex: function getDataForIndex(aRowIndex) {
    return this.getInfoByIndex(aRowIndex).data;
  },

  getInfoByIndex: function getInfoByIndex(aRowIndex) {
    var info = {
      data: null,
      parentIndex: -1,
      level: 0,
      index: -1,
    };

    this.getInfoByIndexIntl(aRowIndex, info, this.mData, 0);
    return info;
  },

  getRowCountIntl: function getRowCountIntl(aChildren) {
    var rowCount = 0;
    for (var childIdx = 0; childIdx < aChildren.length; childIdx++) {
      rowCount++;

      var data = aChildren[childIdx];
      if (data.open) {
        rowCount += this.getRowCountIntl(data.children);
      }
    }

    return rowCount;
  },

  getInfoByIndexIntl: function getInfoByIndexIntl(
    aRowIdx,
    aInfo,
    aChildren,
    aLevel
  ) {
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
        rowIdx = this.getInfoByIndexIntl(
          rowIdx - 1,
          aInfo,
          data.children,
          aLevel + 1
        );

        if (rowIdx == -1) {
          if (aInfo.parentIndex == -1) {
            aInfo.parentIndex = parentIdx;
          }
          return 0;
        }
      } else {
        rowIdx--;
      }
    }

    return rowIdx;
  },

  mCyclerStates: ["cyclerState1", "cyclerState2", "cyclerState3"],
};

function treeItem(aText, aOpen, aChildren) {
  this.text = aText;
  this.colsText = {};
  this.open = aOpen;
  this.value = "true";
  this.cyclerState = 0;
  if (aChildren) {
    this.children = aChildren;
  }
}

/**
 * Used in conjunction with loadXULTreeAndDoTest and addA11yXULTreeLoadEvent.
 */
var gXULTreeLoadContext = {
  doTestFunc: null,
  treeID: null,
  treeView: null,
  queue: null,
};
