function run_test() {
    do_check_true("@mozilla.org/layout-debug/regressiontester;1" in
		  Components.classes);
    do_check_true("@mozilla.org/layout-debug/layout-debuggingtools;1" in
		  Components.classes);
    do_check_true("@mozilla.org/commandlinehandler/general-startup;1?type=layoutdebug" in
		  Components.classes);    
}