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
   symlink(fComponents + "libjavadom.so", 
           getFolder(fJavadev, "lib/libjavadom.so"));
   symlink(fComponents + "libbcorb.so", 
           getFolder(fJavadev, "lib/libbcorb.so"));
   symlink(fComponents + "libbcjavastubs.so", 
           getFolder(fJavadev, "lib/libbcjavastubs.so"));
   symlink(fComponents + "libjavaloader.so", 
           getFolder(fJavadev, "lib/libjavaloader.so"));
   symlink(fComponents + "libjavaSample.so", 
           getFolder(fJavadev, "lib/libjavaSample.so"));
   symlink(fComponents + "libbcTest.so", 
           getFolder(fJavadev, "lib/libbcTest.so"));
   symlink(fComponents + "libbcxpcomstubs.so", 
           getFolder(fJavadev, "lib/libbcxpcomstubs.so"));
   symlink(fProgram + "regxpcom", 
           getFolder(fJavadev, "lib/regxpcom"));

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
