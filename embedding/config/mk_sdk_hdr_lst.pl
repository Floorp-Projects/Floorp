#!/bin/perl

# This script dumps out a list of all public Mozilla headers based on what the
# embedding samples use. It uses dependency generator tools to find all
# headers, sorts them removes duplicates and dumps them out to display.


# Configuration
$moz = "$ENV{'MOZ_SRC'}/mozilla";
$makedepexe = "$moz/config/makedep.exe";

# List of Mozilla include directories
@incdirs = (
  "$moz/dist/include/nspr",
  "$moz/dist/include",
);

# List of embedding sample/wrapper app directories
%embeddirs = (
  "winEmbed", "$moz/embedding/tests/winEmbed",
  "mfcEmbed", "$moz/embedding/tests/mfcEmbed",
  "gtkEmbed", "$moz/embedding/tests/gtkEmbed",
  "activex", "$moz/embedding/browser/activex/src/control",
  "powerplant", "$moz/embedding/browser/powerplant/source",
  "gtk", "$moz/embedding/browser/gtk/src"
);

@deps = ();

# Analyze each embedding project in turn
while (($prjname,$prjpath) = each %embeddirs) {
  makedep($prjname, $prjpath);
}

# Remove duplicate dependencies & sort alphabetically
$prev = 'nonesuch';
@out = grep($_ ne $prev && ($prev = $_), sort @deps);

# Output the list of headers
foreach $h (@out) {
	$printinfo{$h} = 0;
}
foreach $i (@incdirs) {
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
  my($prjname, $prjpath) = @_;

  print STDERR "Analyzing dependencies for $prjname ...\n";

  chdir $prjpath
    or die "Cannot change directory to \"$prjpath\"";

  # Search for .c, .cpp and .h files
  opendir(THISDIR, ".")
    or die "Cannot open directory \"$prjpath\"";
  @srcfiles = grep(/\.(cpp|c|h)$/i, readdir(THISDIR));
  closedir(THISDIR);

  # Construct the arguments for the dependency tool
  @args = ($makedepexe);
  foreach $inc (@incdirs) {
    push(@args, "-I$inc");
  }
  foreach $src (@srcfiles) {
    push(@args, $src);
  }
  $cmd = join(' ', @args);

  # Run the dependency tool and read the output
  open(DATA, "$cmd |")
    or die("Cannot open output from dependency tool");

  # Filter out all lines not containing ".h"
  while (<DATA>) {
    foreach $inc (@incdirs) {
      if (/\.h/i) {
        # Remove whitespace and trailing backslash, newline
        chomp;
        s/^\s+//g;
        s/\s+\\$//g;
        # Store in list
        push @deps, $_;
      }
    }
  }
}



