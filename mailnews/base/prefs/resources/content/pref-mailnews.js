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

function StartPageCheck()
{
  var checked = document.getElementById("mailnewsStartPageEnabled").checked;
  var startPageLocked = parent.hPrefWindow.getPrefIsLocked("mailnews.start_page.url");
  document.getElementById("mailnewsStartPageUrl").disabled = !checked || startPageLocked;
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

