var cmdStopLoading =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    var res = false;
    try {
      var tab = document.getElementById("tabeditor").selectedTab;
      if (tab)
        res = tab.hasAttribute("busy");
    }
    catch(e) {}
    return res;
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    document.getElementById("tabeditor").stopWebNavigation();
  }
};

var cmdTabeditor =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return document.getElementById("tabeditor").mTabpanels.hasChildNodes();
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},
  doCommand: function(aCommand) {}
};

var cmdNew =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},
  doCommand: function(aCommand)
  {
    OpenFile("chrome://composer/content/blanks/transitional.html", true);
  }
};
