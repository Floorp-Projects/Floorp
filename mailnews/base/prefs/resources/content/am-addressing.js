var gIdentity = null;
var gPrefInt = null;

function onLoad()
{
  createDirectoriesList();
  parent.onPanelLoaded('am-addressing.xul');
}

function onInit(aPageId, aServerId) 
{
  onInitCompositionAndAddressing();
}

function onInitCompositionAndAddressing()
{
  setupDirectoriesList();
  enabling();
  quoteEnabling();
}

function onPreInit(account, accountValues)
{
  gIdentity = account.defaultIdentity;
}

function enabling()
{
  var autocomplete = document.getElementById("ldapAutocomplete");
  var directoriesList =  document.getElementById("directoriesList"); 
  var directoriesListPopup = document.getElementById("directoriesListPopup");
  var editButton = document.getElementById("editButton");

  // this is the hidden text element that assigned a value from the prefs
  var overrideGlobalPref = document.getElementById("identity.overrideGlobalPref");

  switch(autocomplete.value)
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

  if (!gPrefInt) {
    gPrefInt = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
  }

  // If the default per-identity directory preferences are locked 
  // disable the corresponding elements.
  if (gIdentity && gPrefInt.prefIsLocked("mail.identity." + gIdentity.key + ".overrideGlobal_Pref")) {
    document.getElementById("useGlobalPref").setAttribute("disabled", "true");
    document.getElementById("directories").setAttribute("disabled", "true");
  }
  else
  {
    document.getElementById("useGlobalPref").removeAttribute("disabled");
    document.getElementById("directories").removeAttribute("disabled");
  }
  if (gIdentity && gPrefInt.prefIsLocked("mail.identity." + gIdentity.key + ".directoryServer")) {
    document.getElementById("directoriesList").setAttribute("disabled", "true");
    document.getElementById("directoriesListPopup").setAttribute("disabled", "true");
  }

  LoadDirectories(directoriesListPopup);
}

function onSave()
{
  onSaveCompositionAndAddressing();
}

function onSaveCompositionAndAddressing()
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
  switch(autocomplete.value)
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

function quoteEnabling()
{
  var quotebox = document.getElementById("thenBox");
  var placebox = document.getElementById("placeBox");
  var quotecheck = document.getElementById("identity.autoQuote");

  if (quotecheck.checked && !quotecheck.disabled &&
      document.getElementById("identity.attachSignature").checked &&
      (document.getElementById("identity.replyOnTop").value == 1)) {
    placebox.firstChild.removeAttribute("disabled");
    placebox.lastChild.removeAttribute("disabled");
  }
  else {
    placebox.firstChild.setAttribute("disabled", "true");
    placebox.lastChild.setAttribute("disabled", "true");
  }
  if (quotecheck.checked && !quotecheck.disabled) {
    quotebox.firstChild.removeAttribute("disabled");
    quotebox.lastChild.removeAttribute("disabled");
  }
  else {
    quotebox.firstChild.setAttribute("disabled", "true");
    quotebox.lastChild.setAttribute("disabled", "true");
  }
}

