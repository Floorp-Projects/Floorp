#!/usr/bin/perl
#
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
# Date  : Aug. 6, 2001
#
require 'getopts.pl';
#
# Usage: $progname [Options] fromString toString [file|directory]*
#
#
sub usage {
    die
"\n::\n",
"\n",
"  Usage: $progname [Options] fromString toString [file|directory]* \n",
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

sub strop 
{
	open (INFILE, $_[0]);
	$newfile = $_[0] . ".new";
	$orgfile = $_[0] . ".org";
	print ">> writing to: $newfile\n\n";
	
	open (TMPFILE, "> $newfile");
	
	$aline;
	while ( <INFILE> ) {
	  if (/$patstr/ && /$oldstr/) {
		print "\n>> current line: $_";
		$_ =~ s/$oldstr/$newstr/;
		print TMPFILE $_;
	  }
	  else {
		print TMPFILE $_;
	  }
	}
    # while
	close (INFILE);
	close (TMPFILE);
	#
	# backup the file and put the new one in place
	print " --> renaming $_[0] to $orgfile ... \n";
	rename($_[0], $orgfile);
	print " --> renaming $newfile to $_[0] ...\n";
	rename($newfile, $_[0]);

	print ">> done!\n\n";
}

#--------------------------------------------------------------------------
#  main body
#--------------------------------------------------------------------------
#
# Extract base part of this script's name
($progname) = $0 =~ /([^\/]+)$/;
#
&usage if (!&Getopts('p:h') || ($#ARGV < 2));
&usage if ($opt_h);		# help option
#
$count = 0;
$oldstr = $ARGV[$count++];
$newstr = $ARGV[$count++];
$patstr = ($opt_p)?($opt_p):"";

@myargv = $ARGV[$count];
print "\n:: oldstr    =$oldstr\n",
        ":: newstr    =$newstr\n",
        ":: patstr    =$patstr\n",
        ":: target file\(s\) = @myargv \n\n";
#
#
$count = 0;
foreach $arg (@myargv) {
  print ">> file $count: $myargv[$count]\n\n";
  &strop($arg);
  $count++;
}
#
printf "\n";

