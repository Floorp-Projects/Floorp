const X_APP             = "Buster";
const X_VER             = "2.0"
const X_JAR_FILE        = "xslt-qa.jar";

var err = initInstall("Install " + X_APP, X_APP, X_VER);
logComment("initInstall: " + err);
logComment( "Installation started ..." );
addFile("We're on our way ...", X_JAR_FILE, getFolder("chrome"), "");
registerChrome(CONTENT|DELAYED_CHROME, getFolder("chrome", X_JAR_FILE), "content/xslt-qa/");
err = getLastError();
if (err == SUCCESS)  {
  performInstall();
  alert("Please restart Mozilla");
}
else  {
  cancelInstall();
}
