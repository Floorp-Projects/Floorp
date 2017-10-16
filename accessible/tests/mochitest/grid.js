const nsIDOMKeyEvent = Components.interfaces.nsIDOMKeyEvent;

/**
 * Create grid object based on HTML table.
 */
function grid(aTableIdentifier) {
  this.getRowCount = function getRowCount() {
    return this.table.rows.length - (this.table.tHead ? 1 : 0);
  };
  this.getColsCount = function getColsCount() {
    return this.table.rows[0].cells.length;
  };

  this.getRowAtIndex = function getRowAtIndex(aIndex) {
    return this.table.rows[this.table.tHead ? aIndex + 1 : aIndex];
  };

  this.getMaxIndex = function getMaxIndex() {
    return this.getRowCount() * this.getColsCount() - 1;
  };

  this.getCellAtIndex = function getCellAtIndex(aIndex) {
    var colsCount = this.getColsCount();

    var rowIdx = Math.floor(aIndex / colsCount);
    var colIdx = aIndex % colsCount;

    var row = this.getRowAtIndex(rowIdx);
    return row.cells[colIdx];
  };

  this.getIndexByCell = function getIndexByCell(aCell) {
    var colIdx = aCell.cellIndex;

    var rowIdx = aCell.parentNode.rowIndex;
    if (this.table.tHead)
      rowIdx -= 1;

    var colsCount = this.getColsCount();
    return rowIdx * colsCount + colIdx;
  };

  this.getCurrentCell = function getCurrentCell() {
    var rowCount = this.table.rows.length;
    var colsCount = this.getColsCount();
    for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
      for (var colIdx = 0; colIdx < colsCount; colIdx++) {
        var cell = this.table.rows[rowIdx].cells[colIdx];
        if (cell.hasAttribute("tabindex"))
          return cell;
      }
    }
    return null;
  };

  this.initGrid = function initGrid() {
    this.table.addEventListener("keypress", this);
    this.table.addEventListener("click", this);
  };

  this.handleEvent = function handleEvent(aEvent) {
    if (aEvent instanceof nsIDOMKeyEvent)
      this.handleKeyEvent(aEvent);
    else
      this.handleClickEvent(aEvent);
  };

  this.handleKeyEvent = function handleKeyEvent(aEvent) {
    if (aEvent.target.localName != "td")
      return;

    var cell = aEvent.target;
    switch (aEvent.keyCode) {
      case nsIDOMKeyEvent.DOM_VK_UP:
        var colsCount = this.getColsCount();
        var idx = this.getIndexByCell(cell);
        var upidx = idx - colsCount;
        if (upidx >= 0) {
          cell.removeAttribute("tabindex");
          var upcell = this.getCellAtIndex(upidx);
          upcell.setAttribute("tabindex", "0");
          upcell.focus();
        }
        break;

      case nsIDOMKeyEvent.DOM_VK_DOWN:
        var colsCount = this.getColsCount();
        var idx = this.getIndexByCell(cell);
        var downidx = idx + colsCount;
        if (downidx <= this.getMaxIndex()) {
          cell.removeAttribute("tabindex");
          var downcell = this.getCellAtIndex(downidx);
          downcell.setAttribute("tabindex", "0");
          downcell.focus();
        }
        break;

      case nsIDOMKeyEvent.DOM_VK_LEFT:
        var idx = this.getIndexByCell(cell);
        if (idx > 0) {
          cell.removeAttribute("tabindex");
          var prevcell = this.getCellAtIndex(idx - 1);
          prevcell.setAttribute("tabindex", "0");
          prevcell.focus();
        }
        break;

      case nsIDOMKeyEvent.DOM_VK_RIGHT:
        var idx = this.getIndexByCell(cell);
        if (idx < this.getMaxIndex()) {
          cell.removeAttribute("tabindex");
          var nextcell = this.getCellAtIndex(idx + 1);
          nextcell.setAttribute("tabindex", "0");
          nextcell.focus();
        }
        break;
    }
  };

  this.handleClickEvent = function handleClickEvent(aEvent) {
    if (aEvent.target.localName != "td")
      return;

    var curCell = this.getCurrentCell();
    var cell = aEvent.target;

    if (cell != curCell) {
      curCell.removeAttribute("tabindex");
      cell.setAttribute("tabindex", "0");
      cell.focus();
    }
  };

  this.table = getNode(aTableIdentifier);
  this.initGrid();
}
