# add mathml.css to the ua.css

$sep = '/'; # directory separator

$ua = $ARGV[0]; $ua =~ s|\\|$sep|g;

open(UA, $ua) || die "Cannot find ua.css\n";
$css = join("", <UA>);
close(UA);

if (!($css =~ m|\@import.*mathml\.css|)) 
{
  open(UA, "+>>$ua");
  print UA "\@import url(resource:/res/mathml.css);\n";
  close(UA);
}
