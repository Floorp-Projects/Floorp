#############################################################################
# $Id: test.pl,v 1.2 1998/07/21 02:42:37 leif Exp $
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
# The Original Code is PerlDap. The Initial Developer of the Original
# Code is Netscape Communications Corp. and Clayton Donley. Portions
# created by Netscape are Copyright (C) Netscape Communications
# Corp., portions created by Clayton Donley are Copyright (C) Clayton
# Donley. All Rights Reserved.
#
# Contributor(s):
#
# DESCRIPTION
#    Before `make install' is performed this script should be runnable with
#    `make test'. After `make install' it should work as `perl test.pl'
#
#############################################################################

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..8\n"; }
END {print "modinit  - not ok\n" unless $loaded;}
use Mozilla::LDAP::API;
$loaded = 1;
print "modinit  - ok\n";

######################### End of black magic.


$attrs = ["cn","telephoneNumber","mail"];
print "\nEnter LDAP Server: ";
chomp($ldap_host = <>);
print "Enter Search Filter (ex. uid=abc123): ";
chomp($filter = <>);
print "Enter LDAP Search Base (ex. o=Org, c=US): ";
chomp($BASEDN = <>);
print "\n";

if (!$ldap_host)
{
   die "Please edit \$BASEDN, \$filter and \$ldap_host in test.pl.\n";
}

##
##  Initialize LDAP Connection
##

if (($ld = ldap_init($ldap_host,LDAP_PORT)) == -1)
{
   print "open     - not ok\n";
   exit -1; 
}
print "open     - ok\n";

##
##  Bind as DN, PASSWORD (NULL,NULL) on LDAP connection $ld
##

if (ldap_simple_bind_s($ld,"","") != LDAP_SUCCESS)
{
   ldap_perror($ld,"bind_s");
   print "bind     - not ok\n";
   exit -1;
}
print "bind     - ok\n";

##
## ldap_search_s - Synchronous Search
##

if (ldap_search_s($ld,$BASEDN,LDAP_SCOPE_SUBTREE,$filter,$attrs,0,$result) != LDAP_SUCCESS)
{
   ldap_perror($ld,"search_s");
   print  "search   - not ok\n";
}
print "search   - ok\n";

$status = ldap_parse_result($ld,$result,$a,$b,$c,$d,$e,0);

print "parse    - ok	- $a:$b:$c:" . join(@{$d},":") . "\n";

##
## ldap_count_entries - Count Matched Entries
##

if (($count = ldap_count_entries($ld,$result)) == -1)
{
   ldap_perror($ld,"count_entry");
   print "count    - not ok\n";
}
print "count    - ok	- $count\n";

##
## first_entry - Get First Matched Entry
## next_entry  - Get Next Matched Entry
##

   for ($ent = ldap_first_entry($ld,$result); $ent; $ent = ldap_next_entry($ld,$ent))
   {
      
##
## ldap_get_dn  -  Get DN for Matched Entries
##

      if (($dn = ldap_get_dn($ld,$ent)) ne "")
      {
         print "getdn    - ok	- $dn\n";
      } else {
         ldap_perror($ld,"get_dn");
         print "getdn    - not ok\n";
      }

      for ($attr = ldap_first_attribute($ld,$ent,$ber); $attr; $attr = ldap_next_attribute($ld,$ent,$ber))
      {
         print "firstatt - ok	- $attr\n";

##
## ldap_get_values
##

         @vals = ldap_get_values($ld,$ent,$attr);
         if ($#vals >= 0)
         {
            foreach $val (@vals)
            {
               print "getvals  - ok	- $val\n";
            }
         } else {
            print "getvals  - not ok\n";
         }
      }


   }

##
##  Unbind LDAP Connection
##

ldap_unbind($ld);

