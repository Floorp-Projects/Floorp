#!/usr/bin/perl

# Get and log the input
open(LOGFILE, ">/tmp/dumpfile2");
print LOGFILE `date`;

read(STDIN, $buffer, $ENV{'CONTENT_LENGTH'});
print LOGFILE $ENV{'REQUEST_METHOD'};
print LOGFILE $ENV{'CONTENT_LENGTH'};
print LOGFILE $buffer;
close LOGFILE;

# Send the response
print "Content-type: text/xml\n\n";
print "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:SOAP-ENC=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:xsi=\"http://www.w3.org/1999/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/1999/XMLSchema\">\n";
print "<SOAP-ENV:Body>\n";
print "<m:GetLastTradePriceResponse xmlns:m=\"uri:some-namespace\">\n";
print "<SOAP-ENC:Array SOAP-ENC:arrayType=\"xsd:ur-type[5]\">\n";
print "  <foo xsi:type=\"xsd:int\">23</foo>\n";
print "  <SOAP-ENC:boolean>false</SOAP-ENC:boolean>\n";
print "  <bar xsi:type=\"xsd:float\">0.234</bar>\n";
print "  <baz xsi:type=\"SOAP-ENC:Array\" SOAP-ENC:arrayType=\"xsd:short[]\">\n";
print "     <element1>2</element1>\n";
print "     <element2>3</element2>\n";
print "     <foobar>4</foobar>\n";
print "     <ignoredType xsi:type=\"xsd:float\">5</ignoredType>\n";
print "     <SOAP-ENC:int>45</SOAP-ENC:int>\n";
print "  </baz>\n";
print "  <bob>\n";
print "     <inst1>untyped string</inst1>\n";
print "     <inst2 xsi:type=\"xsd:byte\">c</inst2>\n";
print "     <SOAP-ENC:int>456</SOAP-ENC:int>\n";
print "  </bob>\n";
print "</SOAP-ENC:Array>\n";
print "</m:GetLastTradePriceResponse>\n";
print "</SOAP-ENV:Body>\n";
print "</SOAP-ENV:Envelope>\n";
