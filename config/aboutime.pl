
use strict;
use Getopt::Std;
require mozBDate;
require "Moz/Milestone.pm";

my $mfile;
getopts('m:');
if (defined($::opt_m)) {
    $mfile = $::opt_m;
}

my $outfile = $ARGV[0];
my $build_num_file = $ARGV[1];
my $infile = "";

$infile = $ARGV[2] if ("$ARGV[2]" ne "");

if (defined($mfile)) {
    my $milestone = &Moz::Milestone::getOfficialMilestone($mfile);
    &mozBDate::SetMilestone($milestone);
}
&mozBDate::SubstituteBuildNumber($outfile, $build_num_file, $infile);

