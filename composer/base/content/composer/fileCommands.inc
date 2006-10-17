var cmdOpen =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    window.openDialog("chrome://composer/content/dialogs/openLocation.xul","_blank",
                      "chrome,modal,titlebar", "tab");
  }
};

