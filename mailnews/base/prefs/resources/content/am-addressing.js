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

  // this is the hidden text element that assigned a value from the prefs
  var overrideGlobalPref = document.getElementById("identity.overrideGlobalPref");

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
  var attrVal=overrideGlobalPref.getAttribute("disabled");
  document.getElementById("ldapAutocomplete").disabled=attrVal;

  // if the pref is locked, we'll need to disable the elements
  if (overrideGlobalPref.getAttribute("disabled") == "true") {
    directoriesList.setAttribute("disabled", true);
    directoriesListPopup.setAttribute("disabled", true);
    editButton.setAttribute("disabled", true);
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
  
  // When switching between panes, 
  // if we save the value of an element as null
  // we will be forced to get the value from preferences when we get back.
  // We are saving the value as "" for the radio button and also for
  // the directory server if the selected directory is "None"
  // So, we need the two elements overrideGlobalPref and directoryServer
  // to save the state when the directory is 
  // set to none and the first radio button is selected.
  switch(autocomplete.selectedItem.value)
  {
    case "0":
      override.setAttribute('value', "");
      document.getElementById("overrideGlobalPref").setAttribute("value", "0");
      document.getElementById("directoryServer").setAttribute("value", "");
      break;
    case "1":
      override.setAttribute('value', true);
      directoryServer.setAttribute("value", directoriesList);
      document.getElementById("overrideGlobalPref").setAttribute("value", "");
      if(directoriesList == "")
        document.getElementById("directoryServer").setAttribute("value", "none");
      else
        document.getElementById("directoryServer").setAttribute("value", "");
      break;
  } 
}
