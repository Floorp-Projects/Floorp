// Force SafeBrowsing to be initialized for the tests
Services.prefs.setCharPref("urlclassifier.malware_table", "test-malware-simple");
Services.prefs.setCharPref("urlclassifier.phish_table", "test-phish-simple");
SafeBrowsing.init();

