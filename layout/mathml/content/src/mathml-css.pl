# add mathml.css to the ua.css

$sep = '/'; # directory separator

$ua = $ARGV[0]; $ua =~ s|\\|$sep|g;

open(UA, $ua) || die "Cannot find ua.css\n";
$css = join("", <UA>);
close(UA);

if (!($css =~ m|\@import.*mathml\.css|)) 
{
  $css =~ s/\cM\n/\n/g; # dos2unix end of line
  $css =~ s|(\@import[^\@]+\;)\n\n|$1\n\@import url\(resource:/res/mathml\.css\);\n\n|;
  open(UA, ">$ua");
  print UA $css;
  close(UA);
}

