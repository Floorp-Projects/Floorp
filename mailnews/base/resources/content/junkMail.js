function onJunkMailLoad()
{
  if (window.arguments && window.arguments[0])
    selectAccountFromFolder(window.arguments[0].folder);
}

function selectAccountFromFolder(folder)
{
  if (folder) {
  }
  else {
  }
}

function junkLog()
{
  var args = {foo: null};
  window.openDialog("chrome://messenger/content/junkLog.xul", "junkLog", "chrome,modal,titlebar,resizable,centerscreen", args);
}

function onAccept()
{
}

function doHelpButton()
{
}