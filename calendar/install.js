/* ***************
Desc: Installation script
****************** */
const displayName = "Mozilla Calendar";
const name        = "MozillaCalendar";
const version     = "0.8";

initInstall(displayName, name, version);

calendarDir = getFolder("Chrome","calendar");

setPackageFolder(calendarDir);

addDirectory( "resources" );

addDirectory("", "bin/components", getFolder( "Components" ), "" );

addDirectory( "", "", "icons", getFolder( "Chrome", "icons" ), "", true );

addFile( "Calendar Chrome",
         "bin/chrome/calendar.jar", // jar source folder 
         getFolder("Chrome"),        // target folder
         "");

var err = getLastError();

if ( err == SUCCESS ) { 
  
   var calendarLocale  = getFolder(calendarDir, "locale");

   registerChrome(PACKAGE | DELAYED_CHROME, getFolder("Chrome","calendar.jar"), "content/calendar/");
   registerChrome(SKIN | DELAYED_CHROME, getFolder("Chrome","calendar.jar"), "skin/classic/calendar/");
   registerChrome(SKIN | DELAYED_CHROME, getFolder("Chrome","calendar.jar"), "skin/modern/calendar/");
   registerChrome(LOCALE | DELAYED_CHROME, getFolder("Chrome","calendar.jar"), "locale/en-US/calendar/");

   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "cs-CZ/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "cy-GB/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "de-AT/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "es-ES/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "fr-FR/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "hu-HU/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "ja-JP/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "lt-LT/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "nl-NL/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "pl-PL/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "pt-BR/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "sk-SK/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "sl-SI/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "sv-SE/");
   registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "wen-DE/");

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

