#!<PERL_DIR>/perl
use CGI qw(:standard);
#/*
# * The contents of this file are subject to the Mozilla Public
# * License Version 1.1 (the "License"); you may not use this file
# * except in compliance with the License. You may obtain a copy of
# * the License at http://www.mozilla.org/MPL/
# *
# * Software distributed under the License is distributed on an "AS
# * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# * implied. See the License for the specific language governing
# * rights and limitations under the License.
# *
# * The Original Code is mozilla.org code.
# *
# * The Initial Developer of the Original Code is Sun Microsystems,
# * Inc. Portions created by Sun are
# * Copyright (C) 1999 Sun Microsystems, Inc. All
# * Rights Reserved.
# *
# * Contributor(s):
# */

$LogPath = "<HTML_ROOT_DIR>/log";
$URLRoot = "<HTML_ROOT>/log";
$RESULT_FILE = "<RESULT_FILE>";
$CURRENT_LOG = "<CURRENT_LOG_DIR>";
$DESCRIPTION_FILE = "<DESCRIPTION_FILE>";

#Do not modify this line!!!
$LogNPath = "";

&CopyLog;
exit 0;

sub CopyLog{
        #Open current log
        open(DESCR, "$LogPath/$CURRENT_LOG/$DESCRIPTION_FILE");
        @LINES1=<DESCR>;
        ($tmp, $uid) = split(/=/, $LINES1[0]);
        ($tmp, $description) = split(/=/, $LINES1[1]);
        close(DESCR);
        unlink("$LogPath/$CURRENT_LOG/$DESCRIPTION_FILE");
        open(FILE, "$LogPath/$CURRENT_LOG/$RESULT_FILE");
	@LINES=<FILE>;
	$time=localtime(time);
	$aa = $time;
	$time=~s/\ |\:/_/g;
        chomp($uid);
	$LogNPath=$uid;
	mkdir("$LogPath/$LogNPath", 0777) || die "cannot mkdir ";
	open(FILE_LOG, ">$LogPath/$LogNPath/log.html");
	print FILE_LOG "<html>\n<head>\n<title>Log for Responsiveness Test of Current Browser for $aa</title>\n</head>";
	print FILE_LOG "<body>\n<center><H1>Responsiveness Tests for Browser</H1>\n<H1>$aa</H1><H1>Unique ID: $uid</H1><hr hoshade></center>\n<p><b>DESCRIPTIONS of these tests:&nbsp</b>$description</p>\n<table bgcolor=\"#99FFCC\" border=1>\n<tr bgcolor=\"lightblue\"><td>Test name</td><td>Time (seconds)</td></tr>\n";
	$SIZE=@LINES;
        for ($i=0;$i<=$SIZE;$i++) {
          $_=$LINES[$i];
          ($name, $value) = split(/=/, $_);
	  print FILE_LOG "<tr><td>$name</td><td><center>$value</center></td></tr>\n"; 
        }
        print FILE_LOG "</table>\n</body></html>";
	close FILE;
        close FILE_LOG;
        open(FILE_DESCR, ">$LogPath/$LogNPath/$DESCRIPTION_FILE");
        print FILE_DESCR @LINES1;
        print FILE_DESCR "time=$aa";
        close FILE_DESCR;
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

