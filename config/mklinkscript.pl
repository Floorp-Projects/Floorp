#!/usr/bin/perl -w
#
# This perl script takes an order file produced by the "order" program
# as input (specified on the command line) and outputs a script for
# GNU ld that orders functions in the specified order, assuming that
# all of the source files were compiled with the -ffunction-sections
# flag.  Output goes to the file specified with "-o" flag.

use Getopt::Std;
getopts("o:");
if ($#ARGV != 0 || !defined $::opt_o) {
    die("usage: mklinkscript -o script-file order-file");
}
$linkScript = $::opt_o;
$tmpFile = "$linkScript.tmp";
$orderFile = $ARGV[0];

open(LD, "ld --verbose|") || die("ld: $!");

#
# Initial output is to a temp file so that if we fail there won't be
# broken GNU ld script lying around to confuse the build and anyone
# trying to use it.
#
open(TMP, ">$tmpFile") || die("$tmpFile: $!");

sub PrintOrder {
    my $line;
    open(ORDER, $orderFile) || die("$orderFile: $!");
    while ($line = <ORDER>) {
        chomp $line;
        print(TMP "*(.text.$line)\n");
        print(TMP "*(.gnu.linkonce.t.$line)\n");
    }
    close(ORDER);
}

$skip = 1;
LINE:
while ($line = <LD>) {
    chomp $line;
    if ($skip && $line =~ /^=*$/) {
        $skip = 0;
        next LINE;
    } elsif (!$skip && $line =~ /^=*$/) {
        last LINE;
    }
    if ($skip) {
        next LINE;
    }
    print TMP "$line\n";
    if ($line =~ /^[\s]*.text[\s]*:[\s]*$/) {
        defined($line = <LD>) || die("Premature end of ld input");
        print TMP "$line";
	print TMP "*(.text)\n";
        &PrintOrder();
	<LD>;			# Skip over *(.text)
    }
}

close(LD);
close(TMP);

#
# Everything went OK, so create the real output file.
#
rename($tmpFile, $linkScript) || die("$linkScript: $!");
