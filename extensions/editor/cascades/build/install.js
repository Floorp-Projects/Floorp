

var gVersion = "0.2.1";

var err = initInstall("CaScadeS", "cascades", gVersion);
logComment("initInstall: " + err);

var fProgram = getFolder("Program");
logComment("fProgram: " + fProgram);

err = addDirectory("", gVersion, "bin", fProgram, "", true);
logComment("addDirectory: " + err);

registerChrome(CONTENT | DELAYED_CHROME, getFolder("Chrome","cascades.jar"), "content/cascades/");
registerChrome(LOCALE | DELAYED_CHROME, getFolder("Chrome","cascades.jar"), "locale/en-US/cascades/");

if (getLastError() == SUCCESS) {
  err = performInstall(); 
  logComment("performInstall: " + err);
} else {
  cancelInstall(err);
}
