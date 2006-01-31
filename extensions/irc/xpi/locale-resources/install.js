/* NOTE FOR LOCALIZERS:
 * Copy this file to a new ab-CD subdirectory, where ab-CD is the localization you are doing
 *
 * You should edit the following three strings manually if you manipulate an existing jar
 * If you are building from CVS, please leave them be (but do modify the srDest variable
 * if necessary.
 */
var version = "@REVISION@";
var locale = "@LOCALE@";
var jarFile = "chatzilla-@LOCALE@.jar";
// Set this to the size of the jar file you're installing in kibibytes (1024 bytes per KiB)
var srDest = 110;
// You're done now. Don't modify the rest of the script without a very good reason.

// this function verifies disk space in kilobytes
function verifyDiskSpace(dirPath, spaceRequired)
{
  var spaceAvailable;

  // Get the available disk space on the given path
  spaceAvailable = fileGetDiskSpaceAvailable(dirPath);

  // Convert the available disk space into kilobytes
  spaceAvailable = parseInt(spaceAvailable / 1024);

  // do the verification
  if(spaceAvailable < spaceRequired)
  {
    logComment("Insufficient disk space: " + dirPath);
    logComment("  required : " + spaceRequired + " K");
    logComment("  available: " + spaceAvailable + " K");
    return(false);
  }

  return(true);
}

// this function deletes a file if it exists
function deleteThisFile(dirKey, file)
{
  var fFileToDelete;

  fFileToDelete = getFolder(dirKey, file);
  logComment("File to delete: " + fFileToDelete);
  if(File.isFile(fFileToDelete))
  {
    File.remove(fFileToDelete);
    return(true);
  }
  else
    return(false);
}

// this function deletes a folder if it exists
function deleteThisFolder(dirKey, folder, recursiveDelete)
{
  var fToDelete;

  if(typeof recursiveDelete == "undefined")
    recursiveDelete = true;

  fToDelete = getFolder(dirKey, folder);
  logComment("folder to delete: " + fToDelete);
  if(File.isDirectory(fToDelete))
  {
    File.dirRemove(fToDelete, recursiveDelete);
    return(true);
  }
  else
    return(false);
}

// OS type detection
// which platform?
function getPlatform()
{
  var platformStr;
  var platformNode;

  if('platform' in Install)
  {
    platformStr = new String(Install.platform);

    if (!platformStr.search(/^Macintosh/))
      platformNode = 'mac';
    else if (!platformStr.search(/^Win/))
      platformNode = 'win';
    else if (!platformStr.search(/^OS\/2/))
      platformNode = 'win';
    else
      platformNode = 'unix';
  }
  else
  {
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

var err = initInstall("Chatzilla " + locale + " " + version, "Chatzilla " + locale, version); 
logComment("initInstall: " + err);

if (verifyDiskSpace(getFolder("Program"), srDest))
{
    addFile("Chatzilla " + locale + " Locale",
            "chrome/" + jarFile,        // jar source folder 
            getFolder("Chrome"),        // target folder
            "");                        // target subdir 

    registerChrome(LOCALE | DELAYED_CHROME, getFolder("Chrome", jarFile), "locale/" + locale + "/chatzilla/");

    if (err==SUCCESS)
        performInstall(); 
    else
        cancelInstall(err);
}
else
    cancelInstall(INSUFFICIENT_DISK_SPACE);
