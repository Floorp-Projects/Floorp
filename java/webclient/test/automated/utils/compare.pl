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

sub usage {
    print "Usage: perl compare.pl -o <output_file> <log1> <log2> [log3] ... [logN]\n ";
}

sub addOut{
    my $str = shift;
    $commonOut.=$str;
}
sub printLine {
    my $id=shift @_;
    my $results=shift @_;
    my $comments=shift @_;
    my $aname="";
    my $i=0;
    $aname=$id;
    $aname =~ s/\/|\:/_/g;
    if($brief == 1) {
	addOut "<tr><td valign=center><A name=brief_$aname href=\#full_$aname>$id</A></td>";
    }else {
	addOut "<tr><td rowspan=2 valign=center><A name=full_$aname href=\#brief_$aname>$id</A></td>";
    }
    for($i=0;$i<=$#$results;$i++) {
	if ($$results[$i] =~ /PASSED/) {
	    $bgcolor="GREEN";
	}elsif ($$results[$i] =~ /FAILED/) {
	    $bgcolor="RED";
	}elsif ($$results[$i] =~ /UNIMPLEMENTED/) {
	    $bgcolor="YELLOW";
	}else {
	    $bgcolor="BLUE";
	}
	addOut "<td align=center bgcolor=$bgcolor>$$results[$i]</td>";
    }
    addOut "</tr>\n";
    if($brief != 1) {
	addOut "<tr>";
	for($i=0;$i<=$#comments;$i++) {
	    addOut "<td>$$comments[$i]</td>";
	}
	addOut "</tr>\n";
    }
}



#Main
$brief=0;
if(($ARGV[0] eq "-b")&&($ARGV[1] eq "-o")&&($#ARGV >=4)) {
    $brief = 1;
    $outputfile = $ARGV[2];
    $argcount = 3;
}elsif(($ARGV[0] eq "-o")&&($#ARGV >=3)) {
    $brief = 0;
    $outputfile = $ARGV[1];
    $argcount = 2;
}else {
    usage();
    exit -1;
}
$commonOut="";
addOut "<html><body bgcolor=white>";
while($brief>=0) {

    $logcount=$#ARGV-$argcount;
    @logs=();
    $maxcount=0;
    $max = 0;
    for($i=0;$i<=$logcount;$i++) {
	my %tmp=();
	open(IN,$ARGV[$i+$argcount]) || die;
	while($curline=<IN>) {
	    $curline=~/^(.*?=)(.*?=)(.*)$/;
	    $class = $1;
	    $status = $2;
	    $comment = $3;
	    $class =~ s/=$//g;
	    $status =~ s/=$//g;
	    $tmp{$class} = "$status=$comment";
	}
	close(IN);
	if($max<keys(%tmp)) {
	    $max=keys(%tmp);
	    $maxcount=$i;
	}
	$logs[$i] = \%tmp;
    }
    $maxhash = $logs[$maxcount];
    if($brief ==1) {
	addOut "<BR><center><H1>Brief table of results</H1></center><BR>\n";
    }else {
        addOut "<BR><center><H1>Full table of results</H1></center><BR>\n";
    }
    addOut "<table border=1 bgcolor=lightblue width=100%>\n";
    foreach $id (keys %{$maxhash}) {
	@results=[];
	@comments=[];
	for($i=0;$i<=$logcount;$i++) {
	    $curline = $logs[$i]->{$id};
	    if(defined $curline) {
		$curline=~/^(.*?)=(.*)$/;
		$results[$i] = $1;
		$comments[$i] = $2;
	    } else {
		$results[$i] = "UNDEF";
		$comments[$i] = "UNDEF"
		}
	}
	printLine($id,\@results,\@comments);
    }
    addOut "</table>\n";
    $brief--;
}
addOut "</body></html>\n";
open(OUT,">$outputfile") || die "Can't create $outputfile\n";
print OUT $commonOut;
close(OUT);







