function DeleteAllFromTree
    (tree, view, table, deletedTable, removeButton, removeAllButton) {

  // remove all items from table and place in deleted table
  for (var i=0; i<table.length; i++) {
    deletedTable[deletedTable.length] = table[i];
  }
  table.length = 0;

  // clear out selections
  tree.treeBoxObject.view.selection.select(-1); 

  // redisplay
  view.rowCount = 0;
  tree.treeBoxObject.invalidate();

  // disable buttons
  document.getElementById(removeButton).setAttribute("disabled", "true")
  document.getElementById(removeAllButton).setAttribute("disabled","true");
}

function DeleteSelectedItemFromTree
    (tree, view, table, deletedTable, removeButton, removeAllButton) {

  // remove selected items from list (by setting them to null) and place in deleted list
  var selections = GetTreeSelections(tree);
  for (var s=selections.length-1; s>= 0; s--) {
    var i = selections[s];
    deletedTable[deletedTable.length] = table[i];
    table[i] = null;
  }

  // collapse list by removing all the null entries
  for (var j=0; j<table.length; j++) {
    if (table[j] == null) {
      var k = j;
      while ((k < table.length) && (table[k] == null)) {
        k++;
      }
      table.splice(j, k-j);
    }
  }

  // redisplay
  var box = tree.treeBoxObject;
  var firstRow = box.getFirstVisibleRow();
  if (firstRow > (table.length-1) ) {
    firstRow = table.length-1;
  }
  view.rowCount = table.length;
  box.rowCountChanged(0, table.length);
  box.scrollToRow(firstRow)

  // update selection and/or buttons
  if (table.length) {

    // update selection
    // note: we need to deselect before reselecting in order to trigger ...Selected method
    var nextSelection = (selections[0] < table.length) ? selections[0] : table.length-1;
    tree.treeBoxObject.view.selection.select(-1); 
    tree.treeBoxObject.view.selection.select(nextSelection);

  } else {

    // disable buttons
    document.getElementById(removeButton).setAttribute("disabled", "true")
    document.getElementById(removeAllButton).setAttribute("disabled","true");

    // clear out selections
    tree.treeBoxObject.view.selection.select(-1); 
  }
}

function GetTreeSelections(tree) {
  var selections = [];
  var select = tree.treeBoxObject.selection;
  if (select) {
    var count = select.getRangeCount();
    var min = new Object();
    var max = new Object();
    for (var i=0; i<count; i++) {
      select.getRangeAt(i, min, max);
      for (var k=min.value; k<=max.value; k++) {
        if (k != -1) {
          selections[selections.length] = k;
        }
      }
    }
  }
  return selections;
}

function SortTree(tree, view, table, column, lastSortColumn, lastSortAscending, updateSelection) {

  // remember which item was selected so we can restore it after the sort
  var selections = GetTreeSelections(tree);
  var selectedNumber = selections.length ? table[selections[0]].number : -1;

  // determine if sort is to be ascending or descending
  var ascending = (column == lastSortColumn) ? !lastSortAscending : true;

  // do the sort
  var compareFunc;
  if (ascending) {
    compareFunc = function compare(first, second) {
      if (first[column] < second[column])
        return -1;
      if (first[column] > second[column])
        return 1;
      return 0;
    }
  } else {
    compareFunc = function compare(first, second) {
      if (first[column] < second[column])
        return 1;
      if (first[column] > second[column])
        return -1;
      return 0;
    }
  }
  table.sort(compareFunc);

  // restore the selection
  var selectedRow = -1;
  if (selectedNumber>=0 && updateSelection) {
    for (var s=0; s<table.length; s++) {
      if (table[s].number == selectedNumber) {
        // update selection
        // note: we need to deselect before reselecting in order to trigger ...Selected()
        tree.treeBoxObject.view.selection.select(-1);
        tree.treeBoxObject.view.selection.select(s);
        selectedRow = s;
        break;
      }
    }
  }

  // display the results
  tree.treeBoxObject.invalidate();
  if (selectedRow >= 0) {
    tree.treeBoxObject.ensureRowIsVisible(selectedRow)
  }

  return ascending;
}
