#!/usr/bin/perl

# This script makes docshell test case templates. It takes one argument:
#
#   -b: a bugnumber
#
# For example, this command:
#
#   perl gen_template.pl -b 303267
#
# Writes test case template files test_bug303267.xul and bug303267_window.xul
# to the current directory.

use FindBin;
use Getopt::Long;
GetOptions("b=i"=> \$bug_number);

$template = "$FindBin::RealBin/test.template.txt";

open(IN,$template) or die("Failed to open input file for reading.");
open(OUT, ">>test_bug" . $bug_number . ".xul") or die("Failed to open output file for appending.");
while((defined(IN)) && ($line = <IN>)) {
        $line =~ s/{BUGNUMBER}/$bug_number/g;
        print OUT $line;
}
close(IN);
close(OUT);

$template = "$FindBin::RealBin/window.template.txt";

open(IN,$template) or die("Failed to open input file for reading.");
open(OUT, ">>bug" . $bug_number . "_window.xul") or die("Failed to open output file for appending.");
while((defined(IN)) && ($line = <IN>)) {
        $line =~ s/{BUGNUMBER}/$bug_number/g;
        print OUT $line;
}
close(IN);
close(OUT);

