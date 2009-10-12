# add mathml.css to the ua.css

$sep = '/'; # directory separator

$ua = $ARGV[0]; $ua =~ s|\\|$sep|g;

open(UA, $ua) || die "Cannot find ua.css\n";
$css = join("", <UA>);
close(UA);

if (!($css =~ m|\@import.*mathml\.css|)) 
{
  # since bad end of lines cause troubles on some platforms
  # do a little perl magic for dos2{unix or mac} here
  $css =~ s#(\@import[^\@]+\;)(\cM?)(\n\cM?\n)#$1$2\n\@import url\(resource://gre-resources/mathml\.css\);$2$3#;
  open(UA, ">$ua");
  print UA $css;
  close(UA);
}

