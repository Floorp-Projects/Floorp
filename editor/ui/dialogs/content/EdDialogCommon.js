function onUndo() {
  if (undoCount > 0)
  {
    dump("Undo count = "+undoCount+"\n");
    undoCount = undoCount - 1;
    appCore.undo();
  }
}

function onOK() {
  applyChanges();
  //TODO: Allow this after bug 6005 is fixed
  //toolkitCore.CloseWindow(window);
}

function onCancel() {
  // Undo all actions performed within the dialog
  // TODO: We need to suppress reflow/redraw untill all levels are undone
  while (undoCount > 0) {
    onUndo();
  }
  //TODO: Allow this after bug 6005 is fixed
  //toolkitCore.CloseWindow(window);
}

