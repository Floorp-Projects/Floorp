#!/usr/bin/perl
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Tao Cheng <tao@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
#
# Author: Tao Cheng <tao@netscape.com>
# Date  : July 1, 2002
#
require 'getopts.pl';
#
# Usage: $progname [Options] en_file l10n_file 
#
#
sub usage {
    die
"\n::\n",
"\n",
"  Usage: $progname [Options] en_file l10n_file l10n_code\n",
"\n",
"    $progname takes one or more filenames or directories as input\n",
"    arguments. It substitutes instances of \'fromString\'with \'toString\' \n",
"    in the provided files or directories.\n",
"\n",
"    $progname enters a given directory and its sub-directories\n",
"    to process all text files encountered.\n",
"\n",
"  Options:\n",
"      -h             Print help (this message)\n",
"      -p pattern_str substitute only when the line matches pattern_str \n",
"\n::\n",
"\n";
}

sub getKeyVal
{
    my (@tokens, $key, $value, $pos);
    if (/^#/) {
	# print "comment:$_";
    }
    else {
	$pos = index($_, "=");
	if ($pos) {
	    $key = substr($_, 0, $pos);
	    $value = substr($_, $pos+1);
	    #print "\n-- before (pos,key,value)=($pos,$key,$value)";
	    # trim white space
	    $key =~ s/^[\s]|[\s]$//;
	    $value =~ s/^[\s]|[\s]$|[\r\n]$//g;
	    @tokens = ($key,$value);
	    #print "\n-- getKeyVal: (pos,key,value)=($pos,$key,$value)";
	}
    }
    @tokens;
}

#
# arg 0: Arr
#
sub dumpArr 
{
  my (@Arr, $count, $arr);
  $count = 0;
  @Arr = @_;
  foreach $arr (@Arr) {
	print "\n$count:", $Arr[$count];
	++$count;
  }
  print "\n** End of dumpArr \n";
}

sub getEntity
{
    my (@tokens, $token, @pair, $key, $value, $remaining, $del, $pos, $pos1, $pos2);
    if (/\<\!ENTITY/) {
	@tokens = split(/\s/, $_);
	
	#print "\n pos=$pos";
	#dumpArr(@tokens);
	if ($#tokens > 1) {
	    if ($tokens[1]) {
		$key = $tokens[1];
	    }
	    elsif ($tokens[2]) {
		$key = $tokens[2];
	    }
	    $pos1 = index($_, "\"");
	    $pos2 = rindex($_, "\"");
	    #print "\n-- (pos1,pos2,key,value)=($pos1,$pos2,$key,$value)";
	    if ($pos2 > $pos1) {
		$value = substr($_, $pos1 + 1, $pos2-$pos1-1);
		#print "\n-- getEntity(): (pos1,pos2,key,value)=($pos1,$pos2,$key,$value)";
		@pair = ($key,$value);
	    }
	}
    }
    else {
	# print "-- getEntity(), comment:$_";
    }
    @pair;
}

sub buildGlossary
{
	open (INFILE, $_[0]);
	my (@tokens, $count, $blank, %keyValueTab, $isDTD);
	$isDTD = 0;
	if ($_[0] =~ /dtd$/) {
	    $isDTD = 1;
	    print "\n ** $_[0]: isDTD! **";
	}
	$count = 0;
	$nonblank = 0;
	while ( <INFILE> ) {
	    if ($isDTD) {
		@tokens = &getEntity($_);
	    }
	    else {
		@tokens = &getKeyVal($_);
	    }
	    if ($#tokens > 0) {
		#print "\n ** tokens=($tokens[0],$tokens[1])";
		$keyValueTab{$tokens[0]} = $tokens[1];
		$count++;
		if ($tokens[1]) {
		    $nonblank++;
		}
	    }
	}
	# while
	close (INFILE);
	print "\n>> buildGlossary(), count=$count, nonblank=$nonblank\n\n";

	%keyValueTab;
}

sub replaceEntries()
{
	my ($outf, @tokens, $hits, $miss, $value, $l10n_code);
	if ($_[1]) {
	    $l10n_code = "." . $_[1];
	}
	else {
	    $l10n_code = ".l10n";
	}
	$outf = $_[0] . $l10n_code;
	open (INFILE, $_[0]);
	open (OUTFILE, "> $outf");
	# &dumpTable(%glossaryTab);
	$isDTD = 0;
	if ($_[0] =~ /dtd$/) {
	    $isDTD = 1;
	    print "\n ** $_[0]: isDTD! **";
	}

	$hits = 0;
	$miss = 0;
	while ( <INFILE> ) {
	    if ($isDTD) {
		@tokens = &getEntity($_);
	    }
	    else {
		@tokens = &getKeyVal($_);
	    }
	    if ($#tokens > 0 && $tokens[0]) {
		#print "\n ** replaceEntries: tokens=($tokens[0],$tokens[1])";
		$value = $glossaryTab{$tokens[0]};
		if ($value) {
		    #print "\n replaceEntries(hit)  : (key,value)=($tokens[0],$value)";
		    $hits++;
		    #print "$tokens[0]=$value\n";
		    if ($isDTD) {
			my ($pos1, $aline);
			$pos1 = index($_, "\"");
			$aline = substr($_, 0, $pos1+1) . $value . "\"\>\n";
			#print "\n--aline=$aline\n";
			print OUTFILE $aline;
		    }
		    else {
			print OUTFILE "$tokens[0]=$value\n";
		    }
		}
		else {
		    print "\n replaceEntries(miss) : (key,value)=($tokens[0],$value)";
		    $miss++;

		    $aline = $_;
		    $aline =~ s/[\r\n]$//;
		    print OUTFILE "$aline\n";
		}
	    }
	    else {
		print OUTFILE $_
	    }
	}
	# while
	close (INFILE);
	close (OUTFILE);
	print "\n--replaceEntries hits=$hits, miss=$miss, done!\n\n";

}
#
# dump a hash table to the console
#
sub dumpTable()
{
	my (%Table, $name, $lineno);
	$lineno = 0;
	%Table = @_;
	
	foreach $name (sort keys %Table) {
		++$lineno;
		print "\n dumpTable: $lineno: $name=$Table{$name}";
	}
}
#--------------------------------------------------------------------------
#  main body
#--------------------------------------------------------------------------
#
# Extract base part of this script's name
($progname) = $0 =~ /([^\/]+)$/;
#
#&usage if (!&Getopts('p:h') || ($#ARGV < 2));
&usage if ($opt_h);		# help option
#
$count = 0;
$en_file   = $ARGV[$count++];
$l10n_file = $ARGV[$count++];
$l10n_code = $ARGV[$count++];

@myargv = $ARGV[$count];
print "\n:: en_file   =$en_file\n",
    ":: l10n_file =$l10n_file\n",
    ":: l10n_code =$l10n_code\n";
#
#
%glossaryTab = &buildGlossary($l10n_file);
#&dumpTable(%glossaryTab);
#
&replaceEntries($en_file, $l10n_code);
#
printf "\n";

