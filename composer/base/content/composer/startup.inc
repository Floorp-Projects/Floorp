var gContentWindow = null;

function Startup()
{
  var url = null;
  // do we have a URL to open ?
  if (window.arguments.length == 2)
    url = window.arguments[1];

  GetUIElements();

  // let's finish with the url
  if (url)
    OpenFile(url, true);

  DeclareNotifiers();

  ComposerCommands.setupMainCommands();
  window.updateCommands("style");

  gDialog.sidebaritems.init();
  gDialog.sidebar1.init(gDialog.sidebaritems, gDialog.sidebar2, gDialog.splitter1);
  gDialog.sidebar2.init(gDialog.sidebaritems, gDialog.sidebar1, gDialog.splitter2);

  InstallCustomizationDoneCallbacks();

  gDialog.structurebar.init();
}
