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
#
#
# This script used to replace parameter symbolic names with it values.
#
# Input data:
# 
# parametersFile  - file with test parameters(e.g. user/location specific values), 
#                   usually TestParameters file in your src test directory
# inputLSTFile    - wile with arguments, where symbolic names will be replaced
# outputFile      - output lst file, usually lst file in your build test dir
#

$SEPARATOR="\\";
$DEBUG=0;

if ($#ARGV != 2) {
    die  "Usage:  makeProps.pl parametersFile inputLSTFile outputFile\n";  
}
$parametersFile = $ARGV[0];
$inputFile = $ARGV[1];
$outputFile = $ARGV[2];


@propsArr = readAndSortProps($parametersFile);
@file_data = readData($inputFile);
$file_data_string = join("m47Grt",@file_data);

for($i=$#propsArr;$i>=0;$i--) {
    $pLine=$propsArr[$i];
    chomp($pLine);
    unless($pLine=~/^\s*$/) {
	@prop=split(/=/,$pLine);
	$prop[0]=~s#^\s*|\s*$##;
	$prop[1]=~s#^\s*|\s*$##;
	if(($prop[0] eq "")||($prop[1] eq "")) {
	    die "Error in properties file $parametersFile.\nEmpty property or property value.";
	}
	$file_data_string=~s#$prop[0]#$prop[1]#g;
    }
    @file_data = split(/m47Grt/,$file_data_string);
}

saveData($outputFile,\@file_data);

sub readAndSortProps {
    my $pFileName=@_[0];
    my @sort_arr;
    open (PIH,$pFileName) || die "Can't open file $pFileName";
    while ($pLine=<PIH>) {
	unless($pLine=~/^\s*$|^\s*#/) {
	    chomp $pLine;
	    my @t=split(/=/,$pLine);
	    $t[0]=~s#^\s*|\s*$##g;
	    $t[1]=~s#^\s*|\s*$##g;
	    $pLine=$t[0]."=".$t[1];   
	    if ($sort_arr[length($t[0])] eq "") {
		$sort_arr[length($t[0])] = $pLine."\n";
	    } else 
	    {
		$sort_arr[length($t[0])] .=$pLine."\n";
	    }
	}
    }
    my $sort_arr=join(" ",@sort_arr);
    #print $sort_arr;
    @sort_arr=split(/\n/,$sort_arr);
    close PIH;
    return @sort_arr;

}

sub saveData {
    my ($fileName,$data) = @_;
    if(-e $fileName) {
	rename($fileName,$fileName.".old") || die "Can't rename $fileName in $fileName.old\n";
    }
    open (OH,">".$fileName) || die "Can't create file $fileName\n";
    my $case_count=0;
    foreach $d (@$data) {
	print OH $d,"\n";
    }
    close OH || die "Can't close file $fileName\n"; 
    print "File $fileName created\n";
}

sub readData {
    my $fileName = $_[0];
    my $i=0;
    my @strings;
    open (IH,$fileName) || die "Can't open file $fileName";
    while ($line=<IH>) {
	chomp $line;
	unless($line=~/^\s*$|^\s*\#/) {
	    $strings[$i]=$line;
	    $i++;
	}
    }
    close IH;
    return @strings;
}








