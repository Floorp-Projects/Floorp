var sqlService = null;

function getSqlService() {
  if (! sqlService)
    sqlService = Components.classes["@mozilla.org/sql/service;1"]
                 .getService(Components.interfaces.mozISqlService);
  return sqlService;
}

function getSelectedAlias() {
  var tree = document.getElementById("aliasesTree");
  return tree.builderView.getResourceAtIndex(tree.currentIndex).Value;
}

function updateButtons() {
  var tree = document.getElementById("aliasesTree");
  const buttons = ["updateButton", "removeButton"];
  for (i = 0; i < buttons.length; i++)
    document.getElementById(buttons[i]).disabled = tree.currentIndex < 0;
}

function addAlias() {
  window.openDialog("aliasDialog.xul", "addAlias", "chrome,modal=yes,resizable=no,centerscreen");
}

function updateAlias() {
  var alias = getSelectedAlias();
  window.openDialog("aliasDialog.xul", "updateDatabase", "chrome,modal=yes,resizable=no,centerscreen", alias);
}

function removeAlias() {
  var sqlService = getSqlService();
  var alias = getSelectedAlias();
  sqlService.removeAlias(alias);  
  updateButtons();
}
