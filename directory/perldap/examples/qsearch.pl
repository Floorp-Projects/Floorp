#!/usr/bin/perl5
#############################################################################
# $Id: qsearch.pl,v 1.8 1999/01/21 23:52:47 leif%netscape.com Exp $
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
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
#    Quick Search, like ldapsearch, but in Perl. Look how simple it is.
#
#############################################################################

use Getopt::Std;			# To parse command line arguments.
use Mozilla::LDAP::Conn;		# Main "OO" layer for LDAP
use Mozilla::LDAP::Utils;		# LULU, utilities.

use strict;
no strict "vars";


#################################################################################
# Constants, shouldn't have to edit these...
#
$APPNAM	= "qsearch";
$USAGE	= "$APPNAM -b base -h host -D bind -w pswd -P cert filter [attr...]";


#################################################################################
# Check arguments, and configure some parameters accordingly..
#
if (!getopts('b:h:D:p:s:w:P:'))
{
   print "usage: $APPNAM $USAGE\n";
   exit;
}
%ld = Mozilla::LDAP::Utils::ldapArgs();


#################################################################################
# Now do all the searches, one by one.
#
$conn = new Mozilla::LDAP::Conn(\%ld);
die "Could't connect to LDAP server $ld{host}" unless $conn;

foreach (@ARGV)
{
  if (/\=/)
    {
      push(@srch, $_);
    }
  else
    {
      push(@attr, $_);
    }
}

foreach $search (@srch)
{
  if ($#attr >= $[)
    {
      $entry = $conn->search($ld{root}, $ld{scope}, $search, 0, @attr);
    }
  else
    {
      $entry = $conn->search($ld{root}, $ld{scope}, "$search");
    }

  print "Searched for `$search':\n\n";
  $conn->printError() if $conn->getErrorCode();

  while ($entry)
    {
      $entry->printLDIF();
      $entry = $conn->nextEntry;
    }
  print "\n";
}


#################################################################################
# Close the connection.
#
$conn->close if $conn;
