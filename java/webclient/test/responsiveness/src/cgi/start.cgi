#!<PERL_DIR>/perl
use CGI;

# /*
#  * The contents of this file are subject to the Mozilla Public
#  * License Version 1.1 (the "License"); you may not use this file
#  * except in compliance with the License. You may obtain a copy of
#  * the License at http://www.mozilla.org/MPL/
#  *
#  * Software distributed under the License is distributed on an "AS
#  * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
#  * implied. See the License for the specific language governing
#  * rights and limitations under the License.
#  *
#  * The Original Code is mozilla.org code.
#  *
#  * The Initial Developer of the Original Code is Sun Microsystems,
#  * Inc. Portions created by Sun are
#  * Copyright (C) 1999 Sun Microsystems, Inc. All
#  * Rights Reserved.
#  *
#  * Contributor(s):
#  */

$LogPath = "<HTML_ROOT_DIR>/log";
$URLRoot = "<HTML_ROOT>";
$LST_FILE = "<TEMP_LST_FILE>";
$RESULT_FILE = "<RESULT_FILE>";
$CURRENT_LOG = "<CURRENT_LOG_DIR>";
$DESCR_FILE = "$LogPath/<CURRENT_LOG_DIR>/<DESCRIPTION_FILE>";

$LogNPath = "";

$query = new CGI;

@testsToRun = $query->param('TestsToRun');
$descr = $query->param('description');
$uid = $query->param('uid');
open(LST,">$LST_FILE");
print LST join("\n",@testsToRun);
close(LST);
MakeLogDir();
if(-d "$LogPath/$uid"){
    NewUidRequest();
}else{
    CreateDescriptionFile();
    StartTesting($testsToRun[0]);
}
exit 0;

sub MakeLogDir{
        #Open current log
        if(-e "$LogPath/$CURRENT_LOG"){
            if(-e "$LogPath/$CURRENT_LOG/$RESULT_FILE"){
                unlink("$LogPath/$CURRENT_LOG/$RESULT_FILE");
            }
        }else{
            mkdir("$LogPath/$CURRENT_LOG", 0777);
        }
}

#######################################################################
sub CreateDescriptionFile{
    if(-e $DESCR_FILE){
        unlink($DESCR_FILE);
    }
    open(LOG,">>".$DESCR_FILE) || die "ERROR $!";
    print LOG "uid=$uid\n";
    print LOG "description=$descr\n";
    close(LOG);
}   

sub NewUidRequest{
    print "Content-type:text/html\n\n";
    print "<html>\n<head>\n<title>Draft version of manual responsiveness tests.</title>\n</head>";
    print "<body bgcolor=white>\n<b><font color=red>Error, such uid : $uid already exist in logs, please enter new Unique ID</font></b><BR>\n Draft version of manual responsiveness tests.<BR>";
    print "Please select or unselect tests, that you want run, enter UID and Descriptions (it may be Browser, platform, OS etc.) of tests and then press a <B>Start</B> button.<hr>";
    print "<form action=\"<CGI_BIN_ROOT>/start.cgi\" method=post>\n<CHECKBOX_GROUP>\n<hr>\n";
    print "Please also enter a Description and UID of tests.<br>\n<table border=0 width=\"60\%\">\n";
    print "<tr><td><b>Description: </b></td><td><textarea name=description rows=5 cols=40 >$descr</textarea></td></tr>\n";
    print "<tr><td colspan=2><b><font color=red>Please enter another Unique ID</font></b></td></tr>";
    print "<tr><td><b>Unique ID: </b></td><td><input type=text name=uid value=\"\" size=47><br></td></tr>\n";
    print "</table>\n<input type=submit value=\"Start\">\n</form>\n</body>\n</html>";

}
#######################################################################
sub StartTesting {
    my $value = shift(@_);
    $value=~s/%2F/\//g;
    print "Content-type:text/html\n\n";
    print "<html><head>";
    #print "<meta http-equiv=refresh content=\"5,$URLRoot/$value.html\">
    print "</head><body bgcolor=white>";
    print "First test page: <A href=$URLRoot/$value.html>$URLRoot/$value.html</A><BR>";
    print "<HR><P align=left>\n";
    print "All logs will be temporary placed to the <b>$LogPath/$CURRENT_LOG/$RESULT_FILE</b>.<BR>";
    print "At the end of testing all log entries will be copied to unique directory: <b>$LogPath/$uid/</b>";
    print "</P><HR>";
    print "The following tests selected:<BR>";
    print "<OL>";
    print join("<BR>\n",@testsToRun);
    print "</OL>";
    print "</body></html>";   
}





