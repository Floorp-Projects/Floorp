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

$SEPARATOR="\\";
$DEBUG=0;

if ($#ARGV < 0) {
    die  "Usage:  makeProps.pl [-p parametersFile] combinationsFile outputFile lstLogFile\n";  
}
if ($ARGV[0] eq "-p") {
    if ($#ARGV != 4) {
	die "Usage:  makeProps.pl [-p parametersFile] combinationsFile outputFile lstLogFile\n";  
    }
    unless (-e $ARGV[1]) {
	die "Parameters file  $ARGV[1] doesn't exist.\n"
    }
    $parametersFile = $ARGV[1];
    $combinationsFile = $ARGV[2];
    $outputFile = $ARGV[3];
    $lstLogFile = $ARGV[4];
} else {
    if ($#ARGV == 2) {
	$combinationsFile = $ARGV[0];
	$outputFile = $ARGV[1];
	$lstLogFile = $ARGV[2];
	$parametersFile="";
    } else {
	die "Usage:  makeProps.pl [-p parametersFile] combinationsFile outputFile  lstLogFile\n";
    }
}
@s1=split(/\\/,$outputFile);
@s1=split(/\./,$s1[$#s1]);
$testId=$s1[0];
@propsArr;
unless ($parametersFile eq "") {
    @propsArr = readAndSortProps($parametersFile);
    @file_data = readData($combinationsFile);
    $file_data_string = join("m47Grt",@file_data);
    for($i=$#propsArr;$i>=0;$i--) {
	$pLine=$propsArr[$i];
        unless($pLine=~/^\s*$/) {
	    $pLine=~s#\n$##;
	    $pLine=~s#\r$##;
            @prop=split(/=/,$pLine);
            $prop[0]=~s#^\s*|\s*$##;
            $prop[1]=~s#^\s*|\s*$##;
            if(($prop[0] eq "")||($prop[1] eq "")) {
		die "Error in properties file $parametersFile.\nEmpty property or property value.";
	    }
	    $file_data_string=~s#$prop[0]#$prop[1]#g; 
	}
    }
    @file_data = split(/m47Grt/,$file_data_string);
} else {
    @file_data = readData($pcombDir.$combinationsFile);
}
@combs=allComb(\@file_data);
saveData($outputFile,\@combs);

sub readAndSortProps {
    my $pFileName=@_[0];
    my @sort_arr;
    open (PIH,$pFileName) || die "Can't open file $pFileName";
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
    my $sort_arr=join(" ",@sort_arr);
    #print $sort_arr;
    @sort_arr=split(/\n/,$sort_arr);
    close PIH;
    return @sort_arr;

}


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
	unless($line=~/^\s*$/) {
	    $line=~s#\n$##;
	    $line=~s#\r$##;
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








