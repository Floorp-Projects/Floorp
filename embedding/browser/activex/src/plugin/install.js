///////////////////////////////////////////////////////////////////////////////
// Settings!

var SOFTWARE_NAME  = "ActiveX Plugin";
var VERSION        = "1.0.0.3";
var PLID_BASE      = "@mozilla.org/ActiveXPlugin";
var PLID           = PLID_BASE + ",version=" + VERSION;

var FLDR_COMPONENTS = getFolder("Components");
var FLDR_PLUGINS   = getFolder("Plugins");
var FLDR_PREFS     = getFolder("Program","defaults/pref");
var FLDR_WINSYS    = getFolder("Win System");

var PLUGIN         = new FileToInstall("npmozax.dll", 300, FLDR_PLUGINS);
var XPT            = new FileToInstall("nsIMozAxPlugin.xpt", 2, FLDR_COMPONENTS);
var SECURITYPOLICY = new FileToInstall("nsAxSecurityPolicy.js", 9, FLDR_COMPONENTS);
var PREFS          = new FileToInstall("activex.js", 5, FLDR_PREFS);

var MSVCRT         = new FileToInstall("msvcrt.dll", 400, FLDR_WINSYS);
var MSSTL60        = new FileToInstall("msvcp60.dll", 300, FLDR_WINSYS);
var MSSTL70        = new FileToInstall("msvcp70.dll", 300, FLDR_WINSYS);

var filesToAdd     = new Array(PLUGIN, XPT, SECURITYPOLICY, PREFS);
var sysFilesToAdd  = new Array(MSVCRT, MSSTL60, MSSTL70);


///////////////////////////////////////////////////////////////////////////////


// Invoke initInstall to start the installation
err = initInstall(SOFTWARE_NAME, PLID, VERSION);
if (err == BAD_PACKAGE_NAME)
{
    // HACK: Mozilla 1.1 has a busted PLID parser which doesn't like the equals sign
    PLID = PLID_BASE;
    err = initInstall(SOFTWARE_NAME, PLID, VERSION);
}
if (err == SUCCESS)
{
    // Install plugin files
    err = verifyDiskSpace(FLDR_PLUGINS, calcSpaceRequired(filesToAdd));
    if (err == SUCCESS)
    {
        for (i = 0; i < filesToAdd.length; i++)
        {
            err = addFile(PLID, VERSION, filesToAdd[i].name, filesToAdd[i].path, null);
            if (err != SUCCESS)
            {
                alert("Installation of " + filesToAdd[i].name + " failed. Error code " + err);
                logComment("adding file " + filesToAdd[i].name + " failed. Errror code: " + err);
                break;
            }
        }
    }
    else
    {
        logComment("Cancelling current browser install due to lack of space...");
    }

    // Install C runtime files
    if (err == SUCCESS)
    {
        if (verifyDiskSpace(FLDR_WINSYS, calcSpaceRequired(sysFilesToAdd)) == SUCCESS)
        {
            // Install system dlls *only* if they do not exist.
            //
            // NOTE: Ignore problems installing these files, since all kinds
            //       of stuff could cause this to fail and I really don't care
            //       about dealing with email describing failed permissions,
            //       locked files or whatnot.
            for (i = 0; i < sysFilesToAdd.length; i++)
            {
                fileTemp   = sysFilesToAdd[i].path + sysFilesToAdd[i].name;
                fileUrl = getFolder("file:///", fileTemp);
                if (File.exists(fileUrl) == false)
                {
                    logComment("File not found: " + fileTemp);
                    addFile("/Microsoft/Shared",
                        VERSION,
                        sysFilesToAdd[i].name, // dir name in jar to extract 
                        sysFilesToAdd[i].path, // Where to put this file (Returned from getFolder) 
                        "",                    // subdir name to create relative to fProgram
                        WIN_SHARED_FILE);
                    logComment("addFile() of " + sysFilesToAdd[i].name + " returned: " + err);
                }
                else
                {
                    logComment("File found: " + sysFilesToAdd[i].name );
                }
            }
        }
        else
        {
            logComment("Cancelling current browser install due to lack of space...");
        }
    }
}
else
{
    logComment("Install failed at initInstall level with " + err);
}

if (err == SUCCESS)
{
    err = performInstall();
    if (err == SUCCESS)
    {
        alert("Installation performed successfully, you must restart the browser for the changes to take effect");
    }
}
else
    cancelInstall();

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
        return INSUFFICIENT_DISK_SPACE;
    }

    return SUCCESS;
}

function calcSpaceRequired(fileArray)
{
    var spaceRqd = 0;
    for (i = 0; i < fileArray.length; i++)
    {
        spaceRqd += fileArray[i].size;
    }
    return spaceRqd;
}

function FileToInstall(fileName, fileSize, dirPath)
{
    this.name = fileName;
    this.size = fileSize;
    this.path = dirPath;
}