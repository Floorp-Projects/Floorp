initInstall("Mozilla Calendar", "Mozilla/Calendar", "0.7");

calendarDir = getFolder("Chrome","calendar");

setPackageFolder(calendarDir);

addDirectory( "resources" );

addDirectory("", "components", getFolder( "Components" ), "" );

var err = getLastError();
  
if ( err == SUCCESS ) { 
  
   registerChrome(CONTENT, calendarDir, "content");
   registerChrome(SKIN,    calendarDir, "skin/modern");
   registerChrome(LOCALE,  calendarDir, "locale/en-US");

   err = performInstall();
  
   if ( err == SUCCESS ) {
      refreshPlugins();
      alert("The Mozilla Calendar has been succesfully installed. \n"
      +"Please restart your browser to continue.");
   }
   else { 
    alert("performInstall() failed. \n"
    +"_____________________________\nError code:" + err);
    cancelInstall(err);
   }
} 
else { 
  alert("Failed to create directory. \n"
    +"You probably don't have appropriate permissions \n"
    +"(write access to mozilla/chrome directory). \n"
    +"_____________________________\nError code:" + err);
    cancelInstall(err);
}


