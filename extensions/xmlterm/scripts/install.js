var err = initInstall("XMLterm v0.5",  // name for install UI
                      "XMLterm",       // registered name
                      "0.5");          // package version

logComment("initInstall: " + err);

var fProgram = getFolder("Program");
var fComponents = getFolder("Components");
var fXMLterm = getFolder("Chrome","packages/xmlterm");

// addDirectory: blank, archive_dir, install_dir, subdir
err = addDirectory("", "bin",                     fProgram,    "");
if (err != SUCCESS)
   cancelInstall(err);

err = addDirectory("", "components",              fComponents, "");
if (err != SUCCESS)
   cancelInstall(err);

err = addDirectory("", "chrome/packages/xmlterm", fXMLterm,    "");
if (err != SUCCESS)
   cancelInstall(err);

// Register chrome
err = registerChrome(PACKAGE|DELAYED_CHROME, fXMLterm );

if (getLastError() == SUCCESS)
    performInstall();
else {
   alert("Error detected: "+getLastError());
   cancelInstall();
}
