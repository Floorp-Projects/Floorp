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
var srDest = 66;// size in kb

var err = startInstall("XMLExtras 0.7", "XMLExtras", "0.7.0.2000122814"); 
logComment("startInstall: " + err);

var fProgram = getFolder("Program");
logComment("fProgram: " + fProgram);

if (verifyDiskSpace(fProgram, srDest))
{
    err = addDirectory("XMLExtras",
                       "0.7.0.2000122814",
                       "bin",
                       fProgram,
                       "",
                       true );

    logComment("addDirectory() returned: " + err);

    if (err==SUCCESS)
    {
	    err = performInstall(); 
          logComment("performInstall() returned: " + err);
    }
    else
    {
	    cancelInstall(err);
	    logComment("cancelInstall() due to error: " + err);
    }
}
else
    cancelInstall(INSUFFICIENT_DISK_SPACE);
