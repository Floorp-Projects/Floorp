#!/usr/bin/perl5
#############################################################################
# $Id: lfinger.pl,v 1.11 2000/10/05 19:47:36 leif%netscape.com Exp $
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
#    "finger" version using LDAP information (using RFC 2307 objectclass).
#
#############################################################################

use Getopt::Std;			# To parse command line arguments.
use Mozilla::LDAP::Conn;		# Main "OO" layer for LDAP
use Mozilla::LDAP::Utils;		# LULU, utilities.


#############################################################################
# Constants, shouldn't have to edit these... The HIDE mechanism is a very
# Netscape internal specific feature. We use this objectclass to mark some
# entries to be "hidden", and some of our applications will honor this. With
# more recent versions of the Directory Server this can be accomplished more
# effectively with appropriate ACI/ACLs.
#
$APPNAM	= "lfinger";
$USAGE	= "$APPNAM -m -b base -h host -D bind -w pswd -P cert user_info";

@ATTRIBUTES = ("uid", "cn", "homedirectory", "loginshell", "pager",
	       "telephonenumber", "facsimiletelephonenumber", "mobile");
$HIDE = "(objectclass=nscphidethis)";



#############################################################################
# Print a "finger" entry.
#
sub printIt
{
  my($entry) = @_;

  print "Login name: $entry->{uid}[0]";
  print " " x (39 - 11 - length($entry->{uid}[0]));
  print "In real life: $entry->{cn}[0]\n";
  if ($entry->{homedirectory}[0] || $entry->{loginshell}[0])
    {
      print "Directory: $entry->{homedirectory}[0]";
      print " " x (39 - 10 - length($entry->{homedirectory}[0]));
      print "Shell: $entry->{loginshell}[0]\n";
    }

  if ($entry->{telephonenumber}[0] || $entry->{pager}[0])
    {
      print "Phone: $entry->{telephonenumber}[0]";
      print " " x (39 - 6 - length($entry->{telephonenumber}[0]));
      print "Pager: $entry->{pager}[0]\n";
    }

  if ($entry->{mobile}[0] || $entry->{facsimiletelephonenumber}[0])
    {
      print "Mobile: $entry->{mobile}[0]";
      print " " x (39 - 7 - length($entry->{mobile}[0]));
      print "Fax: $entry->{facsimiletelephonenumber}[0]\n";
    }

  print "\n";
}


#############################################################################
# Check arguments, and configure some parameters accordingly..
#
if (!getopts('mb:h:D:p:w:P:') || !defined($ARGV[$[]))
{
   print "usage: $APPNAM $USAGE\n";
   exit;
}
%ld = Mozilla::LDAP::Utils::ldapArgs();
$user=$ARGV[$[];


#############################################################################
# Instantiate an LDAP object, which also binds to the LDAP server.
#
$conn = new Mozilla::LDAP::Conn(\%ld);
die "Could't connect to LDAP server $ld{host}" unless $conn;


#############################################################################
# Ok, lets generate the filter, and do the search!
#
if ($opt_m)
{
  $search = "(&(uid=$user)(!$HIDE))";
}
else
{
  $search = "(&(|(cn=*$user*)(uid=*$user*)(telephonenumber=*$user*))(!$HIDE))";
}

$entry = $conn->search($ld{root}, "subtree", $search, 0, @ATTRIBUTES);
$conn->printError() if $conn->getErrorCode();

while($entry)
{
  printIt($entry);
  $entry = $conn->nextEntry();
}


#############################################################################
# Close the connection.
#
$conn->close if $conn;
