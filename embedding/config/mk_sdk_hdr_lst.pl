#!/usr/bin/perl

# This script dumps out a list of all public Mozilla headers based on what the
# embedding samples use. It uses dependency generator tools to find all
# headers, sorts them removes duplicates and dumps them out to display.

use Getopt::Long;

$showhelp = 0;
$verbose = 0;
$nodefaults = 0;
$appname = "Gecko public header list generator";

# Configuration
$win32 = ($^O eq "MSWin32") ? 1 : 0; # ActiveState Perl
if ($win32) {
  $moz = "$ENV{'MOZ_SRC'}/mozilla";
  $makedepexe = "$moz/config/makedep.exe";
}
else {
  $moz = "/usr/local/src/mozilla";
  $makedepexe = "makedepend";
}

GetOptions('verbose!' => \$verbose,
	   'nodefaults' => \$nodefaults,
           'mozpath=s' => \$moz,
	   'deptool=s' => \$makedepexe,
	   'include=s' => \@cmd_incs,
	   'dir=s' => \@cmd_dirs,
	   'help' => \$showhelp);

if ($showhelp) {
  print STDERR "$appname\n",
               "Usage:\n",
               "--help            Show this help\n",
               "--verbose         Print out more information\n",
               "--nodefaults      Don't use the default include and directory settings\n",
               "--mozsrc  <path>  Specify the path to Mozilla source code (e.g. /usr/src/mozilla)\n",
               "--deptool <exe>   Specify the dependency tool path\n",
               "--include <path>  Add an include path\n",
               "--dir     <path>  Add a directory to be inspected\n";
  exit 1;
}

print STDERR "$appname\n",
             "Path to mozilla is \"$moz\"\n",
             "Dependency tool is \"$makedepexe\"\n" unless !$verbose;

# List of default include directories
@default_incs = (
  "$moz/dist/include/nspr",
  "$moz/dist/include",
);

# List of default directories to analyze
@default_dirs = (
  "$moz/embedding/tests/winEmbed",
  "$moz/embedding/tests/mfcembed",
  "$moz/embedding/browser/gtk/tests",
  "$moz/embedding/browser/activex/src/control",
  "$moz/embedding/browser/powerplant/source",
  "$moz/embedding/browser/gtk/src"
);

@deps = ();

if ($nodefaults) {
  @incs = ( @cmd_incs );
  @dirs = ( @cmd_dirs );
}
else {
  @incs = ( @default_incs, @cmd_incs );
  @dirs = ( @default_dirs, @cmd_dirs );
}

if ($verbose) {
  print "Include paths:\n";
  foreach $inc (@incs) {
    print "  ", $inc, "\n";
  }
}

# Analyze each embedding project in turn
foreach $dir (@dirs) {
  makedep($dir);
}

# Remove duplicate dependencies & sort alphabetically
$prev = 'nonesuch';
@out = grep($_ ne $prev && ($prev = $_), sort @deps);

# Output the list of headers
foreach $h (@out) {
  $printinfo{$h} = 0;
}
foreach $i (@incs) {
  $i =~ s/\\/\//g; # Back slash to forward slash
  foreach $h (@out) {
    # Compare lowercase portion of header to include dir
    $h =~ s/\\/\//g;
    $lci = lc($i);
    $lch = lc(substr($h, 0, length($i)));
    if ($lch eq $lci && $printinfo{$h} == 0) {
      print $h, "\n";
      $printinfo{$h} = 1; # Stops same header from matching two include dirs
    }
  }
}

sub makedep {
  my($dir) = @_;

  print STDERR "Analyzing dependencies for \"$dir\" ...\n" unless !$verbose;

  chdir $dir
    or die "Cannot change directory to \"$dir\"";

  # Search for .c, .cpp and .h files
  opendir(THISDIR, ".")
    or die "Cannot open directory \"$dir\"";
  @srcfiles = grep(/\.(cpp|c|h)$/i, readdir(THISDIR));
  closedir(THISDIR);

  # Construct the arguments for the dependency tool
  if ($win32) {
    @args = ($makedepexe);
    foreach $inc (@incs) {
      push(@args, "-I$inc");
    }
    foreach $src (@srcfiles) {
      push(@args, $src);
    }
  }
  else {
    @args = ($makedepexe);
    push(@args, "-f-"); # To stdout
    push(@args, "-w1"); # width = 1 forces one dependency per line
    foreach $inc (@incs) {
      push(@args, "-I$inc");
    }
    foreach $src (@srcfiles) {
      push(@args, $src);
    }
  }

  $cmd = join(' ', @args);

  if (!$win32) {
    open(OLDERR, ">&STDERR");
    open(STDERR, ">/dev/null");
  }

  # Run the dependency tool and read the output
  open(DATA, "$cmd |")
    or die("Cannot open output from dependency tool");

  # Filter out all lines not containing ".h"
  while (<DATA>) {
    foreach $inc (@incs) {
      if (/\.h/i) {
        # Remove whitespace and trailing backslash, newline
        chomp;
        s/^\s+//g;
	s/^[\w]+\.\w\:\s+//g;
        s/\s+\\$//g;
        # Store in list
        push @deps, $_;
      }
    }
  }

  if (!$win32) {
    close(STDERR);
    open(STDERR, ">&OLDERR");
  }
}



