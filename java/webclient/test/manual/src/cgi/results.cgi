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

$LOG_DIR="<HTML_ROOT_DIR>/log";
$HTML_ROOT="<HTML_ROOT>";
$LST_FILE="<TEMP_LST_FILE>";
$LOG_FILE="$LOG_DIR/<CURRENT_LOG_DIR>/<RESULT_FILE>";
$FINISH_HTML="<FINISH_HTML>";

#Define routines

sub getNextID {
    my $currentID=shift(@_);
    my $found=0;
    my $nextID="";
    open(LST,$LST_FILE) || die "Can't open $!";
    while($test=<LST>) {
	$test=~s/\r|\n|\s//g;	
	if($currentID eq $test) {
	    print "Current is $test";
	    $found=1;
	    last;
	}
    }
    if($found==1) {
	$nextID=<LST>;
	$nextID=~s/\r|\n|\s//g;
	if($nextID=~/$\s*#/) {
	   close(LST);
	   return getNextID($nextID);
        }
    } 
    close(LST);
    return $nextID;
}

sub redirect {
    $new_url=shift(@_);
    print "Status: 301 Redirect\n";
    print "Content-type: text/html\n";
    print "Location: $new_url\n\n\n";
}

sub getParameters() {
    my $parameters=<STDIN>;
    my $testID;
    my $result;
    ($testID,$result)=split(/\&/,$parameters);
    $testID=~s/testID\=//g;
    $testID=~s/%2F/\//ig;
    $result=~s/good\=|bad\=//g;
    return ($testID,$result);
}

sub addLogEntry {
    my $testID = shift(@_);
    my $result = shift(@_);
    open(LOG,">>".$LOG_FILE) || die "ERROR $!";
    print LOG "$testID=$result\n";
    close(LOG);
}
##############################################################################
#                              Main
##############################################################################
($currentID,$result)=getParameters();
addLogEntry($currentID,$result);
$nextID=getNextID($currentID);
 unless ($nextID eq "") {
   redirect($HTML_ROOT."/".$nextID.".html");
} else {
   redirect("$HTML_ROOT/$FINISH_HTML");
}










