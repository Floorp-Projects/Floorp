open INFILE, "<$ARGV[1]";
$build = <INFILE>;
close INFILE;
chop $build;
open INFILE, "<$ARGV[0]";
open OUTFILE, ">$ARGV[0].old";

while (<INFILE>) {
if (/<TITLE>Version/) {
s/-[0-9]+/-$build/;
}
print OUTFILE $_;
}

close INFILE;
close OUTFILE;

rename "$ARGV[0].old", "$ARGV[0]";
