// logComment writes to install.log

// Installation guide for Webclient.xpi
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
 
// this function creates a symlink
function symlink(lnk, tgt)
{
    var err = execute("symlink.sh", tgt + " " + lnk);
    logComment("execute symlink.sh "+tgt+" "+lnk+" returned: "+err);
}

// this function makes the argument executable
function chmodx(tgt)
{
    var err = execute("chmodx.sh", tgt);
    logComment("execute chmodx.sh "+tgt+" returned: "+err);
}


 
 // main
 var srDest;
 var err;
 var fProgram;
 
 srDest = 1000;
logComment("Starting Install Process");
 err    = initInstall("Webclient", "Webclient", "1.3"); 
 logComment("initInstall: " + err);
 
 fProgram = getFolder("Program");
 logComment("fProgram: " + fProgram);
 
 if(verifyDiskSpace(fProgram, srDest))
 {
   setPackageFolder(fProgram);
   err = addDirectory("",
     "1.3",
     "javadev", // dir name in jar to extract 
     fProgram, // Where to put this file 
               // (Returned from GetFolder) 
     "javadev", // subdir name to create relative to fProgram
     true); // Force Flag 
   logComment("addDirectory() returned: " + err);

   var fComponents     = getFolder("Components");
   var fJavadev        = getFolder("Program","javadev");

   // check return value
   if(err == SUCCESS)
   {
     err = performInstall(); 
     logComment("performInstall() returned: " + err);
     err = chmodx(getFolder(fJavadev, "example/runem.bat"));
     logComment("chmodx() returned: " + err);
     err = symlink(fComponents + "libjavadom.so", 
                   getFolder(fJavadev, "lib/libjavadom.so"));
     logComment("symlink() returned: " + err);
   }
   else
     cancelInstall(err);
 }
 else
   cancelInstall(INSUFFICIENT_DISK_SPACE);

 
 // end main
