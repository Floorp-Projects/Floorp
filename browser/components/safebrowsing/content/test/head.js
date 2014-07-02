// Force SafeBrowsing to be initialized for the tests
Services.prefs.setCharPref("urlclassifier.malwareTable", "test-malware-simple");
Services.prefs.setCharPref("urlclassifier.phishTable", "test-phish-simple");
SafeBrowsing.init();

