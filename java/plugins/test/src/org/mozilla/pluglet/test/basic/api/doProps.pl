#!/usr/local/bin/perl
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


if($#ARGV != 2) {
    die  "Usage:  doProps.pl propsFile inputFile outputFile \n";    
}

#Sort properties for key length
sortProps();
#read and replace
$file_data = readData($ARGV[1]);
open (PIH,$ARGV[0]) || die "Can't open file $ARGV[0]";
while ($pLine=<PIH>) {
    $pLine=~s#\n$##;
    @prop=split(/=/,$pLine);
    $prop[0]=~s#^\s*|\s*$##;
    $prop[1]=~s#^\s*|\s*$##;
    if(($prop[0] eq "")||($prop[1] eq "")) {
	die "Error in properties file $ARGV[0].\nEmpty property or property value.";
    }
    $file_data=~s#$prop[0]#$prop[1]#g;
}
#Save data
saveData($ARGV[2],$file_data);

sub sortProps {
    my @sort_arr;
    print "Sorting props..\n";
    open (PIH,$ARGV[0]) || die "Can't open file $ARGV[0]";
    while ($pLine=<PIH>) {
	unless($pLine=~/^\s*$/) {
	    $pLine=~s#\n$##;
	    my @t=split(/=/,$pLine);
	    $t[0]=~s#^\s*|\s*$##;
	    if ($sort_arr[length($t[0])] eq "") {
		$sort_arr[length($t[0])] = $pLine."\n";
	    } else 
	    {
		$sort_arr[length($t[0])] .=$pLine."\n";
	    }
	}
    }
    close PIH;
    open (POH,">".$ARGV[0]) || die "Can't open file $ARGV[0] rewrite. \nWe need rewrite it for sort props\n";
    for($i=$#sort_arr;$i>=0;$i--) {
	unless($sort_arr[$i]=~/^\s*$/) {
	    print POH $sort_arr[$i];
	}
    }
    close POH;    
}
sub readData {
    my $fileName = $_[0];
    my $all_file;
    open (IH,$fileName) || die "Can't open file $fileName";
    while ($line=<IH>) {
	$all_file.=$line
    }
    return $all_file;
}

sub saveData {
    my ($fileName,$data) = @_;
    if(-e $fileName) {
	rename($fileName,$fileName.".old") || die "Can't rename $fileName in $fileName.old\n";
	print "Existing file $fileName renamed in $fileName.old\n";
    }
    open (OH,">".$fileName) || die "Can't create file $fileName\n";
    print OH $data;
    close OH || die "Can't close file $fileName\n"; 
    print "File $fileName created\n";
}
