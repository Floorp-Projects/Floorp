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
print "  <SOAP-ENV:Fault>\n";
print "    <faultcode>SOAP-ENV:Server</faultcode>\n";
print "    <faultstring>Server Error</faultstring>\n";
print "    <detail>\n";
print "      <e:myfaultdetails xmlns:e=\"Some-URI\">\n";
print "      <message>\n";
print "        My application didn't work\n";
print "      </message>\n";
print "      <errorcode>\n";
print "       1001\n";
print "      </errorcode>\n";
print "      </e:myfaultdetails>\n";
print "    </detail>\n";
print "  </SOAP-ENV:Fault>\n";
print "</SOAP-ENV:Body>\n";
print "</SOAP-ENV:Envelope>\n";
