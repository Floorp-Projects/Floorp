#!/usr/bin/perl5
#############################################################################
# $Id: tabdump.pl,v 1.4 2000/10/05 19:47:42 leif%netscape.com Exp $
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
#    Generate a TAB separate "dump" of entries matching the search criteria,
#    using the list of attributes specified.
#
#############################################################################

use Getopt::Std;			# To parse command line arguments.
use Mozilla::LDAP::Conn;		# Main "OO" layer for LDAP
use Mozilla::LDAP::Utils;		# LULU, utilities.


#################################################################################
# Constants, shouldn't have to edit these...
#
$APPNAM	= "tabdump";
$USAGE	= "$APPNAM [-nv] -b base -h host -D bind -w pswd -P [-t <sep>] cert attr1,attr2,.. srch";


#################################################################################
# Check arguments, and configure some parameters accordingly..
#
if (!getopts('nvb:h:D:p:s:w:P:t:'))
{
   print "usage: $APPNAM $USAGE\n";
   exit;
}
%ld = Mozilla::LDAP::Utils::ldapArgs();
$separator = (defined($opt_t) ? $opt_t : "\t");

$attributes = $ARGV[$[];
$search = $ARGV[$[ + 1];
die "Need to specify a list of attributes and the search filter.\n"
  unless ($attributes && $search);


#################################################################################
# Do the searches, and produce the output.
#
$conn = new Mozilla::LDAP::Conn(\%ld);
die "Could't connect to LDAP server $ld{host}" unless $conn;

@attr = split(/,/, $attributes);
$entry = $conn->search($ld{root}, $ld{scope}, $search, 0, @attr);
$conn->printError() if $conn->getErrorCode();

while ($entry)
{
  foreach (@attr)
    {
      print $entry->{$_}[0], $separator;
    }
  print "\n";
  $entry = $conn->nextEntry;
}


#################################################################################
# Close the connection.
#
$conn->close if $conn;
