#! /tools/ns/bin/perl5.004

# The contents of this file are subject to the Netscape Public License 
# Version 1.0 (the "NPL"); you may not use this file except in 
# compliance with the NPL.  You may obtain a copy of the NPL at 
# http://www.mozilla.org/NPL/ 
# 
# Software distributed under the NPL is distributed on an "AS IS" basis, 
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
# for the specific language governing rights and limitations under the 
# NPL. 
# 
# The Initial Developer of this code under the NPL is Netscape 
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
# Reserved. 
  
######################################################################
#
# This is a simple HTTP command-line getter program. You simply give
# it the URL where you want it to get things, and it goes off and
# reads the information. There are several flags that can be used to 
# control what is done with the output.
#
# Usage:
#   wwwget <host> <path>
#
# Example:
#   wwwget www.infoseek.com /index.html


# Steve Mansour
# sman@netscape.com
# Aug 27, 1998

use Socket;

die "Usage:\n$0  host  page\n" if $#ARGV <1;


print "Content-type: text/html\n\n";
print &GetHTTP($ARGV[0],$ARGV[1]);
exit 0;

sub GetHTTP
{
  my($remote,$doc) = @_;

  my ($port, $iaddr, $paddr, $proto, $line);

  $port    = 80;
  if ($port =~ /\D/)
      { $port = getservbyname($port, 'tcp') }
      die "No port" unless $port;
  $iaddr   = inet_aton($remote)               || die "no host: $remote";
  $paddr   = sockaddr_in($port, $iaddr);

  $proto   = getprotobyname('tcp');
  socket(SOCK, PF_INET, SOCK_STREAM, $proto)  || die "socket: $!";
  connect(SOCK, $paddr) || die "connect: $!";

  select(SOCK); $| = 1; select(STDOUT);

  ###########################
  # Ask for the data...
  ###########################
  print SOCK "GET $doc HTTP/1.0\n\n";

  ##############################
  # skip over the meta data...
  ##############################
  do {
    $line = <SOCK>
  } until ($line =~ /^\r\n/);

  ##############################
  # gobble up the output...
  ##############################
  @output = <SOCK>;
  close (SOCK) || die "close: $!";
  @output;
}
