#############################################################################
# $Id: Makefile.PL,v 1.15 1999/08/24 22:30:46 leif%netscape.com Exp $
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
use Carp;

$perlpath = $Config{'perlpath'};
$osname   = $Config{'osname'};

$ldapsdk_loc = $ENV{"LDAPSDKDIR"}; # Full Path to C SDK Top-Level
$ldapsdk_ssl = $ENV{"LDAPSDKSSL"}; # N to exclude SSL
$ldapsdk_ver = $ENV{"LDAPV3ON"};   # N to exclude LDAP v3 features

$libexts = "so|sl|a|lib";


print "\nPerLDAP - Perl 5 Module for LDAP\n";
print     "================================\n";

$silent = 1;
print "Directory containing 'include' and 'lib' directory of the Netscape\n";
print "LDAP Software Developer Kit (default: /usr): ";
if (!$ldapsdk_loc)
{
   $silent = 0;
   chomp ($ldapsdk_loc = <STDIN>);
   $ldapsdk_loc = "/usr" unless $ldapsdk_loc =~ /\S/;
} else {
  print "$ldapsdk_loc\n";
}
croak("Directory $ldapsdk_loc does not exist!") unless -d $ldapsdk_loc;

if ($osname =~ /mswin/i)
{
   $dir_sep = "\\";
} else {
   $dir_sep = "/";
}

$include_ldap = $ldapsdk_loc . $dir_sep . "include";
$lib_ldap = $ldapsdk_loc . $dir_sep . "lib";

print "Using LDAPv3 Developer Kit (default: yes)?  ";
if (!$ldapsdk_ver)
{
   $silent = 0;
   chomp ($ldapsdk_ver = <STDIN>);
} else {
  print "YES\n";
}
$v3_def = "-DLDAPV3" unless ($ldapsdk_ver =~ /^n/i);

   
print "Include SSL Support (default: yes)?  ";
if (!$ldapsdk_ssl)
{
   $silent = 0;
   chomp ($ldapsdk_ssl = <STDIN>);
} else {
  print "YES\n";
}
$ssl_def = "-DUSE_SSL" unless ($ldapsdk_ssl =~ /^n/i);


opendir(DIR,$lib_ldap);
@files = grep{/ldap|lber/} readdir(DIR);
closedir(DIR);

if (!((@ldaplib = grep{/ldapssl.*\.($libexts)$/} @files) && $ssl_def))
{
   @ldaplib = grep{/ldap.*\.($libexts)$/} @files;
   @lberlib = grep{/lber.*\.($libexts)$/} @files;
}

if ($#ldaplib < 0)
{
   die "No LDAP libraries found.";
}

if ($#ldaplib > 0)
{
   print "Located multiple libraries:\n";
   foreach $alib (@ldaplib)
   {
      print " - $alib\n";
   }
}

$lline_ldap = $ldaplib[0];
$lline_ldap =~ s/^lib//;
$lline_ldap =~ s/\.($libexts)$//;
$lline = "-L$lib_ldap -l$lline_ldap";

if ($#lberlib >= 0 && $lline =~ /ldap$/)
{
   $lline_lber = $lberlib[0];
   $lline_lber =~ s/^lib//;
   $lline_lber =~ s/\.($libexts)$//;
   $lline .= " -l$lline_lber";
}

print "Libraries to link with (default: $lline):  ";
if (!$silent)
{
   chomp ($lib_line = <STDIN>);
   $lib_line = $lline unless $lib_line =~ /\S/;
} else {
  print "\n";
}


# Include directories etc.
$my_includes = "";
$my_includes .= " -I$include_ldap" unless ($include_ldap eq "/usr/include");


# Add system dependant stuff here...
@extras = ();
if ($osname =~ /mswin/i)
{
   $my_extlib = "$lib_ldap\\$ldaplib[0]";
   $my_extlib .= " $lib_ldap\\$lberlib[0]" if $lber_lib;
   push(@extras, 'dynamic_lib' => {
     'OTHERLDFLAGS' => "kernel32.lib oldnames.lib" });
} else {
   $my_extlib = "";
}

push(@extras,
     CAPI => 'TRUE')
    if ($] >= 5.005 and $^O eq 'MSWin32'
        and $Config{archname} =~ /-object\b/i);

push(@extras,
     ABSTRACT => 'Perl methods for LDAP C API calls',
     AUTHOR   => 'Netscape Communications Corp., Inc. and Clayton Donley')
    if ($] >= 5.005);

#
# Ok, let's do it!
#
WriteMakefile(
	      'NAME'		=> 	'Mozilla::LDAP::API',
	      'DISTNAME'	=>	'PerLDAP',
	      'VERSION_FROM'	=> 	'API.pm',
	      'INC'		=>	$my_includes,
	      'LIBS'		=> 	[$lib_line],
	      'MYEXTLIB'	=>	$my_extlib,
	      'DEFINE'		=> 	"$v3_def $ssl_def",
	      'XSOPT'		=>	"-nolinenumbers",
	      @extras
);


#
# Generate a "make HTML" target
#
sub MY::postamble
{
  '
.SUFFIXES: .pm .html
.PHONY: html

.pm.html:
	pod2html --netscape $< > $@

html:	Entry.html Conn.html Utils.html API.html LDIF.html $(FIRST_MAKEFILE)
	@rm -f pod2html-itemcache pod2html-dircache
'
}
