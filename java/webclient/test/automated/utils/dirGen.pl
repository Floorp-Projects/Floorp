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

use Cwd ; 
use Cwd 'abs_path';
sub usage {
    printf("Usage: dirGen.pl <test_dir> <output_file>\n");
    exit(-1);
}

#Main
if($#ARGV !=1 ) {
    usage();
}
$cur_dir=cwd();
$test_dir=abs_path($ARGV[0]);
$output_file=$ARGV[1];
(-d $test_dir) || die ("ERROR: $test_dir doesn't exist !\n");
#Assume that depth is 1
@all_dirs;
$i=0;
chdir($test_dir);
while($nextdir = <*>) {
    (-d $nextdir) || next; #Proceed only directories
    ($nextdir =~ /SCCS$/) && next; #ignore all SCCS entries
    ($nextdir =~ /CVS$/) && next; #ignore all CVS entries
    unless (-f "$nextdir/TestProperties") {
	print "Skipping $nextdir\n";
	next;
    }
    $nextdir =~s/\\/\//g;
    chomp($nextdir);
    $all_dirs[$i] = $nextdir;
    $i++;
}
chdir($cur_dir);
open(OUT,">$output_file") || die "Can't open output file\n";
print(OUT join("\n",@all_dirs));
close(OUT) || die "Can't close output file\n";;







