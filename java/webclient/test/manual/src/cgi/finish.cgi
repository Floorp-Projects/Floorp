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

$LogPath = "<HTML_ROOT_DIR>/log";
$URLRoot = "<HTML_ROOT>/log";
$RESULT_FILE = "<RESULT_FILE>";
$CURRENT_LOG = "<CURRENT_LOG_DIR>";

#Do not modify this line!!!
$LogNPath = "";

&CopyLog;
exit 0;

sub CopyLog{
        #Open current log
        open(FILE, "$LogPath/$CURRENT_LOG/$RESULT_FILE");
	@LINES=<FILE>;
	$time=localtime(time);
	$aa = $time;
	$time=~s/\ |\:/_/g;
	mkdir("$LogPath/$time", 0777) || die "cannot mkdir ";
	$LogNPath=$time;
	open(FILE_LOG, ">$LogPath/$LogNPath/log.html");
	print FILE_LOG "<html>\n<head>\n<title>Log for Test of Webclient for $aa</title>\n</head>";
	print FILE_LOG "<body>\n<center><H1>Manual Tests for Webclient</H1>\n<H1>$aa</H1><hr hoshade></center>\n<table bgcolor=\"#99FFCC\" border=1>\n<tr bgcolor=\"lightblue\"><td>Test name</td><td>Value</td></tr>\n";
	$SIZE=@LINES;
        for ($i=0;$i<=$SIZE;$i++) {
          $_=$LINES[$i];
          ($name, $value) = split(/=/, $_);
	  if($value=~/FAILED/) {
	      print FILE_LOG "<tr bgcolor=red><td>$name</td><td>$value</td></tr>\n"; 
	  }else {
	      print FILE_LOG "<tr><td>$name</td><td>$value</td></tr>\n"; 
	  }
        }
        print FILE_LOG "</table>\n</body></html>";
	close FILE;
        close FILE_LOG;
        open(FILE_RES, ">$LogPath/$LogNPath/$RESULT_FILE");
        print FILE_RES @LINES;
        close FILE_RES;
	&ExitRedirect();

}

#######################################################################
sub ExitRedirect {
# Redirect to new log
    print redirect("$URLRoot/$LogNPath/log.html");
}

