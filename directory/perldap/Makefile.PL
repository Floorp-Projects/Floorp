#############################################################################
# $Id: Makefile.PL,v 1.6 1998/07/31 21:18:29 clayton Exp $
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
#    The Makefile "source".
#
#############################################################################

use ExtUtils::MakeMaker;
use Config;

$perlpath = $Config{'perlpath'};
$osname   = $Config{'osname'};

$include_ldap = $ENV{"LDAPINCLUDE"};
$lib_ldap = $ENV{"LDAPLIB"};
$sslopt = $ENV{"USESSL"};

print "\n\nPerlDAP - Perl 5 Module for LDAP\n";
print     "================================\n";
print "\nPut all sorts of legal bits and pieces here...\n\n";

if (!$include_ldap)
{
   print "Location of LDAP SDK 3.0 Include Files (default: /usr/include):  ";
   chomp ($include_ldap = <>);
   $include_ldap = "/usr/include" unless $include_ldap =~ /\S/;
}

if (!$lib_ldap)
{
   $def_lib = ($include_ldap =~ m%^(\S+)([/\\])include% ? "$1$2lib" : "/usr/lib");
   print "Location of LDAP SDK 3.0 Library Files (default: $def_lib):  ";
   chomp ($lib_ldap = <>);
   $lib_ldap = $def_lib unless $lib_ldap =~ /\S/;
}

if (!$sslopt)
{
   print "Include SSL Routines (default: yes)?  ";
   chomp ($sslopt = <>);
}

if ($sslopt =~ /^n/i)
{
   $ssl_def = "";
} else {
   $ssl_def = "-DUSE_SSL";
}

if ($osname =~ /mswin/i)
{
   if ($ssl_def)
   {
      $ldap_lib = "nsldapssl32v30";
   } else {
      $ldap_lib = "nsldap32v30";
   }
} else {
   if ($ssl_def)
   {
      $ldap_lib = "ldapssl30";
   } else {
      $ldap_lib = "ldap30";
   }
}

$v3_def = "-DLDAPV3";

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
