    var err = initInstall("Mozilla Calendar", "/Mozilla/Calendar", "0.1");
    logComment("initInstall: " + err);

    var fProgram = getFolder("Program");
    logComment("fProgram: " + fProgram);

    //First install components
    var ComponentDirectory = getFolder("Components");
    err = addDirectory("Calendar Components",
                       "",                              //version
                       "viewer/components",             // jar source folder
                       ComponentDirectory,              // target folder 
                       "",                              // target subdir
                       true );                          // force flag

    logComment("addDirectory() returned: " + err);

    //Next install frontend files
    var ChromeDirectory = getFolder("Chrome");
    err = addDirectory("Calendar Frontend",
                       "",                              //version
                       "viewer/chrome",                 // jar source folder
                       ChromeDirectory,                 // target folder 
                       "",                              // target subdir
                       true );                          // force flag

    logComment("addDirectory() returned: " + err);

    calendarDir = getFolder("Chrome","calendar.jar");

    err = registerChrome(PACKAGE | DELAYED_CHROME, calendarDir, "content/calendar/" );
    err = registerChrome(SKIN | DELAYED_CHROME, calendarDir, "skin/modern/calendar/");
    err = registerChrome(SKIN | DELAYED_CHROME, calendarDir, "skin/classic/calendar/");
    err = registerChrome(LOCALE | DELAYED_CHROME, calendarDir, "locale/en-US/calendar/");

    logComment("registerChrome() returned: " + err);

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
