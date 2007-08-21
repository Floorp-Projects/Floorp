const Ci = Components.interfaces;
const Cc = Components.classes;

function test() {
  ok(Application, "Check global access to Application");
  
  // I'd test these against a specific value, but that is bound to flucuate
  ok(Application.id, "Check to see if an ID exists for the Application");
  ok(Application.name, "Check to see if a name exists for the Application");
  ok(Application.version, "Check to see if a version exists for the Application");
  
  var wMediator = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
  var console = wMediator.getMostRecentWindow("global:console");
  if (!console) {
    Application.console.open();
    setTimeout(checkConsole, 500);
  }
}

function checkConsole() {
  var wMediator = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
  var console = wMediator.getMostRecentWindow("global:console");
  ok(console, "Check to see if the console window opened");
  if (console)
    console.close();
}
