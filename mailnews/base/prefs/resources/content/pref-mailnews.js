function Startup()
{
  var startupFunc;
  try {
    startupFunc = document.getElementById("mailnewsEnableMapi").getAttribute('startupFunc');
  }
  catch (ex) {
    startupFunc = null;
  }
  if (startupFunc)
    eval(startupFunc);

  StartPageCheck();
}

function setColorWell(menu) 
{
  // Find the colorWell and colorPicker in the hierarchy.
  var colorWell = menu.firstChild.nextSibling;
  var colorPicker = menu.firstChild.nextSibling.nextSibling.firstChild;

  // Extract color from colorPicker and assign to colorWell.
  var color = colorPicker.getAttribute('color');
  colorWell.style.backgroundColor = color;
}

function StartPageCheck()
{
  var checked = document.getElementById("mailnewsStartPageEnabled").checked;
  document.getElementById("mailnewsStartPageUrl").disabled = !checked;
}

function setHomePageToDefaultPage(folderFieldId)
{
  var homePageField = document.getElementById(folderFieldId);
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefs = prefService.getDefaultBranch(null);
  var url = prefs.getComplexValue("mailnews.start_page.url",
                                  Components.interfaces.nsIPrefLocalizedString).data;
  homePageField.value = url;
}

