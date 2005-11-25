#!perl
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

use Cwd;

# Add our lib path to the Perl search path.
use lib "../PerlLib/";
use BuildMod;
use TestsMod;
use ReportMod;

while (1) {

    # Build the Debug build.
    #@args = ("efDbg.txt");
    #$runTests = BuildMod::build(@args);

    TestsMod::runSuite("testsuite.txt");

    # Run the conformance tests only if the build was successful.
    if ($runTests) {

	print "Tests not being run right now.\n";

	# Temp way to run the tests.
	$save_dir = cwd; 
	chdir("D:/continuousbuilds/tests"); 
	print ("running tests.\n"); 
	system("tcl D:/continuousbuilds/scripts/runTestSuite.tcl"); 
	chdir($save_dir); 

    }

    # Build the Optimize build.
    #@args = ("efOpt.txt");
    #BuildMod::build(@args);

}
