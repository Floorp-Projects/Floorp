open INFILE, "<$ARGV[1]";
$build = <INFILE>;
close INFILE;
chop $build;
open INFILE, "<$ARGV[0]" || die;
open OUTFILE, ">$ARGV[0].old" || die;

while (<INFILE>) {

$id = $_;
   if ($id =~ "Build ID:") {
     print OUTFILE "Build ID: " . $build . "\n";
   }
   else {
      print OUTFILE $_;
   }
}

close INFILE;
close OUTFILE;

rename "$ARGV[0].old", "$ARGV[0]";
