function Init()
{
  parent.initPanel('chrome://messenger/content/pref-mailnews.xul');
  var initFunc;
  try {
    initFunc = document.getElementById("mailnewsEnableMapi").getAttribute('initFunc');
  }
  catch (ex) {
    initFunc = null;
  }
  if (initFunc)
    eval(initFunc);
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

function setHomePageToDefaultPage(folderFieldId)
{
  var homePageField = document.getElementById(folderFieldId);
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefs = prefService.getDefaultBranch(null);
  var url = prefs.getComplexValue("mailnews.start_page.url",
                                  Components.interfaces.nsIPrefLocalizedString);
  homePageField.value = url;
}

