#!/usr/bin/env perl
#
# fast-update.pl [-h hours] [-m module] [-r branch]
#
# This command, fast-update.pl, does a (fast) cvs update of the current 
# directory. It is fast because the cvs up command is only run on those 
# directories / sub-directories where changes have occured since the 
# last fast-update.
#
# The last update time is stored in a ".fast-update" file in the current
# directory. Thus one can choose to only fast-update a branch of the tree 
# and then fast-update the whole tree later.
#
# The first time this command is run in a directory the last cvs update
# time is assumed to be the timestamp of the CVS/Entries file.
#
use Getopt::Long;

my $filename = ".fast-update";
my $start_time = time();

my $branch;
my $module="SeaMonkeyAll";
my $maxdirs=5;
my $rootdir = "";
my $hours = 0;
my $dir = '';

&GetOptions('d=s' => \$dir, 'h=s' => \$hours, 'm=s' => \$module, 'r=s' => \$branch);

#print "dir = ($dir), hours = ($hours), module = ($module), branch = ($branch)\n";
if ($dir) {
  chdir '..';
  chdir $dir;
}

if (!$hours) {
  $hours = get_hours_since_last_update();
}
if (!$hours) {
  $hours = 24;
}


# pull out the current directory
# if there is no such file, this will all just fail, which is ok
open REPOSITORY, "<CVS/Repository";
$rootdir = <REPOSITORY>;
chop $rootdir;
close REPOSITORY;

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

# check for a static Tag
# (at least that is what I think this does)
# (bonsai does not report changes when the Tag starts with 'N')
# (I do not really understand all this)
if ($branch) {
  open TAG, "<CVS/Tag";
  my $line = <TAG>;
  if ($line =~ /^N/) { 
    print "static tag, ignore branch\n";
    $branch = '';
  }
  close TAG;
}


my $url = "http://bonsai.mozilla.org/cvsquery.cgi?module=${module}&branch=${branch}&branchtype=match&sortby=File&date=hours&hours=${hours}&cvsroot=%2Fcvsroot";

my $esc_dir = escape($dir);
if ($dir) {
  $url .= "&dir=$esc_dir";
}

print "Contacting bonsai for updates to ${module} ";
print "on the ${branch} branch " if ($branch);
print "in the last ${hours} hours ";
print "within the $rootdir directory..\n" if ($rootdir);
#print "url = $url\n";

# first try wget, then try lynx

# this is my lame way of checking if a command succeeded AND getting
# output from it. I'd love a better way. -alecf@netscape.com
my $have_checkins = 0;
@args = ("wget", "--quiet", "--output-document=-", "\"$url\"");
open CHECKINS,"wget --quiet --output-document=- \"$url\"|" or
  die "Error opening wget: $!\n";

$header = <CHECKINS> and $have_checkins=1;

if (!$have_checkins) {

  open CHECKINS, "lynx -source '$url'|" or die "Error opening lynx: $!\n";

  $header = <CHECKINS> and $have_checkins = 1;
}

$have_checkins || die "Couldn't get checkins\n";

open REALOUT, ">.fast-update.bonsai.html" || die "argh $!\n";
print "Processing checkins...";
while (<CHECKINS>) {
  print REALOUT $_;
  
  if (/js_file_menu\((.*),\s*\'(.*)\'\s*,\s*(.*),\s*(.*),\s*(.*),\s*(.*)\)/) {
    my ($repos, $dir, $file, $rev, $branch, $event) =
      ($1, $2, $3, $4, $5, $6);
    push @dirlist, $dir;
  }
}

print "done.\n";
close REALOUT;
unlink '.fast-update.bonsai.html';

my $lastdir = "";
my @uniquedirs;

foreach $dir (sort @dirlist) {
  next if ($lastdir eq $dir);

  my $strippeddir = "";
  $lastdir = $dir;

  # now strip out $rootdir
  if ($rootdir) {

    # only deal with directories that start with $rootdir
    if (substr($dir, 0, (length $rootdir)) eq $rootdir) {

      if ($dir eq $rootdir) {
        $strippeddir = ".";
      } else {
        $strippeddir = substr($dir,(length $rootdir) + 1 );
      }

    }
  } else {
    $strippeddir = $dir;
  }

  if ($strippeddir) {
    push @uniquedirs, $strippeddir;
  }
}

my $status = 0;
if (scalar(@uniquedirs)) {
  print "Updating tree..($#uniquedirs directories)\n";
  my $i=0;
  my $dirlist = "";
  foreach $dir (sort @uniquedirs) {
    if (!-d $dir) {
      cvs_up_parent($dir);
    }
    $dirlist .= "\"$dir\" ";
    $i++;
    if ($i == 5) {
      $status |= spawn("cvs up -l -d $dirlist\n");
      $dirlist = "";
      $i=0;
    }
  }
  if ($i) {
    $status |= spawn("cvs up -l -d $dirlist\n");
  }
}
else {
  print "No directories to update.\n";
}

close CHECKINS;
if ($status == 0) {
  set_last_update_time($filename, $start_time);
  print "successfully updated $module/$dir\n";
}
else {
  print "error while updating $module/$dir\n";
}

exit $status;

sub cvs_up_parent {
  my ($dir) = @_;
  my $pdir = $dir;
  $pdir =~ s|/*[^/]*/*$||;
  #$pdir =~ s|/$||;
  #$pdir =~ s|[^/]*$||;
  #$pdir =~ s|/$||;
  if (!$pdir) {
    $pdir = '.';
  }
  if (!-d $pdir) {
    cvs_up_parent($pdir);
  }
  $status |= system "cvs up -d -l $pdir\n";
}

sub get_hours_since_last_update {
  # get the last time this command was run
  my $last_time = get_last_update_time($filename);
  if (!defined($last_time)) {
    #
    # This must be the first use of fast-update.pl so use the timestamp 
    # of a file that: 
    #  1) is managed by cvs
    #  2) the user should not be tampering with
    #  3) that gets updated fairly frequently.
    #
    $last_time = (stat "CVS/Entries")[9];
    if (defined($last_time)) {
      $last_time -= 3600*24; # for safety go back a bit
      print "use fallback time of ".localtime($last_time)."\n";
    }
  }
  if(!defined($last_time)) {
    print "last_time not defined\n";
  }

  # figure the hours (rounded up) since the last fast-update
  my $hours = int(($start_time - $last_time + 3600)/3600);
  print "last updated $hours hour(s) ago at ".localtime($last_time)."\n";
  return $hours;
}

# returns time of last update if known
sub get_last_update_time {
  my ($filename) = @_;
  if (!-r $filename) {
    return undef;
  }
  open FILE, "<$filename";
  my $line = <FILE>;
  if (!defined(line)) {
    return undef;
  }
#  print "line = $line";
  $line =~ /^(\d+):/;
  return $1;
}

sub set_last_update_time {
  my ($filename, $time) = @_;
  my $time_str = localtime($time);
  open FILE, ">$filename";
  print FILE "$time: last fast-update.pl at ".localtime($time)."\n";
}

# URL-encode data
sub escape {
  my ($toencode) = @_;
  $toencode=~s/([^a-zA-Z0-9_.-])/uc sprintf("%%%02x",ord($1))/eg;
  return $toencode;
}

sub spawn {
  my ($procname) = @_;
  return system "$procname";
}
