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

use CGI;

$LogPath = "<HTML_ROOT_DIR>/log";
$URLRoot = "<HTML_ROOT>";
$LST_FILE = "<TEMP_LST_FILE>";
$RESULT_FILE = "<RESULT_FILE>";
$CURRENT_LOG = "<CURRENT_LOG_DIR>";
$CGI_BIN_ROOT_DIR = "<CGI_BIN_ROOT_DIR>";
$TEMP_LST_FILE = "<TEMP_LST_FILE>";

$LogNPath = "";

$query = new CGI;

@testsToRun = $query->param('TestsToRun');
$continue = $query->param('continue');
$stage = $query->param('stage');
open(LST,">$LST_FILE");
print LST join("\n",@testsToRun);
close(LST);
MakeLogDir();
if($stage == 1){
    if($continue =~ /Continue/){
        ContinueTesting($testsToRun[0]);
    }else{
        StartTesting($testsToRun[0]);
    }
}else{
    StartTesting($testsToRun[0]);
}
exit 0;

sub MakeLogDir{
        #Open current log
        if(-e "$LogPath/$CURRENT_LOG"){
            if(-e "$LogPath/$CURRENT_LOG/$RESULT_FILE"){
                if($continue!~/Continue/){
                    unlink("$LogPath/$CURRENT_LOG/$RESULT_FILE");
                }
            }
        }else{
            mkdir("$LogPath/$CURRENT_LOG", 0777);
        }
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
    print "At the end of testing the unique directory will be created in the <B>$LogPath/</b><BR> ";
    print "and all log entries will be copied to this unique directory.";
    print "</P><HR>";
    print "The following tests selected:<BR>";
    print "<OL>";
    print join("<BR>\n",@testsToRun);
    print "</OL>";
    print "</body></html>";   
}

sub ContinueTesting {
    my $value = shift(@_);
    $value=~s/%2F/\//g;
    if(-e ("$LogPath/$CURRENT_LOG/$RESULT_FILE") && ("$CGI_BIN_ROOT_DIR/$TEMP_LST_FILE")){
        open(RES_FILE, "$LogPath/$CURRENT_LOG/$RESULT_FILE");
        open(LST_TEMP, "$CGI_BIN_ROOT_DIR/$TEMP_LST_FILE");
        @LINES_RES = <RES_FILE>;
        @LINES_LST = <LST_TEMP>;
        close(RES_FILE);
        close(LST_TEMP);
        @LINES_RES = sort(@LINES_RES);
        @LINES_LST = sort(@LINES_LST);
        $SIZE_RES = @LINES_RES;
        $SIZE_LST = @LINES_LST;
        print "Content-type:text/html\n\n";
        print "<html>\n<head>\n<title>Select / Unselect tests</title>";
        print "</head><body bgcolor=white>\n";
        print "Unselected tests were done in previous session. <br>Please review/correct checked/unchecked checkboxes and press <b>Continue</b> button.<hr>\n";
        print "<form action=\"<CGI_BIN_ROOT>/start.cgi\" method=post>\n";
        $i1 = 0;
        for($i=0; $i<$SIZE_LST; $i++){
            ($line, $tmp) = split(/=/, $LINES_RES[$i1]);
            #print "$line<br>$LINES_LST[$i]<br>i1=$i1";
            if($LINES_LST[$i] =~ /$line/){
                $curID = $LINES_LST[$i] ;
                print "<input type=checkbox name=TestsToRun unchecked value=$curID>$curID<BR>\n";
                $i1++;
            }else{
                $curID = $LINES_LST[$i] ;
                print "<input type=checkbox name=TestsToRun checked value=$curID>$curID<BR>\n";
            }
        }
        print "<input type=hidden name=stage value=2>\n<hr><p align=right><input type=submit name=continue value=Continue></p>\n";
        print "</form>\n</body>\n</html>";   
    }else{
        print "Content-type:text/html\n\n";
        print "<html><head><title>Error: previous session not exist or corrupted</title>";
        #print "<meta http-equiv=refresh content=\"5,$URLRoot/$value.html\">
        print "</head><body bgcolor=white>\n";
        print "<h1>Error: previous session not exist or corrupted</h1>\n";
        print "<input type=button value=\"Back\" onClick=\"history.back()\">";
        print "</body></html>";   
    }

}




