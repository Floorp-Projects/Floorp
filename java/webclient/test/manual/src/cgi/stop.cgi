#!<PERL_DIR>/perl
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s):
# Client QA Team, St. Petersburg, Russia

use CGI qw(:standard);

$| = 1;
&Main;
exit 0;

sub Main{
        print "Content-type: text/html\n\n";
        print "<html>\n<head>\n<title>Stop's test page 1</title>\n<head><body>\n";
        print "<h1>Stop's test page 1 - Pressing STOP during loading page</h1>\n";
        print "<table>\n<tr><td></td><td width=\"50%\">Action</td><td>Expected result</td></tr>\n";
        print "<tr><td>1.</td><td width=\"50%\">Now, every second one string will be add to this page</td><td>You should look at this process</td></tr>\n";
        print "<tr><td>2.</td><td width=\"50%\">When you'll see next string: \"This is string number 8\", press Stop button.</td><td>Webclient should stop loaded this page, and do not hung up. </td></tr>\n";
        print "</table>\n";
        print "<hr>\n<br><br>Test PASSED if expected result matched obtained result, and FAILED otherwise.\n"; 
        print "<form action=<CGI_BIN_ROOT>/results.cgi method=post>\n";
        print "<input type=hidden name=testID value=manual/features/stop/1>\n";
        print "<input type=submit name=good value=PASSED>\n";
        print "<input type=submit name=bad  value=FAILED>\n";
        print "<hr>";
        for($i=0; $i<=10; $i++){
            print "This is string number $i<br>\n";
            sleep 1;
        }
        print "</body>\n";
        print "\n</html>";
}

