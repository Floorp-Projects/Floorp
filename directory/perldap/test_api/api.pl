#!/usr/bin/perl -w
#############################################################################
# $Id: api.pl,v 1.2 1998/07/24 19:01:49 clayton Exp $
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
#   api.pl - Test all LDAPv2 API function
#   Author:  Clayton Donley <donley@wwa.com>
#
#   Performs all API calls directly in order to test for possible issues
#   on a particular platform.
#
#############################################################################

use Mozilla::LDAP::API (:api :constant);
use strict;

my $BASE = "ou=Test,o=Motorola,c=US";
my $DN = "cn=DSManager,o=Motorola,c=US";
my $PASS = "abcd1234";
my $HOST = "localhost";
my $PORT = 389;

system("/usr/ucb/ps -auxw | grep $$ | grep -v grep");
# Initialize the Connection
{
my $ld = ldap_init($HOST,$PORT);
if ($ld <0)
{
   print "init			- Failed!\n";
   die;
}
print "init		- OK\n";

# Set an LDAP Session Option
if (ldap_set_option($ld,LDAP_OPT_PROTOCOL_VERSION,LDAP_VERSION3)
      != LDAP_SUCCESS)
{
   print "set_option	- Failed!\n";
} else {
   print "set_option	- OK\n";
}

# Get an LDAP Session Option
my $option;
ldap_get_option($ld,LDAP_OPT_REFERRALS,$option);

if ($option != 1)
{
   print "get_option	- Failed!\n";
} else {
   print "get_option	- OK\n";
}

# Anonymous Bind
if (ldap_simple_bind_s($ld,"","") != LDAP_SUCCESS)
{
   print "anon_bind	- Failed!\n";
} else {
   print "anon_bind	- OK\n";
}

# Authenticated Simple Bind
if (ldap_simple_bind_s($ld,$DN,$PASS) != LDAP_SUCCESS)
{
   print "simple_bind	- Failed!\n";
} else {
   print "simple_bind	- OK\n";
}

# Set Rebind Process
my $rebindproc = sub { return($DN,$PASS,LDAP_AUTH_SIMPLE); };
if (ldap_set_rebind_proc($ld,$rebindproc) != LDAP_SUCCESS)
{
   print "set_rebind	- Failed!\n";
} else {
   print "set_rebind	- OK\n";
}

# Add an OrgUnit Entry
my $entry = {
	"objectclass" => ["top","organizationalUnit"],
	"ou" => "Test",
};

if (ldap_add_s($ld,$BASE,$entry) != LDAP_SUCCESS)
{
   print "add_org		- Failed!\n";
} else {
   print "add_org		- OK\n";
}


# Add People
foreach my $number (1..50)
{
   $entry = {
	"objectclass" => ["top","person"],
	"cn" => "Mozilla $number",
	"sn" => ["$number"],
   };
   if (ldap_add_s($ld,"cn=Mozilla $number,$BASE",$entry)
         != LDAP_SUCCESS)
   {
      print "add_user_$number	- Failed!\n";
   } else {
      print "add_user_$number	- OK\n";
   }
}

# Modify People
foreach my $number (1..5)
{
   $entry = {
	"sn" => {"a",["Test"]},
	"telephoneNumber" => "800-555-111$number",
   };
   if (ldap_modify_s($ld,"cn=Mozilla $number,$BASE",$entry)
          != LDAP_SUCCESS)
   {
      print "mod_user_$number	- Failed!\n";
   } else {
      print "mod_user_$number	- OK\n";
   }
}


# Search People
my $filter = "(sn=Test)";
my $attrs = ["cn","sn"];
my $res;
if (ldap_search_s($ld,$BASE,LDAP_SCOPE_SUBTREE,$filter,$attrs,0,$res)
     != LDAP_SUCCESS)
{
   print "search_user	- Failed!\n";
} else {
   print "search_user	- OK\n";
}

# Count Results
if (ldap_count_entries($ld,$res) != 5)
{
   print "count_res	- Failed!\n";
} else {
   print "count_res	- OK\n";
}

# Sort Results
if (ldap_sort_entries($ld,$res,"sn") != LDAP_SUCCESS)
{
   print "sort_ent	- Failed!\n";
} else {
   print "sort_ent	- OK\n";
}

# Multisort Results
if (ldap_multisort_entries($ld,$res,["sn","telephoneNumber"]) != LDAP_SUCCESS)
{
   print "multisort	- Failed!\n";
} else {
   print "multisort	- OK\n";
}

# Get First Entry
my $ent = ldap_first_entry($ld,$res);
if (!$ent)
{
   print "first_entry	- Failed!\n";
} else {
   print "first_entry	- OK\n";
}

# Get Next Entry
$ent = ldap_next_entry($ld,$ent);
if (!$ent)
{
   print "next_entry	- Failed!\n";
} else {
   print "next_entry	- OK\n";
}

# Get DN
my $dn = ldap_get_dn($ld,$ent);
if (!$dn)
{
   print "get_dn		- Failed!\n";
} else {
   print "get_dn		- OK\n";
}

# Get First Attribute
my $ber;
my $attr = ldap_first_attribute($ld,$ent,$ber);
if (!$attr)
{
   print "first_attr	- Failed!\n";
} else {
   print "first_attr	- OK\n";
}

# Get Next Attribute
$attr = ldap_next_attribute($ld,$ent,$ber);
if (!$attr)
{
   print "next_attr	- Failed!\n";
} else {
   print "next_attr	- OK\n";
}

# Get Attribute Values
my @vals = ldap_get_values($ld,$ent,$attr);
if ($#vals < 0)
{
   print "get_values	- Failed!\n";
} else {
   print "get_values	- OK\n";
}

# Free structures pointed to by $ber and $res to prevent memory leak
ldap_ber_free($ber,0);
ldap_msgfree($res);

# Compare Attribute Values
foreach my $number (1..10)
{
   if(ldap_compare_s($ld,"cn=Mozilla $number,$BASE","sn",$number)
         != LDAP_COMPARE_TRUE)
   {
      print "comp_user_$number	- Failed!\n";
   } else {
      print "comp_user_$number	- OK\n";
   }
}

# Delete Users
foreach my $number (1..50)
{
   if (ldap_delete_s($ld,"cn=Mozilla $number,$BASE") != LDAP_SUCCESS)
   {
      print "del_user_$number	- Failed!\n";
   } else {
      print "del_user_$number	- OK\n";
   }
}

if (ldap_delete_s($ld,"$BASE") != LDAP_SUCCESS)
{
   print "del_org		- Failed!\n";
} else {
   print "del_org		- OK\n";
}

# Unbind
ldap_unbind($ld);
}

system("/usr/ucb/ps -auxw | grep $$ | grep -v grep");
