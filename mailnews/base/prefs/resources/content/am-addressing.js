function onLoad()
{
  createDirectoriesList(false);
  parent.onPanelLoaded('am-addressing.xul');
}

function onInit() 
{
  setupDirectoriesList();
  enabling();
}

function enabling()
{
  var autocomplete = document.getElementById("ldapAutocomplete");
  var directoriesList =  document.getElementById("directoriesList"); 
  var directoriesListPopup = document.getElementById("directoriesListPopup");
  var editButton = document.getElementById("editButton");

  switch(autocomplete.selectedItem.value)
  {
    case "0":
      directoriesList.setAttribute("disabled", true);
      directoriesListPopup.setAttribute("disabled", true);
      editButton.setAttribute("disabled", true);
      break;
    case "1":
      directoriesList.removeAttribute("disabled");
      directoriesListPopup.removeAttribute("disabled");
      editButton.removeAttribute("disabled");
      break;      
  }
  gFromGlobalPref = false;
  LoadDirectories(directoriesListPopup);
}

function onSave()
{
  var override = document.getElementById("identity.overrideGlobalPref");
  var autocomplete = document.getElementById("ldapAutocomplete");
  var directoryServer = document.getElementById("identity.directoryServer");
  var directoriesList = 
      document.getElementById("directoriesList").getAttribute('value');
  
  switch(autocomplete.selectedItem.value)
  {
    case "0":
      override.setAttribute('value', "");
      break;
    case "1":
      override.setAttribute('value', true);
      directoryServer.setAttribute("value", directoriesList);
      break;
  } 
}
