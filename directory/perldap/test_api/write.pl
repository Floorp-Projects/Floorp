#!/usr/bin/perl -w
#############################################################################
# $Id: write.pl,v 1.5 2000/10/05 19:47:47 leif%netscape.com Exp $
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
# The Original Code is PerlDAP. The Initial Developer of the Original
# Code is Netscape Communications Corp. and Clayton Donley. Portions
# created by Netscape are Copyright (C) Netscape Communications
# Corp., portions created by Clayton Donley are Copyright (C) Clayton
# Donley. All Rights Reserved.
#
# Contributor(s):
#
# DESCRIPTION
#   write.pl - Test of LDAP Modify Operations in Perl5
#   Author:  Clayton Donley <donley@wwa.com>
#
#   This utility is mostly to demonstrate all the write operations
#   that can be done with LDAP through this PERL5 module.
#
#############################################################################

use strict;
use Mozilla::LDAP::API qw(:constant :api);


# This is the entry we will be adding.  Do not use a pre-existing entry.
my $ENTRYDN = "cn=Test Guy, o=Org, c=US";

# This is the DN and password for an Administrator
my $ROOTDN = "cn=DSManager,o=Org,c=US";
my $ROOTPW = "";

my $ldap_server = "";

if (!$ldap_server)
{
   print "Edit the top portion of this file before continuing.\n";
   exit -1;
}

my $ld = ldap_init($ldap_server,LDAP_PORT);

if ($ld == -1)
{
   die "Connection to LDAP Server Failed";
}

if (ldap_simple_bind_s($ld,$ROOTDN,$ROOTPW) != LDAP_SUCCESS)
{
   ldap_perror($ld,"bind_s");
   die;
}

my %testwrite = (
	"cn" => "Test User",
	"sn" => "User",
        "givenName" => "Test",
	"telephoneNumber" => "8475551212",
	"objectClass" => ["top","person","organizationalPerson",
           "inetOrgPerson"],
        "mail" => "tuser\@my.org",
);

if (ldap_add_s($ld,$ENTRYDN,\%testwrite) != LDAP_SUCCESS)
{
   ldap_perror($ld,"add_s");
   die;
}

print "Entry Added.\n";


%testwrite = (
	"telephoneNumber" => "7085551212",
        "mail" => {"a",["Test_User\@my.org"]},
);

if (ldap_modify_s($ld,$ENTRYDN,\%testwrite) != LDAP_SUCCESS)
{
   ldap_perror($ld,"modify_s");
   die;
}

print "Entry Modified.\n";

#
# Delete the entry for $ENTRYDN
#
if (ldap_delete_s($ld,$ENTRYDN) != LDAP_SUCCESS)
{
   ldap_perror($ld,"delete_s");
   die;
}

print "Entry Deleted.\n";

# Unbind to LDAP server
ldap_unbind($ld);

exit;
