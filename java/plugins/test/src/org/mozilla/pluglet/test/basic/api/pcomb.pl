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

$DEBUG=0;

if ($#ARGV < 0) {
    die  "Usage:  pcomb.pl [-d outputDir] file1 file2 ...\n";  
}
if ($ARGV[0] eq "-d") {
    if ($#ARGV < 2) {
	die "Usage:  pcomb.pl [-d outputDir] file1 file2 ...\n";  
    }
    unless (-e $ARGV[1]) {
	die "Directory $ARGV[1] doesn't exist.\n"
    }
    $outputDir=$ARGV[1];
    $fc=2;
} else
{
    $fc=0;
    $outputDir="";
}

for($l=$fc;$l<=$#ARGV;$l++) {
    $file=$ARGV[$l];
    @strings=readData($file);
    @combs=allComb(\@strings);
    saveData($outputDir."/".$file.".lst",\@combs);
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
	print "Existing file $fileName renamed in $fileName.old\n";
    }
    open (OH,">".$fileName) || die "Can't create file $fileName\n";
    foreach $comb (@$combs) {
	print OH $comb,"\n";
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
	unless($line=~/^\s*$/) {
	    $line=~s#\n$##;
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





