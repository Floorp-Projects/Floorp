#!/usr/bin/perl5
#############################################################################
# $Id: ldappasswd.pl,v 1.2 1998/07/30 09:02:33 leif Exp $
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
# The Original Code is PerlDAP. The Initial Developer of the Original
# Code is Netscape Communications Corp. and Clayton Donley. Portions
# created by Netscape are Copyright (C) Netscape Communications
# Corp., portions created by Clayton Donley are Copyright (C) Clayton
# Donley. All Rights Reserved.
#
# Contributor(s):
#
# DESCRIPTION
#    This is an LDAP version of the normal passwd/yppasswd command found
#    on most Unix systems. Note that this will only use the {crypt}
#    encryption/hash algorithm (at this point).
#
#############################################################################

use Getopt::Std;			# To parse command line arguments.
use Mozilla::LDAP::Conn;		# Main "OO" layer for LDAP
use Mozilla::LDAP::Utils;		# LULU, utilities.


#############################################################################
# Constants, shouldn't have to edit these...
#
$APPNAM	= "ldappasswd";
$USAGE	= "$APPNAM [-nv] -b base -h host -D bind -w pswd -P cert search ...";

@ATTRIBUTES = ("uid", "userpassword");


#############################################################################
# Check arguments, and configure some parameters accordingly..
#
if (!getopts('nvb:s:h:D:w:P:')) {
   print "usage: $APPNAM $USAGE\n";
   exit;
}
%ld = Mozilla::LDAP::Utils::ldapArgs();


#############################################################################
# If there was no user to bind as, try to bind as this user (from the
# environment).
#
if ($ld{bind} eq "")
{
  $conn = new Mozilla::LDAP::Conn(\%ld);
  die "Could't connect to LDAP server $ld{host}" unless $conn;

  $search = "(&(objectclass=inetOrgPerson)(uid=$ENV{USER}))";
  $entry = $conn->search($ld{root}, "subtree", $search, 0, ("uid"));
  if (!$entry || $conn->nextEntry())
    {
      print "Couldn't locate a user to bind as, abort.\n";
      $conn->close();

      exit;
    }

  $conn->close();
  $ld{bind} = $entry->getDN();
  print "Binding as $ld{bind}.\n\n" if $opt_v;
}

if ($ld{pswd} eq "")
{
  print "Enter bind password: ";
  $ld{pswd} = Mozilla::LDAP::Utils::askPassword();
}


#############################################################################
# Ask for the new password, and confirm it's correct.
#
do
{
  print "New password: ";
  $new = Mozilla::LDAP::Utils::askPassword();
  print "New password (again): ";
  $new2 = Mozilla::LDAP::Utils::askPassword();
  print "Passwords didn't match, try again!\n\n" if ($new ne $new2);
} until ($new eq $new2);
$crypted = Mozilla::LDAP::Utils::unixCrypt("$new");


#############################################################################
# Now do all the searches, one by one. If there are no search criteria, we
# will change the password for the user running the script.
#
$conn = new Mozilla::LDAP::Conn(\%ld);
die "Could't connect to LDAP server $ld{host}" unless $conn;

foreach $search ($#ARGV >= $[ ? @ARGV : $ld{bind})
{
  $entry = $conn->search($search, "subtree", "ALL", 0, @ATTRIBUTES);
  $entry = $conn->search($ld{root}, "subtree", $search, 0, @ATTRIBUTES)
    unless $entry;
  print "No such user: $search\n" unless $entry;

  while ($entry)
    {
      $entry->{userpassword} = ["{crypt}" . $crypted];
      print "Updated: $entry->{dn}\n" if $opt_v;

      if (!$opt_n)
	{
	  $conn->update($entry) unless $opt_n;
	  $conn->printError() if $conn->getError();
	}

      $entry = $conn->nextEntry();
    }
}


#############################################################################
# Close the connection.
#
$conn->close if $conn;
