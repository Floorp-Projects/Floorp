open INFILE, "<$ARGV[1]";
$build = <INFILE>;
close INFILE;
chop $build;
open INFILE, "<$ARGV[0]" || die;
open OUTFILE, ">$ARGV[0].old" || die;

while (<INFILE>) {

$id = $_;
   if ($id =~ "Build ID:") {
     $temp = "Build ID: " . $build;
     $id =~ s/Build ID:\s\d+/$temp/;
     print OUTFILE $id;
   }
   elsif ($id =~ "NS_BUILD_ID") {
     $temp = "NS_BUILD_ID " . $build;
     $id =~ s/NS_BUILD_ID\s\d+/$temp/;
     print OUTFILE $id;
   }
   else {
      print OUTFILE $_;
   }
}

close INFILE;
close OUTFILE;

unlink $ARGV[0];
rename "$ARGV[0].old", "$ARGV[0]";
