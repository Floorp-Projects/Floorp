#!/usr/bin/perl

%embed_files = ();

#################################################################
# Rest of program

use Getopt::Long;
use File::stat;

# Configuration
$win32 = ($^O eq "MSWin32") ? 1 : 0; # ActiveState Perl
if ($win32) {
  $moz = "$ENV{'MOZ_SRC'}/mozilla";
  if ($ENV{'MOZ_DEBUG'}) {
    $chrome = "$moz/dist/WIN32_D.OBJ/Embed/tmpchrome";
  }
  else
  {
    $chrome = "$moz/dist/WIN32_O.OBJ/Embed/tmpchrome";
  }
}
else {
  $moz = "~/mozilla";
  $chrome = "$moz/dist/Embed/tmpchrome";
}

$verbose = 0;
$locale = "en-US";

GetOptions('verbose!' => \$verbose,
           'mozpath=s' => \$moz,
           'manifest=s' => \$manifest,
	   'chrome=s' => \$chrome,
	   'locale=s' => \$locale,
           'help' => \$showhelp);

if ($win32) {
  # Fix those pesky backslashes
  $moz =~ s/\\/\//g;
  $chrome =~ s/\\/\//g;
}

if ($showhelp) {
  print STDERR "Embedding manifest generator.\n",
               "Usage:\n",
               "  -help            Show this help.\n",
               "  -manifest <file> Specify the input manifest.\n",
               "  -mozpath <path>  Specify the path to Mozilla.\n",
               "  -chrome <path>   Specify the path to the chrome.\n",
               "  -locale <value>  Specify the locale to use. (e.g. en-US)\n",
               "  -verbose         Print extra information\n";
  exit 1;
}

if ($manifest eq "") {
  die("Error: No input manifest file was specified.\n");
}

if ($verbose) {
  print STDERR "Mozilla path  = \"$moz\"\n",
               "Chrome path   = \"$chrome\"\n",
               "Manifest file = \"$manifest\"\n";
}

parse_input_manifest();
dump_output_manifest();

exit 0;

sub parse_input_manifest() {

  print STDERR "Parsing \"$manifest\"\n" unless !$verbose;

  open(MANIFEST, "<$manifest") or die ("Error: Cannot open manifest \"$manifest\".\n");
  while(<MANIFEST>) {
    chomp;
    s/^\s+//;
    s/\s+$//;

    # Skip comments & blank lines
    next if (/^\#/);
    next if (/^\s*$/);

    # Read key & data
    ($key, $value) = split(/,/, $_);
    
    # Strip out any remaining whitespace from key & value
    for ($key) {
        s/\s+$//;
    }
    for ($value) {
        s/^\s+//;
    }

    $embed_files{$key} = $value;
  }
}

sub dump_output_manifest() {
  print "embed.jar:\n";
  while (($key, $value) = each %embed_files) {

    $key =~ s/XXXX/$locale/g;
    $value =~ s/XXXX/$locale/g;

    # Run ls on the dir/file to ensure it's there and to get a file list back
    $ls_path = "$chrome/$value";
    $is_dir = (-d $ls_path) ? 1 : 0;
    $is_file = (-f $ls_path) ? 1 : 0;

    print STDERR "Listing \"$ls_path\"\n" unless !$verbose;

    if (!$is_dir && !$is_file) {
      print STDERR "Warning: File or directory \"$ls_path\" does not exist.\n";
      next;
    }

    if (!open(FILES, "ls -1 $ls_path |")) {
      print STDERR "Warning: Cannot run ls on contents of \"$ls_path\".\n";
      next;
    }
    while (<FILES>) {
      chomp;
      if ($is_dir) {
	$chrome_file = "$key/$_";
	$real_file = "$value/$_";
      }
      else {
	$chrome_file = $key;
	$real_file = $value;
      }
      # Ignore directories which are returned by ls
      if (! -d "$chrome/$real_file") {
	print "  $chrome_file   ($real_file)\n";
      }
    }
  }
}
