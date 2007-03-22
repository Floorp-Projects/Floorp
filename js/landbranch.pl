#! /usr/local/bin/perl5

use File::Path;

# The development branch is where primary development and checkins
# are done on a day-to-day basis.
$development_branch_prefix = "SpiderMonkey140";

# Space-separated list of CVS-controlled directories to tag/merge
$merge_dirs =
    "mozilla/js/src " ;

# When line below uncommented, don't recurse into subdirs
#$recurse_flag = '-l';

#----------------------------------------------------------------------------

# The merge branch is itself a branch off of the development branch
# at a point where the development branch is thought to be stable.
# (A branch is used rather than a static tag because, inevitably,
# the development branch is not quite as stable/buildable as was
# thought.)  The contents of the merge branch will be copied to
# the trunk when merging takes place.


# The following tags are created automatically by this script:
#
# JS_STABLE_DROP
#
#     A static tag on the development branch (or a branch off the
#     development branch) that indicates the code that should be merged
#     into the trunk.  This is a "variable" tag in the sense that it is
#     redefined after each merge.
#
# JS_LAST_STABLE_DROP
#
#     A static tag that is a copy of what the JS_STABLE_DROP tag was in
#     the previous merge cycle.  This is a "variable" tag that is
#     redefined after each merge.  Changes in the branch can be merged
#     to the trunk by using:
#
#         cvs up -jJS_LAST_STABLE_DROP -jJS_STABLE_DROP
#
# JS_LANDING
#
#     A static tag that identifies the code on the trunk after the merge
#     from the branch to the trunk takes place.  This is a "variable"
#     tag that is redefined after each merge.  Changes on the trunk
#     since the last branch landing can be seen by using:
#
#         cvs diff -rJS_LANDING -rHEAD
#
# JS_LANDING_mmddyyyy
#
#     This is a tag on the trunk which may be used for archaeological
#     purposes.  This tag is made from the JS_LANDING tag.


$development_branch = $development_branch_prefix . "_BRANCH";
$development_base = $development_branch_prefix . "_BASE";

sub help {
print <<"END";
$0: A tool for merging stable snapshots of JavaScript from a CVS
development branch onto the trunk

Landing a snapshot of the development branch consists of
the following steps:

  1) Tag all/some files on the branch to identify files to be merged.
  2) Merge files from the branch into the trunk using a temporary
     working directory.
  3) Resolve any conflicts that arise as a result of the merge.
  4) Commit merged changes to the trunk.
  5) Make changes to resolve (build) difficulties and re-commit.
     Repeat as necessary.
  6) Backpropagate changes on the trunk to the development branch.
  
This script will assist with steps #2, #4 and #6:

  $0 -merge JS_STABLE_10131998
  $0 -commit
  $0 -backpatch
  
END
}

sub log {
    local($msg) = @_;
    print LOGFILE $msg if $logfile;
}

# Similar to die built-in
sub die {
    local($msg) = @_;
    &log($msg);
    chomp($msg);
    if ($logfile) {
	$msg .= "\nSee $logfile for details.";
    }
    die "$msg\n";
}

# Similar to system() built-in
sub system {
    local(@args) = @_;
    local($cmd) = join(" ", @args);
    &log("Executing: $cmd\n");

    if ($logfile) {
	$cmd .= " >> $logfile 2>&1";
	close(LOGFILE);
    }

    local($result) = 0xffff & system($cmd);

    if ($logfile) {
	open(LOGFILE, ">>$logfile");
    }

    return unless ($result);
    $msg = "Died while executing $cmd";

    if ($result == 0xff00) {
	&die("$msg\nWhile executExecution failed due to perl error: $!. ");
    } else {
	$result >>= 8;
	&die("$msg\nExecution failed; exit status: $result. ");
    }
}

chomp($root_dir = `pwd`);

# Default log file
$logfile = $root_dir . "/log";

while ($#ARGV >=0) {
    $_ = shift;
 
    if (/-merge/) {
	$do_tag = 1;
	$do_checkout = 1;
	$do_merge = 1;
	$tag = shift;
    } elsif (/-commit/ || /-ci/) {
	$do_commit = 1;
    } elsif (/-backpatch/) {
	$do_backpatch = 1;
    } elsif (/-log/) {
	$logfile = shift;
    } elsif (/-tag/) { # Debugging option
	$do_tag = 1;
	$tag = shift;
    } elsif (/-co/) {  # Debugging option
	$do_checkout = 1;
    } else {
	print STDERR "Illegal option: $_\n" unless (/-h/);
	&help();
	exit(1);
    }
}

die "You must set your CVSROOT environment variable" if !$ENV{"CVSROOT"};

if ($logfile) {
    open(LOGFILE, ">$logfile") || die "Couldn't open log file \"$logfile\"";
    print("Logging to file \"$logfile\".\n");
}

$trunk_dir = $root_dir . "/trunk";

if ($do_tag) {
    if (!$tag) {
	&die("Must specify tag on command-line\n");
    }

    print("Tagging tree with tag JS_STABLE_DROP.\n");
    &system("cvs rtag $recurse_flag -F -r $tag JS_STABLE_DROP $merge_dirs");
}

if ($do_checkout) {

    # Delete trunk subdir if it already exists
    if (-d $trunk_dir) {
	&log("Deleting directory $trunk_dir\n");
	rmtree ($trunk_dir, 0, 1);
    }
    &log("Creating directory $trunk_dir\n");
    mkdir($trunk_dir, 0777) || die "Couldn't create directory $trunk_dir";

    # Check out on trunk
    print("Checking out $merge_dirs.\n");
    chdir $trunk_dir;
    &system("cvs co $recurse_flag -A $merge_dirs");
}

if ($do_merge) {
    chdir $trunk_dir;
    print("Merging from JS_STABLE_DROP into trunk\n");
    &system("cvs up -jJS_LAST_STABLE_DROP -jJS_STABLE_DROP");
}

if ($do_commit) {
    &die("No merged tree found.  Wrong directory ?") if (!chdir $trunk_dir);

    ($_,$_,$_,$day,$mon,$year,$_,$_) = localtime(time());
    if ($year < 30) {
	$year = "20" . $year;
    } else {
	$year = "19" . $year;
    }

    $mmddyyyy = sprintf("%02d%02d%s", $mon, $day, $year);

    print("Checking in code on trunk");
    &system("cvs ci -m 'Stable drop of JavaScript interpreter code from " .
	    "$development_branch'");

    # Tag merged result
    &system("cvs tag -F JS_LANDING");
    &system("cvs tag -F JS_LANDING_$mmddyyyy");

    # Move JS_LAST_STABLE_DROP tag forward
    &system("cvs tag -F -rJS_STABLE_DROP JS_LAST_STABLE_DROP");
}
    
    
