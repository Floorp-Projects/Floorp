#!/usr/bin/perl

#################################################################
# Rest of program

# get requirements from the same dir as the script
if ($^O ne "cygwin") {
# we'll be pulling in some stuff from the script directory
require FindBin;
import FindBin;
push @INC, $FindBin::Bin;
}

require GenerateManifest;
import GenerateManifest;
use Getopt::Long;

# Configuration
$win32 = 0;
$darwin = 0;
for ($^O) {
  if (/((MS)?win32)|cygwin/i) {
    $win32 = 1;
  }
  elsif (/darwin/i) {
    $darwin = 1;
  }
}
if ($win32) {
  $moz = "$ENV{'MOZ_SRC'}/mozilla";
  if ($ENV{'MOZ_DEBUG'}) {
    $chrome = "$moz/dist/WIN32_D.OBJ/thunderbird/tmpchrome";
  }
  else
  {
    $chrome = "$moz/dist/WIN32_O.OBJ/thunderbird/tmpchrome";
  }
}
else {
  $moz = "~/mozilla";
  $chrome = "$moz/dist/thunderbird/tmpchrome";
}

$verbose = 0;
$locale = "en-US";
$platform = "en-unix";

GetOptions('verbose!' => \$verbose,
           'mozpath=s' => \$moz,
           'manifest=s' => \$manifest,
	   'jarfile=s' => \$jarfile,
	   'chrome=s' => \$chrome,
	   'locale=s' => \$locale,
           'help' => \$showhelp);

if ($win32) {
  # Fix those pesky backslashes
  $moz =~ s/\\/\//g;
  $chrome =~ s/\\/\//g;
  $platform = "en-win";
}
elsif ($darwin) {
  $platform = "en-mac"
}

if ($showhelp) {
  print STDERR "Thunderbird manifest generator.\n",
               "Usage:\n",
               "  -help            Show this help.\n",
               "  -manifest <file> Specify the input manifest.\n",
               "  -mozpath <path>  Specify the path to Mozilla.\n",
	       "  -jarfile <file>  Specify the name of the resulting jar file.\n",	
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

GenerateManifest($moz, $manifest, $jarfile, $chrome, $locale, $platform, *STDOUT, "/", $verbose);

exit 0;
