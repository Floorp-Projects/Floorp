#!/usr/bin/env perl
#
# cvsco-fast-update.pl cvs co ... 
#
# This command parses a "cvs co ..." command and converts it to 
# fast-update.pl commands
#
use Getopt::Long;

my $filename = ".fast-update";
my $start_time = time();

my $branch;
my @modules;
my @dirs;

print "$0: (".join(')(',@ARGV).")\n";
while (scalar(@ARGV)) {
  my $val = shift(@ARGV);
  if (   ($val eq '-A') || ($val eq 'co') || ($val eq 'cvs')
      || ($val eq '-P') || ($val eq '-q')) {
    #print "ignore $val\n";
    next;
  }
  elsif (($val eq '-d') || ($val eq '-q') || ($val eq '-z')) {
    my $tmp = shift @ARGV;
    #print "ignore $val $tmp\n";
    next;
  }
  elsif ($val eq '-r') {
    $branch = shift @ARGV;
    #print "branch = $branch\n";
    next;
  }
  elsif ($val =~ /^-/) {
    print "*** unknown switch: $val\n";
    exit 1;
  }

  if ($val =~ /\//) {
    push @dirs, $val;
    #print "dir = $val\n";
  }
  else {
    push @modules, $val;
    #print "module = $val\n";
  }
}

#print "dir = (".join(')(', @dirs)."), "
#      . "module = (".join(')(', @modules)."), "
#      . "branch = ($branch)\n";

if (!$branch) {
  $branch = 'HEAD';
}

my $status = 0;
foreach my $mod (@modules) {
  my $cmd = "config/fast-update.pl -r $branch -m $mod";
  #print "system \"$cmd\"\n";
  $status |= system $cmd;
}
foreach my $d (@dirs) {
  my $cmd = "config/fast-update.pl -r $branch -d $d -m all";
  #print "system \"$cmd\"\n";
  $status |= system $cmd;
}

exit $status;



