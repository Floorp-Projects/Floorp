// Install script for palmsync

var err;

err = initInstall("Palmsync v0.4.0", // name for install UI
                  "/palmsync",         // registered name
                  "0.4.0.0");        // package version

const APP_VERSION = "0.0.4";
const APP_PACKAGE = "/XXX.mozdev.org/palmsync";

logComment("initInstall: " + err);

var srDest = 300;       // Disk space required for installation (KB)

var fProgram    = getFolder("Program");
logComment("fProgram: " + fProgram);

if (!verifyDiskSpace(fProgram, srDest)) {
  cancelInstall(INSUFFICIENT_DISK_SPACE);

} else {

  var fInstallDir = getFolder(".");
  var fComponentsDir = getFolder("components");
  err = addFile(APP_PACKAGE, APP_VERSION, "mozABConduit.dll", fProgram, null);
  err = addFile(APP_PACKAGE, APP_VERSION, "CondMgr.dll", fProgram, null);
  err = addFile(APP_PACKAGE, APP_VERSION, "HSAPI.dll", fProgram, null);
  err = addFile(APP_PACKAGE, APP_VERSION, "PalmSyncProxy.dll", fProgram, null);
  err = addFile(APP_PACKAGE, APP_VERSION, "PalmSyncInstall.exe", fProgram, null);
  err = addFile(APP_PACKAGE, APP_VERSION, "palmsync.dll", fComponentsDir, null);
  err = addFile(APP_PACKAGE, APP_VERSION, "palmSync.xpt", fComponentsDir, null);

  err = getLastError();

  if (err == ACCESS_DENIED) {
    alert("Unable to write to installation directory "+fInstallDir+".\n You will need to restart the browser with administrator/root privileges to install this software. After installing as root (or administrator), you will need to restart the browser one more time to register the installed software.\n After the second restart, you can go back to running the browser without privileges!");

    cancelInstall(ACCESS_DENIED);

  } else if (err != SUCCESS) {
    cancelInstall(err);

  } else {
    var args = "/p";
    args += fProgram;
	err = execute("PalmSyncInstall.exe", args, true);
    performInstall();
  }
}

// this function verifies disk space in kilobytes
function verifyDiskSpace(dirPath, spaceRequired) {
  var spaceAvailable;

  // Get the available disk space on the given path
  spaceAvailable = fileGetDiskSpaceAvailable(dirPath);

  // Convert the available disk space into kilobytes
  spaceAvailable = parseInt(spaceAvailable / 1024);

  // do the verification
  if(spaceAvailable < spaceRequired) {
    logComment("Insufficient disk space: " + dirPath);
    logComment("  required : " + spaceRequired + " K");
    logComment("  available: " + spaceAvailable + " K");
    return false;
  }

  return true;
}

// OS type detection
// which platform?
function getPlatform() {
  var platformStr;
  var platformNode;

  if('platform' in Install) {
    platformStr = new String(Install.platform);

    if (!platformStr.search(/^Macintosh/))
      platformNode = 'mac';
    else if (!platformStr.search(/^Win/))
      platformNode = 'win';
    else
      platformNode = 'unix';
  }
  else {
    var fOSMac  = getFolder("Mac System");
    var fOSWin  = getFolder("Win System");

    logComment("fOSMac: "  + fOSMac);
    logComment("fOSWin: "  + fOSWin);

    if(fOSMac != null)
      platformNode = 'mac';
    else if(fOSWin != null)
      platformNode = 'win';
    else
      platformNode = 'unix';
  }

  return platformNode;
}
