/* ***************
Desc: Installation script
****************** */
const displayName = "Mozilla Calendar";
const name        = "MozillaCalendar";
const version     = "0.8";
const addLocales   = new Array("cs-CZ", "cy-GB", "de-AT", "es-ES", "fr-FR", "hu-HU", "ja-JP", "lt-LT", "nl-NL", "pl-PL", "pt-BR", "sk-SK", "sl-SI", "sv-SE", "wen-DE");

var err = initInstall(displayName, name, version);

logComment("initInstall returned: " + err);

calendarDir = getFolder("Chrome","calendar");

logComment("calendarDir is: " + calendarDir);

setPackageFolder(calendarDir);

err = addDirectory( "resources" );

logComment("addDirectory() for resources returned: " + err);

err = addDirectory("", "bin/components", getFolder( "Components" ), "" );

logComment("addDirectory() for components returned: " + err);

err = addDirectory( "", "", "icons", getFolder( "Chrome", "icons" ), "", true );

logComment("addDirectory() for icons returned: " + err);

err = addFile( "Calendar Chrome",
         "bin/chrome/calendar.jar", // jar source folder 
         getFolder("Chrome"),        // target folder
         "");

logComment("addFile() for calendar.jar returned: " + err);

var err = getLastError();

if ( err == SUCCESS ) { 
  
   var calendarLocale  = getFolder(calendarDir, "locale");

   registerChrome(PACKAGE | DELAYED_CHROME, getFolder("Chrome","calendar.jar"), "content/calendar/");
   registerChrome(SKIN | DELAYED_CHROME, getFolder("Chrome","calendar.jar"), "skin/classic/calendar/");
   registerChrome(SKIN | DELAYED_CHROME, getFolder("Chrome","calendar.jar"), "skin/modern/calendar/");
   registerChrome(LOCALE | DELAYED_CHROME, getFolder("Chrome","calendar.jar"), "locale/en-US/calendar/");

   for (var i = 0; i < addLocales.length; i++) {

      // Check Mozilla 1.x, Mozilla Firefox (Browser)
      chkJarFileName = addLocales[i] + ".jar";
      tmp_f = getFolder("Chrome", chkJarFileName);
      if ( File.exists(tmp_f) ) {
        registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, addLocales[i] + "/");
      }

      // Check Mozilla Thunderbird (Mail/News)
      chkJarFileName = addLocales[i] + "-mail.jar";
      tmp_f = getFolder("Chrome", chkJarFileName);
      if ( File.exists(tmp_f) ) {
        registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, addLocales[i] + "/");
      }
   }

   err = performInstall();
  
   if ( err == SUCCESS || err == 999 ) {
       alert("The Mozilla Calendar has been successfully installed. \n"
       +"Please restart your application to continue.");
   } else { 
       alert("performInstall() failed. \n"
       +"_____________________________\nError code:" + err);
       cancelInstall(err);
   }
} 
else { 
   alert("Failed to create directory. \n"
    +"You probably don't have appropriate permissions \n"
    +"(write access to <mozilla>/chrome directory). \n"
    +"If you installed Mozilla as root then you need to install calendar as root as well.\n"
    +"Or, you can change ownership of your Mozilla directory to yourself and install calendar."
    +"_____________________________\nError code:" + err);
    cancelInstall(err);
}

