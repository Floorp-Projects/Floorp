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
 
 
 // main
 var srDest;
 var err;
 var fProgram;
 
 srDest = 1000;
logComment("Starting Install Process");
 err    = initInstall("Webclient", "Webclient", "1.0"); 
 logComment("initInstall: " + err);
 
 fProgram = getFolder("Program");
 logComment("fProgram: " + fProgram);
 
 if(verifyDiskSpace(fProgram, srDest))
 {
   setPackageFolder(fProgram);
   err = addDirectory("",
     "1.0",
     "javadev", // dir name in jar to extract 
     fProgram, // Where to put this file 
               // (Returned from GetFolder) 
     "javadev", // subdir name to create relative to fProgram
     true); // Force Flag 
   logComment("addDirectory() returned: " + err);

   var fComponents     = getFolder("Components");
   var fJavadev        = getFolder("Program","javadev");
   src = getFolder(fJavadev, "lib/javadom.dll");
   err = copy(src, fComponents);
   src = getFolder(fJavadev, "lib/bcorb.dll");
   err = copy(src, fComponents);
   src = getFolder(fJavadev, "lib/bcjavastubs.dll");
   err = copy(src, fComponents);
   src = getFolder(fJavadev, "lib/javaloader.dll");
   err = copy(src, fComponents);
   src = getFolder(fJavadev, "lib/javaSample.dll");
   err = copy(src, fComponents);
   src = getFolder(fJavadev, "lib/bcTest.dll");
   err = copy(src, fComponents);
   src = getFolder(fJavadev, "lib/bcxpcomstubs.dll");
   err = copy(src, fComponents);
   src = getFolder(fJavadev, "regxpcom.exe");
   err = copy(src, fProgram);

   // check return value
   if(err == SUCCESS)
   {
     err = performInstall(); 
     logComment("performInstall() returned: " + err);
   }
   else
     cancelInstall(err);
 }
 else
   cancelInstall(INSUFFICIENT_DISK_SPACE);
 
 // end main
