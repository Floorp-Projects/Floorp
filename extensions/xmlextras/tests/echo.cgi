#!/usr/bin/perl

print "Content-type: text/xml\n\n";
print "<?xml version=\"1.0\"?>\n";

if ($ENV{'REQUEST_METHOD'} eq 'POST') 
{
  # Get the input
  read(STDIN, $buffer, $ENV{'CONTENT_LENGTH'});
  print $buffer;
}
else
{
  print "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
  print "<body><p>Expected a POST</p></body></html>\n";	
}
