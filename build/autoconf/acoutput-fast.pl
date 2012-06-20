#! /usr/bin/env perl
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# acoutput-fast.pl - Quickly create makefiles that are in a common format.
#
# Most of the makefiles in mozilla only refer to two configure variables:
#     @srcdir@
#     @top_srcdir@
# However, configure does not know any better and it runs sed on each file
# with over 150 replacement rules (slow as molasses).
#
# This script takes a list of makefiles as input. For example,
#   
#     echo $MAKEFILES | acoutput-fast.pl
#
# The script creates each Makefile that only references @srcdir@ and
# @top_srcdir@. For other files, it lists them in a shell command that is
# printed to stdout:
#
#     CONFIG_FILES="unhandled_files..."; export CONFIG_FILES
#
# This command can be used to have config.status create the unhandled
# files. For example,
#
#     eval "echo $MAKEFILES | acoutput-fast.pl"
#     AC_OUTPUT($MAKEFILES)
#
# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).

#use File::Basename;
sub dirname {
  my $dir = $_[0];
  return '.' if not $dir =~ m%/%;
  $dir =~ s%/[^/][^/]*$%%;
  return $dir;
}

# Create one directory. Assumes it doesn't already exist.
# Will create parent(s) if needed.
sub create_directory {
  my $dir = $_[0];
  my $parent = dirname($dir);
  create_directory($parent) if not -d $parent;
  mkdir "$dir",0777;
}

# Create all the directories at once.
#   This can be much faster than calling mkdir() for each one.
sub create_directories {
  my @makefiles = @_;
  my @dirs = ();
  my $ac_file;
  foreach $ac_file (@makefiles) {
    push @dirs, dirname($ac_file);
  }
  # Call mkdir with the directories sorted by subdir count (how many /'s)
  if (@dirs) {
    foreach $dir (@dirs) {
      if (not -d $dir) {
        print STDERR "Creating directory $dir\n";
        create_directory($dir);
      }
    }
  }
}

while($arg = shift) {
    if ($arg =~ /^--srcdir=/) {
        $ac_given_srcdir = (split /=/, $arg)[1];
    }
    if ($arg =~ /^--cygwin-srcdir/) {
        $ac_cygwin_srcdir = (split /=/, $arg)[1];
    }
}

if (!$ac_given_srcdir) {
  $ac_given_srcdir = $0;
  $ac_given_srcdir =~ s|/?build/autoconf/.*$||;
  $ac_given_srcdir = '.' if $ac_given_srcdir eq '';
}

if (!$ac_cygwin_srcdir) {
    $ac_cygwin_srcdir = $ac_given_srcdir;
}

# Read list of makefiles from the stdin or,
#   from files listed on the command-line.
#
@makefiles=();
push @makefiles, split while (<STDIN>);

# Create all the directories at once.
#   This can be much faster than calling mkdir() for each one.
create_directories(@makefiles);

# Output the makefiles.
#
@unhandled=();
foreach $ac_file (@makefiles) {
  if (not $ac_file =~ /Makefile$/ or $ac_file =~ /:/) {
    push @unhandled, $ac_file;
    next;
  }
  $ac_file_in = "$ac_given_srcdir/$ac_file.in";
  $ac_dir = dirname($ac_file);
  if ($ac_dir eq '.') {
    $ac_dir_suffix = '';
    $ac_dots = '';
  } else {
    $ac_dir_suffix = "/$ac_dir";
    $ac_dir_suffix =~ s%^/\./%/%;
    $ac_dots = $ac_dir_suffix;
    $ac_dots =~ s%/[^/]*%../%g;
  }
  if ($ac_given_srcdir eq '.') {
    $srcdir = '.';
    if ($ac_dots eq '') {
      $top_srcdir = '.'
    } else {
      $top_srcdir = $ac_dots;
      $top_srcdir =~ s%/$%%;
    }
  } elsif ($ac_cygwin_srcdir =~ m%^/% or $ac_cygwin_srcdir =~ m%^.:/%) {
    $srcdir     = "$ac_cygwin_srcdir$ac_dir_suffix";
    $top_srcdir = "$ac_cygwin_srcdir";
  } else {
    $srcdir     = "$ac_dots$ac_cygwin_srcdir$ac_dir_suffix";
    $top_srcdir = "$ac_dots$ac_cygwin_srcdir";
  }

  if (-e $ac_file) {
    next if -M _ < -M $ac_file_in;
    print STDERR "updating $ac_file\n";
  } else {
    print STDERR "creating $ac_file\n";
  }

  open (INFILE, "<$ac_file_in")
    or ( die "can't read $ac_file_in: No such file or directory\n");
  open (OUTFILE, ">$ac_file")
    or ( warn "Unable to create $ac_file\n" and next);

  while (<INFILE>) {
    #if (/\@[_a-zA-Z]*\@.*\@[_a-zA-Z]*\@/) {
    #  warn "Two defines on a line:$ac_file:$.:$_";
    #  push @unhandled, $ac_file;
    #  last;
    #}

    s/\@srcdir\@/$srcdir/g;
    s/\@top_srcdir\@/$top_srcdir/g;

    if (/\@[_a-zA-Z]*\@/) {
      warn "Unknown variable:$ac_file:$.:$_";
      push @unhandled, $ac_file;
      last;
    }
    print OUTFILE;
  }
  close INFILE;
  close OUTFILE;
}

# Print the shell command to be evaluated by configure.
#
print "CONFIG_FILES=\"".join(' ', @unhandled)."\"; export CONFIG_FILES\n";

