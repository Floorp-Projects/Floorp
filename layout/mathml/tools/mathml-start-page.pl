# Set a custom start page for MathML-enabled builds
#
# Usage:
# perl mathml-start-page.pl "INSERT-PATH-TO/locale/navigator.properties"
#
# e.g,
# perl mathml-start-page.pl "INSERT-PATH-TO/bin/chrome/locales/en-US/navigator/locale/navigator.properties"


# Default start page for MathML-enabled builds
$mathml_start_page = 'http://www.mozilla.org/projects/mathml/start.xml';

$sep = '/'; # directory separator

$navigator_properties_file = $ARGV[0];
$navigator_properties_file =~ s|\\|$sep|g;

$done = 0;
if (open(FILE, $navigator_properties_file))
{
  @LINES = <FILE>;
  close(FILE);
  for ($i = 0; $i <= $#LINES; ++$i) 
  {
    $done = ($LINES[$i] =~ s|^browser\.startup\.homepage=.*|browser\.startup\.homepage=$mathml_start_page|);
    if ($done)
    {
      open(FILE, ">$navigator_properties_file");
      print FILE @LINES;
      close(FILE);
      last;
    }
  }
}

# Feedback if the start page was set, or was already there
if ($done) 
{
  print "Default homepage set to the MathML start page:\n$mathml_start_page\n\n";
}
else {
  print "Couldn't set the default homepage to the MathML start page.\n";
}
