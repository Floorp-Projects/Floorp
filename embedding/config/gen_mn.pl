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
               "  -locale <value>  Specify the chrome locale to use.\n";
  exit 1;
}


%embed_files = (
  "content/global/", "$chrome/toolkit/content/global/",
  "skin/clasic/global/", "$chrome/classic/skin/classic/global/",
  "skin/classic/communicator/contents.rdf", "$chrome/classic/skin/classic/communicator/contents.rdf",
#  "skin/classic/communicator/dialogOverlay.css", "$chrome/classic/skin/classic/communicator/dialogOverlay.css",
#  "skin/classic/communicator/smallheader-bg.gif", "$chrome/classic/skin/classic/communicator/smallheader-bg.gif",
  # Locale stuff
  "locale/$locale/global/", "$chrome/$locale/local/$locale/global/",
  "locale/$locale/necko/contents.rdf", "$chrome/$locale/locale/$locale/necko/contents.rdf",
  "locale/$locale/necko/necko.properties", "$chrome/$locale/locale/$locale/necko/necko.properties",
  "locale/$locale/necko/redirect_loop.dtd", "$chrome/$locale/locale/$locale/necko/redirect_loop.dtd",
  "locale/$locale/communicator/contents.rdf", "$chrome/$locale/locale/$locale/communicator/contents.rdf",
  "locale/$locale/communicator/security.properties", "$chrome/$locale/locale/$locale/communicator/security.properties",
  # Help stuff
#  "content/help/", "help/content/help/",
#  "skin/classic/communicator/help.css", "classic/skin/classic/communicator/help.css",
);

print "embed.jar:\n";
while (($key, $value) = each %embed_files) {
  open (FILES, "ls -1 $value 2>/dev/null |") or die("Cannot list \"$value\"\n");
  while (<FILES>) {
    chomp;
      
      
    if (substr($value, $#value) eq '/') {
      $chrome_file = "$key$_";
      $real_file = "$value$_";
    }
    else {
      $chrome_file = $key;
      $real_file = $value;
    }

    if (! -d $real_file) {
      $real_file =~ s/$chrome\///g;
      print "  $chrome_file   ($real_file)\n";
    }
  }
}

