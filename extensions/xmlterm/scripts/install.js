var err = initInstall("XMLterm v0.5",  // name for install UI
                      "XMLterm",       // registered name
                      "0.5");          // package version

logComment("initInstall: " + err);

var fProgram    = getFolder("Program");
var fComponents = getFolder("Components");
var fChrome     = getFolder("Chrome");

// addDirectory: blank, archive_dir, install_dir, subdir
err = addDirectory("", "bin",        fProgram,    "");
if (err != SUCCESS)
   cancelInstall(err);

err = addDirectory("", "components", fComponents, "");
if (err != SUCCESS)
   cancelInstall(err);

err = addDirectory("", "chrome",     fChrome,     "");
if (err != SUCCESS)
   cancelInstall(err);

// Register chrome
registerChrome(PACKAGE | DELAYED_CHROME, getFolder("Chrome","xmlterm.jar"), "content/xmlterm/");

registerChrome(SKIN | DELAYED_CHROME, getFolder("Chrome","xmlterm.jar"), "skin/modern/xmlterm/");

registerChrome(LOCALE | DELAYED_CHROME, getFolder("Chrome","xmlterm.jar"), "locale/en-US/xmlterm/");

if (getLastError() == SUCCESS)
    performInstall();
else {
   alert("Error detected: "+getLastError());
   cancelInstall();
}
