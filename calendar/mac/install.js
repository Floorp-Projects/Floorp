    var err = initInstall("Mozilla Calendar", "Mozilla/Calendar", "0.1");
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
                       "calendar",                      // target subdir
                       true );                          // force flag

    logComment("addDirectory() returned: " + err);

    calendarDir = getFolder("Chrome","calendar");

    var calendarContent = getFolder(calendarDir, "content");
    var calendarSkin    = getFolder(calendarDir, "skin");
    var calendarLocale  = getFolder(calendarDir, "locale");

    err = registerChrome(CONTENT | DELAYED_CHROME, calendarContent );
    err = registerChrome(SKIN | DELAYED_CHROME, calendarSkin, "modern/");
    err = registerChrome(SKIN | DELAYED_CHROME, calendarSkin, "classic/");
    err = registerChrome(LOCALE | DELAYED_CHROME, calendarLocale, "en-US/");

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
