initInstall("Mozilla Calendar", "Mozilla/Calendar", "0.9");

calendarDir = getFolder("Chrome","calendar");

setPackageFolder(calendarDir);

addDirectory( "resources" );

addDirectory("", "components", getFolder( "Components" ), "" );

var err = getLastError();
  
if ( err == SUCCESS ) { 
  
   var calendarContent = getFolder(calendarDir, "content");
   var calendarSkin    = getFolder(calendarDir, "skin");
   var calendarLocale  = getFolder(calendarDir, "locale");

   var returnval = registerChrome(CONTENT | DELAYED_CHROME, calendarContent );
   var returnval = registerChrome(SKIN | DELAYED_CHROME, calendarSkin, "modern/");
   var returnval = registerChrome(SKIN | DELAYED_CHROME, calendarSkin, "classic/");
   var returnval = registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "en-US/");
  
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


