#############################################################################
# $Id: Makefile.PL,v 1.9 1998/08/13 09:14:28 leif Exp $
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
#    The Makefile "source".
#
#############################################################################

use ExtUtils::MakeMaker;
use Config;

$perlpath = $Config{'perlpath'};
$osname   = $Config{'osname'};

$ldapsdk_loc = $ENV{"LDAPSDKDIR"}; # Full Path to C SDK Top-Level
$ldapsdk_ver = $ENV{"LDAPSDKVER"}; # 11 or 30
$ldapsdk_ssl = $ENV{"LDAPSDKSSL"}; # Y to include SSL

print "\n\nPerlDAP - Perl 5 Module for LDAP\n";
print     "================================\n";
print "\nPut all sorts of legal bits and pieces here...\n\n";

if (!$include_ldap)
{
   print "Directory containing 'include' and 'lib' directory of the Netscape\n";
   print "LDAP Software Developer Kit (default: /usr): ";
   chomp ($ldapsdk_loc = <>);
   $ldapsdk_loc = "/usr" unless $ldapsdk_loc =~ /\S/;
}

if ($osname =~ /mswin/i)
{
   $dir_sep = "\\";
} else {
   $dir_sep = "/";
}

$include_ldap = $ldapsdk_loc . $dir_sep . "include";
$lib_ldap = $ldapsdk_loc . $dir_sep . "lib";

if (!$ldapsdk_ver)
{
   print "Using LDAPv3 Developer Kit (default: yes)?  ";
   chomp ($ver = <>);
   if ($ver =~ /^n/i)
   {
      $ldapsdk_ver = "11";
   } else {
      $ldapsdk_ver = "30";
   }
}

if ($ldapsdk_ver == "30")
{
   $v3def = "-DLDAPV3";
}
   
if (!$ldapsdk_ssl)
{
   print "Include SSL Support (default: yes)?  ";
   chomp ($sslopt = <>);
}

if ($ldapsdk_ssl =~ /^n/i)
{
   $ssl_def = "";
} else {
   $ssl_def = "-DUSE_SSL";
}

if ($osname =~ /mswin/i)
{
   if ($ssl_def)
   {
      if ($ldapsdk_ver == "30")
      {
         $ldap_lib = "nsldapssl32v30";
      } else {
         $ldap_lib = "nsldap32v11";
      }
   } else {
      $ldap_lib = "nsldap32v" . $ldapsdk_ver;
   }
} else {
   if ($ssl_def)
   {
      if ($ldapsdk_ver == "30")
      {
         $ldap_lib = "ldapssl30";
      } else {
         $ldap_lib = "ldap30";
      }
   } else {
      $ldap_lib = "ldap" . $ldapsdk_ver;
   }
}

if ($osname =~ /mswin/i)
{
   $myextlib = "$lib_ldap\\$ldap_lib.lib";
} else {
   $myextlib = "";
}


WriteMakefile(
	'NAME'		=> 	'Mozilla::LDAP::API',
	'VERSION_FROM'	=> 	'API.pm',
	($include_ldap ne "/usr/include" ? (
	'INC'		=> 	"-I$include_ldap",
	) : (
	'INC'		=> 	"",
	)),
	'LIBS'		=> 	["-L$lib_ldap -l$ldap_lib"],
	'MYEXTLIB'	=>	$myextlib,
	'DEFINE'	=> 	"$v3_def $ssl_def",
);
