///////////////////////////////////////////////////////////////////////////////
// Settings!

var SOFTWARE_NAME  = "ActiveX Plugin";
var VERSION        = "1.0.0.3";

var PLUGIN_FILE    = "npmozax.dll";
var PLUGIN_SIZE    = 300;
var COMPONENT_FILE = "nsIMozAxPlugin.xpt";
var COMPONENT_SIZE = 2;
var CLASS_FILE     = "MozAxPlugin.class";
var CLASS_SIZE     = 2;

var MSVCRT_FILE    = "msvcrt.dll";
var MSVCRT_SIZE    = 400;
var MSSTL_FILE     = "msvcp60.dll";
var MSSTL_SIZE     = 300;

var PLID           = "@mozilla.org/ActivexPlugin"; // ,version=1.0.0.3";

///////////////////////////////////////////////////////////////////////////////


// Invoke initInstall to start the installation
err = initInstall(SOFTWARE_NAME, PLID, VERSION);
if (err != 0)
{
    logComment("Install failed at initInstall level with " + err);
    cancelInstall(err);
}

var pluginFolder = getFolder("Plugins");
var winsysFolder = getFolder("Win System");

if (verifyDiskSpace(pluginFolder, PLUGIN_SIZE + COMPONENT_SIZE))
{
    errBlock1 = addFile(PLID, VERSION, PLUGIN_FILE, pluginFolder, null);
    if (errBlock1 != 0)
    {
        alert("Installation of MozAxPlugin plug-in failed. Error code " + errBlock1);
        logComment("adding file " + PLUGIN_FILE + " failed. Errror code: " + errBlock1);
        cancelInstall(errBlock1);
    }

    errBlock1 = addFile (PLID, VERSION, COMPONENT_FILE, pluginFolder, null);
    if (errBlock1 != 0)
    {
        alert("Installation of MozAxPlugin component failed. Error code " + errBlock1);
        logComment("adding file " + COMPONENT_FILE + " failed. Error code: " + errBlock1);
        cancelInstall(errBlock1);
    }

// TODO is there any point installing the 4.x LiveConnect class in an XPI file?
//  errBlock1 = addFile(CLASS_FILE);
//  if (errBlock1 != 0)
//  {
//      alert("Installation of MozAxPlugin liveconnect class failed. Error code " + errBlock1);
//      logComment("adding file " + CLASS_FILE + " failed. Errror code: " + errBlock1);
//      cancelInstall(errBlock1);
//  }
}
else
{
    logComment("Cancelling current browser install due to lack of space...");
    cancellInstall();
}

// TODO install in secondary location???

// Install C runtime files
if (verifyDiskSpace(pluginFolder, MSVCRT_SIZE + MSSTL_SIZE))
{
    // install msvcrt.dll *only* if it does not exist
    // we don't care if addFile() fails (if the file does not exist in the archive)
    // bacause it will still install

    fileTemp   = winsysFolder + MSVCRT_FILE;
    fileMsvcrt = getFolder("file:///", fileTemp);
    if (File.exists(fileMsvcrt) == false)
    {
        logComment("File not found: " + fileMsvcrt);
        addFile("/Microsoft/Shared",
            VERSION,
            MSVCRT_FILE,        // dir name in jar to extract 
            winsysFolder,       // Where to put this file (Returned from getFolder) 
            "",                 // subdir name to create relative to fProgram
            WIN_SHARED_FILE);
        logComment("addFile() of msvcrt.dll returned: " + err);
    }
    else
    {
        logComment("File found: " + fileMsvcrt);
    }

    // install msvcp60.dll
    fileTemp  = winsysFolder + MSSTL_FILE;
    fileMsstl = getFolder("file:///", fileTemp);
    if (File.exists(fileMsstl) == false)
    {
        logComment("File not found: " + fileMsvcrt);
        addFile("/Microsoft/Shared",
            VERSION,
            MSSTL_FILE,   // dir name in jar to extract 
            winsysFolder, // Where to put this file (Returned from getFolder) 
            "",           // subdir name to create relative to fProgram
            WIN_SHARED_FILE);
        logComment("addFile() of msvcp60.dll returned: " + err);
    }
    else
    {
        logComment("File found: " + fileMsstl);
    }
}
else
{
    logComment("Cancelling current browser install due to lack of space...");
    cancellInstall();
}

performInstall();



/**
 * Function for preinstallation of plugin (FirstInstall).
 * You should not stop the install process because the function failed,
 * you still have a chance to install the plugin for the already
 * installed gecko browsers.
 *
 * @param dirPath   directory path from getFolder
 * @param spaceRequired    required space in kilobytes
 * 
 **/
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
