open INFILE, "<$ARGV[1]";
$build = <INFILE>;
close INFILE;
chop $build;
open OUTFILE, ">$ARGV[0]" || die;

print OUTFILE "/* THIS IS A GENERATED FILE!\n*\n";
print OUTFILE "* See mozilla/config/build_header.pl */\n*\n*/";
print OUTFILE "\n\#define NS_BUILD_ID " . $build . "\n";

close OUTFILE;

