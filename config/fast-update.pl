#!/usr/bin/env perl

use Getopt::Long;

my $hours=24;
my $branch;
my $module="SeaMonkeyAll";
my $maxdirs=5;
my $rootdir = "";
my $tempfile;

# pull out the current directory
# if there is no such file, this will all just fail, which is ok
open REPOSITORY, "<CVS/Repository";
$rootdir = <REPOSITORY>;
chop $rootdir;
close REPOSITORY;
print "Current dir = $rootdir\n";

&GetOptions('h=s' => \$hours, 'm=s' => \$module, 'r=s' => \$branch);

# try to guess the current branch by looking at all the
# files in CVS/Entries
if (!$branch) {
  my $foundbranch =0;
  
  open ENTRIES, "<CVS/Entries";
  while (<ENTRIES>) {
    chop;
    @entry = split(/\//);
    my ($type, $file, $ver, $date, $unknown, $tag) = @entry;
    
    # the tag usually starts with "T"
    $thisbranch = substr($tag, 1);
    
    # look for more than one branch
    if ($type eq "") {
      
      if ($foundbranch and ($lastbranch ne $thisbranch)) {
        die "Multiple branches in this directory, cannot determine branch\n";
      }
      $foundbranch = 1;
      $lastbranch = $thisbranch;
    }
    
  }
  
  $branch = $lastbranch if ($foundbranch);

  close ENTRIES;
}


my $url = "http://bonsai.mozilla.org/cvsquery.cgi?module=${module}&branch=${branch}&branchtype=match&dir=${rootdir}&sortby=File&date=hours&hours=${hours}&cvsroot=%2Fcvsroot";

print "Contacting bonsai for updates to ${module} on the ${branch} branch in the last ${hours} hours, within the $rootdir directory..\n";

# first try wget, then try lynx

# this is my lame way of checking if a command succeeded AND getting
# output from it. I'd love a better way. -alecf@netscape.com
my $have_checkins = 0;
@args = ("wget", "--quiet", "--output-document=-", $url);
open CHECKINS,"wget --quiet --output-document=- '$url'|" or
  die "Error opening wget: $!\n";

$header = <CHECKINS> and $have_checkins=1;

if (!$have_checkins) {

  open CHECKINS, "lynx -source '$url'|" or die "Error opening lynx: $!\n";

  $header = <CHECKINS> and $have_checkins = 1;
}

$have_checkins || die "Couldn't get checkins\n";

open REALOUT, ">foo" || die "argh $!\n";
print "Processing checkins..\n";
while (<CHECKINS>) {
  print REALOUT $_;
  
  if (/js_file_menu\((.*),\s*\'(.*)\'\s*,\s*(.*),\s*(.*),\s*(.*),\s*(.*)\)/) {
    my ($repos, $dir, $file, $rev, $branch, $event) =
      ($1, $2, $3, $4, $5, $6);
    push @dirlist, $dir;
  }
}

close REALOUT;

my $lastdir = "";
my @uniquedirs;

foreach $dir (sort @dirlist) {
  next if ($lastdir eq $dir);
  $lastdir = $dir;

  # now strip out $rootdir
  if ($rootdir) {
    if ($dir eq $rootdir) {
      $strippeddir = ".";
    } else {
      $strippeddir = substr($dir, (length $rootdir) + 1 );
    }
  } else {
    $strippeddir = $dir;
  }

  push @uniquedirs, $strippeddir;
}

if ($#uniquedirs < 0) {
  print "No directories to update.\n";
  exit 0;
}

print "Updating tree..\n";

my $i=0;
my $dirlist = "";
foreach $dir (sort @uniquedirs) {
  $dirlist .= "$dir ";
  $i++;
  if ($i == 5) {
    print "$dirlist:\n";
    system "cvs up -l $dirlist\n";
    $dirlist = "";
    $i=0;
  }
}
if ($i < 5) {
  system "cvs up -l -d $dirlist\n";
}

close CHECKINS;

if ($tempfile) {
  print "removing $tempfile\n";
  
  unlink $tempfile;
}

print "done.\n";

