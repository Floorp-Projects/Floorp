#!/usr/local/bin/perl

#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1996-2003
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#	Mark Smith <MarkCSmithWork@aol.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK ***** 


# Open many connections to a host.
#
# usage: connect host port [count]
#

require 5.002;
use strict;
use IO::Socket;

my ( $host, $port, $count, $msg, $i, @handle );

unless (@ARGV >= 2 && @ARGV <=4) { die "usage: $0 host port [count] [msg]" };

$host = shift;
$port = shift;
$count = shift || 1;	# try to make just one connection by default
$msg = shift || "";

for ( $i = 0; $i < $count; $i++ ) {
	$handle[$i] = IO::Socket::INET->new(
			PeerAddr => $host,
			PeerPort => $port,
			Proto => 'tcp' )
			or die "can't connect to port $port on $host: $!";
	print "$handle[$i] (", $i+1, " /$count)\n";
	if ( $msg ne "" ) {
		send $handle[$i], $msg, 0;
		print "wrote msg: $msg\n";
	}
}

print "Sleeping for 1 hour....\n";
sleep 3600;
exit;
