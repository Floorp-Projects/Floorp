initInstall("Mozilla Calendar", "Mozilla/Calendar", "0.7");

calendarDir = getFolder("Chrome","calendar");

setPackageFolder(calendarDir);

addDirectory( "resources" );

addDirectory("", "components", getFolder( "Components" ), "" );

addDirectory( "", "icons", getFolder( "Chrome", "icons" ), "", true );

err = addFile( "", "calendar_pref.js", getFolder( getFolder( "Program", "defaults" ), "pref" ), "" );
alert( err );
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
      alert("The Mozilla Calendar has been succesfully installed. \n"
      +"Please restart your browser to continue.");
   }
   else if( err == "999" )
   {
      alert("The Mozilla Calendar has been installed. \n Please restart your browser to continue.");
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
    +"If you installed Mozilla as root then you need to install calendar as root as well.\n"
    +"Or, you can change ownership of your Mozilla directory to yourself and install calendar."
    +"_____________________________\nError code:" + err);
    cancelInstall(err);
}


