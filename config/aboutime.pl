
use strict;
use mozBDate;

my $outfile = $ARGV[0];
my $build_num_file = $ARGV[1];
my $infile = "";

$infile = $ARGV[2] if ("$ARGV[2]" ne "");
    
&mozBDate::SubstituteBuildNumber($outfile, $build_num_file, $infile);

