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
    printf("Usage: lstGen.pl <upper_test_dir> <output_lst_file>\n");
    exit(-1);
}

#Main
if($#ARGV !=1 ) {
    usage();
}
$basic_dir=$ARGV[0];
$output_file=$ARGV[1];
(-d $basic_dir) || die ("ERROR: $basic_dir doesn't exist !\n");
#Assume that depth is 2 or less
@all_dirs;
@all_id;
$i=0;
while($nextdir = <$basic_dir/*>) {
    (-d $nextdir) || next;
    (-f "$nextdir/TestProperties") || next;
    $nextdir =~s/\\/\//g;
    $nextdir =~/.*\/(basic.*)/;
    $id = $1;
    #print("Processng $nextdir ...\n");
    chomp($id);
    if (-f "$nextdir/Test.lst") {
       #print("Found Test.lst file ...");
       open(IN,"$nextdir/Test.lst") || die "Can't open $nextdir/Test.lst\n";
       while($line=<IN>) {
          if($line=~/^.*\:/) {
              $line=~s/:.*$//g;
              chomp($line);
              $all_id[$i] = "$id:$line";
              $i++;
          }
       }
       close(IN);
    } else {
        $all_id[$i] = $id;
        $i++;
    }
}
#print("Size is $#all_dirs_with_props");
open(OUT,">$output_file") || die "Can't open output file\n";
print(OUT join("\n",@all_id));
close(OUT) || die "Can't close output file\n";;







