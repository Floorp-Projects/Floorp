#!/bin/perl
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

###################################################################
#
# lst_gen.pl - configuration utility. Cretate checkbox group in
# the index.html	

sub getCheckBoxes {
    my $lst_file = shift(@_);
    my $curID="";
    my $checkBoxLine="";
    open( LST, $lst_file) || die ("Can't open lst file $lst_file for reading. $!\n");
    while($curID = <LST>) {
	$curID =~ s/\s*//g; #ID shouldn't contains spaces inside, so remove it from begin and end 
	unless(($curID=~/^\s*\#/)||($curID=~/^\s*$/)) {
	       $checkBoxLine.="<input type=checkbox name=TestsToRun checked value=$curID>$curID<BR>\n";
	   }
    }
    close(LST);
    return $checkBoxLine;
}


###########################
#  Main procedure
##########################

if( $#ARGV != 1) {
	printf "Usage: perl lst_gen.pl <html file> <input lst file>\n";
	exit 0;
};

$html   = $ARGV[0];
$lst  = $ARGV[1];
$checkBoxLine = getCheckBoxes($lst);
@html_storage;
$i=0;
open( HTML, $html ) || die("Can't open $html for reading. $!\n");
while( $line = <HTML> ) {
    if( $line =~ /<CHECKBOX_GROUP>/ ) {
	$line=~s/<CHECKBOX_GROUP>/$checkBoxLine/g;
    } # if
    $html_storage[$i]=$line;
    $i++;
} # while

#Rewriting html file
close(HTML);
open( HTML, ">$html") || die("Can't open $html for writing. $!\n");
print HTML @html_storage;
close(HTML);








