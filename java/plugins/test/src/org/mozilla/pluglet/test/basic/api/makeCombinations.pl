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
# This script used to create listing with all possible combinations of
# arguments.
# Input data:
#
# combinationsFile - a file with the values of each argument. See README.paramCombinations for 
#                    more details about it's format. 
# outputFile       - output file name. Usually lst file in your src test dir.
# lstLogFile       - Test id's will be added to this LST file. Usually is's your BWTests.lst.ORIG
#

$SEPARATOR="\\";
$DEBUG=0;

if ($#ARGV != 2) {
    die  "Usage:  makeCombinations.pl combinationsFile outputFile lstLogFile\n";  
}
$combinationsFile = $ARGV[0];
$outputFile = $ARGV[1];
$lstLogFile = $ARGV[2];

@s1=split(/\\/,$outputFile);
@s1=split(/\./,$s1[$#s1]);
$testId=$s1[0];
@propsArr;

#Read data, make combinations and save.
@file_data = readData($pcombDir.$combinationsFile);
@combs=allComb(\@file_data);
saveData($outputFile,\@combs);

sub allComb {
    my $strings = $_[0];
    my @tmp;
    my @fields=split(/, /,$$strings[$#$strings]);
    my $k=$#$strings-1;
    while($k>=0) {
	@tmp=split(/, /,$$strings[$k]);
	@fields=getCombination(\@tmp,\@fields);  
	$k--;
    }
    if($DEBUG eq "0") {
	for($k=0;$k<=$#fields;$k++) {
	    $fields[$k]=~s#\'0|\'1##g;
	    $fields[$k]=~s#,#, #g;
	}
    }
    return @fields;
}
sub saveData {
    my ($fileName,$combs) = @_;
    if(-e $fileName) {
	rename($fileName,$fileName.".old") || die "Can't rename $fileName in $fileName.old\n";
    }
    my $testIdStr = "basic/api/".$testId;
    open (OH,">".$fileName) || die "Can't create file $fileName\n";
    open (OLSTH,">>".$lstLogFile) || die "Can't create file $lstLogFile\n";
    my $case_count=0;
    foreach $comb (@$combs) {
	print OH $case_count,": ";
	print OH $comb,"\n";
	print OLSTH $testIdStr,":",$case_count,"\n";
	$case_count++;
    }
    close OLSTH || die "Can't close file $lstLogFile\n"; 
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
	    $line=~s#^\s*|\s*$##g;
	    $strings[$i]=$line;
	    $i++;
	}
    }
    return @strings;
}

sub getCombination {
    my ($arr1,$arr2) = @_;
    my $i=0;
    my $k=0;
    my $count=0;
    my @cr=('');
    while($i<=$#$arr1) {
	$vstr1=$$arr1[$i];
        @t1=split(/'/,$vstr1);
        $EXT_FLAG=$t1[1];
	$j=0;
	while($j<=$#$arr2) {
	    $vstr2=$$arr2[$j];
            @t2=split(/,/,$vstr2);
            $k=0;
	    $kFlag="0";
	    while($k<=$#t2) {
		if($t2[$k]=~/.*\'1.*/) {
		    $kFlag="1";
		    $k=$#t2;
		}
		$k++;
	    }
	    if(($EXT_FLAG eq "0")||($kFlag eq "0")) {
	       $cr[$count]=$vstr1.",".$vstr2;
	       $count++;
	     }
	    $j++
            }
	$i++;
    }
    return @cr;
}








