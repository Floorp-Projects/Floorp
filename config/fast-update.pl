#!/usr/bin/env perl

use Getopt::Long;

my $hours=24;
my $branch="Netscape_20000922_BRANCH";
my $module="SeaMonkeyAll";
my $maxdirs=5;

&GetOptions('h=s' => \$hours, 'm=s' => \$module, 'r=s' => \$branch);

my $url = "http://bonsai.mozilla.org/cvsquery.cgi?module=${module}&branch=${branch}&branchtype=match&sortby=Date&date=hours&hours=${hours}&cvsroot=%2Fcvsroot";

print "Contacting bonsai for updates to the ${branch} branch in the last ${hours} hours..\n";

@args = ("wget", "--quiet", $url);
system(@args) == 0 ||
  die "wget failed: $!\n";

print "Processing checkins..\n";
open CHECKINS, "<cvsroot";

while (<CHECKINS>) {
  if (/js_file_menu\((.*),\s*\'(.*)\'\s*,\s*(.*),\s*(.*),\s*(.*),\s*(.*)\)/) {
    my ($repos, $dir, $file, $rev, $branch, $event) =
      ($1, $2, $3, $4, $5, $6);
    push @dirlist, $dir;
  }
}

my $lastdir = "";
foreach $dir (sort @dirlist) {
  next if ($lastdir eq $dir);
  push @uniquedirs, $dir;
  $lastdir = $dir;
}

print "Updating tree ($#uniquedirs directories)..\n";

my $i=0;
my $dirlist = "";
foreach $dir (sort @uniquedirs) {
  $dirlist .= "$dir ";
  $i++;
  if ($i == 5) {
    
    system "cvs up -l $dirlist\n";
    $dirlist = "";
    $i=0;
  }
}
if ($i < 5) {
  system "cvs up -l $dirlist\n";
}

close CHECKINS;
unlink "cvsroot";
print "done.\n";

