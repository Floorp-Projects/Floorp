#!/usr/bin/perl5
#############################################################################
# $Id: monitor.pl,v 1.3 2000/10/05 19:47:37 leif%netscape.com Exp $
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is PerLDAP. The Initial Developer of the Original
# Code is Netscape Communications Corp. and Clayton Donley. Portions
# created by Netscape are Copyright (C) Netscape Communications
# Corp., portions created by Clayton Donley are Copyright (C) Clayton
# Donley. All Rights Reserved.
#
# Contributor(s):
#
# DESCRIPTION
#    Ask the directory server for it's monitor entry, to see some
#    performance and usage stats.
#
#############################################################################

use Getopt::Std;			# To parse command line arguments.
use Mozilla::LDAP::Conn;		# Main "OO" layer for LDAP
use Mozilla::LDAP::Utils;		# LULU, utilities.


#################################################################################
# Constants, shouldn't have to edit these...
#
$APPNAM	= "monitor";
$USAGE	= "$APPNAM [-nv] -b base -h host -D bind -w pswd -P cert";


#################################################################################
# Check arguments, and configure some parameters accordingly..
#
if (!getopts('b:h:D:p:w:P:'))
{
   print "usage: $APPNAM $USAGE\n";
   exit;
}
%ld = Mozilla::LDAP::Utils::ldapArgs("", "cn=monitor");


#################################################################################
# Instantiate an LDAP object, which also binds to the LDAP server, and then
# do the simple search.
#
$conn = new Mozilla::LDAP::Conn(\%ld);
die "Could't connect to LDAP server $ld{host}" unless $conn;

$entry = $conn->search($ld{root}, "base", "objectclass=*");
Mozilla::LDAP::Utils::printEntry($entry)
  if ($entry);


#################################################################################
# Close the connection.
#
$conn->close if $conn;
