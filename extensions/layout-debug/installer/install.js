// Name of the files to be installed
var PLUGIN_FILE    = "npdebug.dll";
var COMPONENT_FILE = "npdebug.xpt";
var SERVICE_FILE = "abozo.txt";

//!!! VERY IMPORTANT TO UPDATE
var LD_MAJOR_VER = "0.5";                           // Major minor version
var LD_VERSION   = LD_MAJOR_VERSION = ".0.1";       // Append the buildnr to the major version

//!!! VERY IMPORTANT TO UPDATE
// The plug-in sise in kilobyte, both the dll and the xpi file. Used to check if there is enough space left on the clients harddisk
var PLUGIN_SIZE    = 2000; // (DLL file) Reserve a little extra so it is not required to update too often
var COMPONENT_SIZE = 10;   // (XPI file) Reserve a little extra so it is not required to update too often
var DEBUGDLL    = 2000; // (DLL file) Reserve a little extra so it is not required to update too often


////////////////////////////////////////
//         MAIN                       //
//         main                       //
//        M A I N                     //
////////////////////////////////////////
startLayoutDebugInstallation();


/**
 * This function installs the Mozilla debug layout plugins.
 * Must be called from installC3DPlugin
 *
 * @return true if the installation were successfull, false otherwise.
 */
function startLayoutDebugInstallation()
{
	var err = initInstall("LayoutDebug Mozilla  "+LD_MAJOR_VER, "Mozilla Debug Plugin", LD_VERSION);
	logComment("Layout Debug initInstall: " + err);
	if (err != SUCCESS) {
    /// InitInstall failed. Couldn't even start to initialize the installation
		alert("Initialization failed. Errorcode: "+err);
		logComment("Layout Debug initInstall failed "+err);
		cancelInstall(err);
		return false;
	}


  // install the debug service
  var componentsFolder = getFolder("components");
	if ( verifyDiskSpace(componentsFolder, (DEBUGDLL)) == false ) {
		// Insufficent disk space to install the plug-in
		var errMsg = "Not enough space on harddisk to install the service";
		alert(errMsg);
		logComment("Layout Debug "+errMsg);
		logComment("Layout Debug   -- components folder diskspace:    "+getAvailableDiskspace(componentsFolder));
		cancelInstall(INSUFFICIENT_DISK_SPACE);
		return false;
	}

	logComment("Enough space on harddisk to install the debug service");

	if (!fileExists(componentsFolder)) {
	  var errMsg = "Missing the components directory("+componentsFolder+"). Errorcode "+err;
	  alert(errMsg);
	  LogComment("Layout Debug "+errMsg);
	  cancelInstall(err);
	  return false;
	}

	err = addFile(null, LD_VERSION, SERVICE_FILE, componentsFolder, null);
	if (err != SUCCESS) {
		alert("Installation of Layout-Debug Service failed. Error code "+err);
		logComment("adding file "+SERVICE_FILE+" failed. Errror code: " + err);
		cancelInstall(err);
		return false;
	}



  // install the plugins and xlt files
	var pluginsFolder    = getFolder("Plugins");

	logComment("Layout Debug pluginsFolder:    " + pluginsFolder);

	if ( verifyDiskSpace(pluginsFolder, (PLUGIN_SIZE+COMPONENT_SIZE)) == false ) {
		// Insufficent disk space to install the plug-in
		var errMsg = "Not enough space on harddisk to install the plug-in";
		alert(errMsg);
		logComment("Layout Debug "+errMsg);
		logComment("Layout Debug   -- Plugin folder diskspace:    "+getAvailableDiskspace(pluginsFolder));
		cancelInstall(INSUFFICIENT_DISK_SPACE);
		return false;
	}

	logComment("Enough space on harddisk to install the plugin");

	if (!fileExists(pluginsFolder)) {
		err = dirCreate(pluginsFolder);
		if (err != SUCCESS) {
			var errMsg = "Could not create the missing plug-in directory("+pluginsFolder+"). Errorcode "+err;
			alert(errMsg);
			LogComment("Layout Debug "+errMsg);
			cancelInstall(err);

			return false;
		}
	}


	err = addFile(null, LD_VERSION, PLUGIN_FILE, pluginsFolder, null);
	if (err != SUCCESS) {
		alert("Installation of Layout-Debug Plugin failed. Error code "+err);
		logComment("Adding file "+PLUGIN_FILE+" failed. Errror code: " + err);
		cancelInstall(err);

		return false;
	}

	err = addFile(null,LD_VERSION, COMPONENT_FILE, pluginsFolder, null);
	if (err != SUCCESS) {
		alert("Installation of Layout-Debug xpt file failed. Error code "+err);
		logComment("Layout Debug adding file "+COMPONENT_FILE+" failed. Error code: " + err);
		cancelInstall(err);

		return false;
	}

	logComment("Layout Debug performing installation");
	err = performInstall();
	logComment("Layout Debug plugin performInstall() returned: " + err);
	if (err == REBOOT_NEEDED) {
		var errMsg = "You need to reboot your computer in order for the installation to complete";
		alert(errMsg);
		logComment("Layout Debug "+errMsg);
	}

	if ((err != REBOOT_NEEDED) && (err != SUCCESS)) {
		var errMsg = "Installation of Layout-Debug Plugin failed. (performInstall) ErrorCode: "+err;
		alert(errMsg);
		logComment("Layout Debug "+errMsg);
		cancelInstall(err);

		return false;
	}

	var succMsg = "Installation of Layout-Debug Plugin succeeded";
	logComment("Layout Debug "+succMsg);

	logComment("Layout Debug refreshing plugin info");
	refreshPlugins();

	return true;
}


/**
  * this function verifies disk space in kilobytes
  *
  * @param dirPath        The folder where to check if there is enough space
  * @param spaceRequired  The minimum pace that must be avaible for this test to be true
  *
  * @return true if there is enough diskspace return
  * @return false if there isn't enough disk space
  */
function verifyDiskSpace(dirPath, spaceRequired)
{
	// Get the available disk space on the given path
    var spaceAvailable = fileGetDiskSpaceAvailable(dirPath);

    // Convert the available disk space into kilobytes
	spaceAvailable = parseInt(spaceAvailable / 1024);

    // do the verification
	if(spaceAvailable < spaceRequired) {
		logComment("Layout Debug Insufficient disk space: " + dirPath);
		logComment("Layout Debug   required : " + spaceRequired + " K");
		logComment("Layout Debug   available: " + spaceAvailable + " K");

		return (false);
	}

	return (true);
}

/**
 * This function returns how many kilobytes are free in the specified folder
 *
 * @param dirPath  The folder where to check if there is enough diskspace
 *
 * @return The amount of free diskspaces, expressed in kilobytes
 */
function getAvailableDiskspace(dirPath)
{
	var spaceAvailable = fileGetDiskSpaceAvailable(dirPath);
	return parseInt(spaceAvailable / 1024);
}

