#!/usr/bin/perl

use Getopt::Long;
use File::stat;

# Configuration
$win32 = ($^O eq "MSWin32") ? 1 : 0; # ActiveState Perl
if ($win32) {
  print ("This may not work on Win32!\n");
  $moz = "$ENV{'MOZ_SRC'}/mozilla";
}
else {
  $moz = "~/mozilla";
}

$chrome = "$moz/dist/Embed/tmpchrome";
$locale = "en-US";

GetOptions('mozpath=s' => \$moz,
	   'chrome=s' => \$chrome,
	   'locale=s' => \$locale,
           'help' => \$showhelp);

if ($showhelp) {
  print STDERR "Embedding manifest generator\n",
               "Usage:\n",
               "  -help            Show this help.\n",
               "  -mozpath <path>  Specify the path to Mozilla.\n",
               "  -chrome <path>   Specify the path to the chrome.\n",
               "  -locale <value>  Specify the locale to use. (e.g. en-US)\n";
  exit 1;
}

%embed_files = (
  "content/global/", "toolkit/content/global/",
  "skin/clasic/global/", "classic/skin/classic/global/",
  "skin/classic/communicator/contents.rdf", "classic/skin/classic/communicator/contents.rdf",
  "skin/classic/communicator/dialogOverlay.css", "classic/skin/classic/communicator/dialogOverlay.css",
  "skin/classic/communicator/smallheader-bg.gif", "classic/skin/classic/communicator/smallheader-bg.gif",
  # Locale stuff
  "locale/$locale/global/", "$locale/local/$locale/global/",
  "locale/$locale/necko/contents.rdf", "$locale/locale/$locale/necko/contents.rdf",
  "locale/$locale/necko/necko.properties", "$locale/locale/$locale/necko/necko.properties",
  "locale/$locale/necko/redirect_loop.dtd", "$locale/locale/$locale/necko/redirect_loop.dtd",
  "locale/$locale/communicator/contents.rdf", "$locale/locale/$locale/communicator/contents.rdf",
  "locale/$locale/communicator/security.properties", "$locale/locale/$locale/communicator/security.properties",
  "locale/$locale/navigator-platform/", "en-unix/locale/$locale/navigator-platform/",
  "locale/$locale/global-platform/", "en-unix/locale/$locale/global-platform/",
  # Help stuff
  "content/help/", "help/content/help/",
  "skin/classic/communicator/help.css", "classic/skin/classic/communicator/help.css",
);

dump_manifest();

sub dump_manifest() {
  print "embed.jar:\n";
  while (($key, $value) = each %embed_files) {
    # Run ls on the dir/file to ensure it's there and to get a file list back
    open (FILES, "ls -1 $chrome/$value 2>/dev/null |") or die("Cannot list \"$value\"\n");
    while (<FILES>) {
      chomp;
      # If the value ends in a '/' it's a whole dir
      if (substr($value, $#value) eq '/') {
	$chrome_file = "$key$_";
	$real_file = "$value$_";
      }
      else {
	$chrome_file = $key;
	$real_file = $value;
      }
      # Ignore directories which are returned
      if (! -d "$chrome/$real_file") {
	print "  $chrome_file   ($real_file)\n";
      }
    }
  }
}

