#!/usr/bin/perl5
#############################################################################
# $Id: rmentry.pl,v 1.5 1999/08/24 22:30:51 leif%netscape.com Exp $
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
#    Remove one or several LDAP objects. By default this tool is
#    interactive, which can be disabled with the "-I" option (but
#    please be careful...).
#
#############################################################################

use Getopt::Std;			# To parse command line arguments.
use Mozilla::LDAP::Conn;		# Main "OO" layer for LDAP
use Mozilla::LDAP::Utils;		# LULU, utilities.


#################################################################################
# Constants, shouldn't have to edit these...
#
$APPNAM	= "rmentry";
$USAGE	= "$APPNAM [-nvI] -b base -h host -p port -D bind -w pswd" .
          "-P cert filter ...";

@ATTRIBUTES = ("uid");


#################################################################################
# Check arguments, and configure some parameters accordingly..
#
if (!getopts('nvIb:h:p:D:w:P:'))
{
  print "usage: $APPNAM $USAGE\n";
  exit;
}
%ld = Mozilla::LDAP::Utils::ldapArgs();
Mozilla::LDAP::Utils::userCredentials(\%ld) unless $opt_n;


#################################################################################
# Do the search, and process all the entries.
#
$conn = new Mozilla::LDAP::Conn(\%ld);
die "Could't connect to LDAP server $ld{host}" unless $conn;

$key = "Y" if $opt_I;
foreach $search (@ARGV)
{
  $entry = $conn->search($ld{root}, $ld{scope}, $search, 0, @ATTRIBUTES);
  $conn->printError() if $conn->getErrorCode();

  while ($entry)
    {
      if (! $opt_I)
	{
	  print "Delete $entry->{dn} [N]? ";
	  $key = Mozilla::LDAP::Utils::answer("N") unless $opt_I;
	}

      if ($key eq "Y")
	{
	  if (! $opt_n)
	    {
	      $conn->delete($entry);
	      $conn->printError() if $conn->getErrorCode();
	    }
	  print "Deleted $entry->{dn}\n" if $opt_v;
	}

      $entry = $conn->nextEntry();
    }
}


#################################################################################
# Close the connection.
#
$conn->close if $conn;
