#############################################################################
# $Id: Utils.pm,v 1.4 1998/07/29 08:29:07 leif Exp $
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
#    Lots of Useful Little Utilities, for LDAP related operations.
#
#############################################################################

package Mozilla::LDAP::Utils;

use Mozilla::LDAP::API qw(:constant);
use vars qw(@ISA %EXPORT_TAGS);

require Exporter;

@ISA = qw(Exporter);
%EXPORT_TAGS = (
		all => [qw(normalizeDN
			   isUrl
			   printEntry
			   printentry
			   encodeBase64
			   decodeBase64
			   str2Scope
			   ldapArgs)]
		);


# Add Everything in %EXPORT_TAGS to @EXPORT_OK
Exporter::export_ok_tags('all');


#############################################################################
# Normalize the DN string (first argument), and return the new, normalized,
# string (DN). This is useful to make sure that two syntactically
# identical DNs compare (eq) as the same string.
#
sub normalizeDN
{
  my ($self, $dn) = @_;
  my (@vals);

  $dn = $self->{dn} unless $dn;
  return "" if ($dn eq "");

  $vals = Mozilla::LDAP::API::ldap_explode_dn(lc $dn, 0);

  return join(",", @{$vals});
}


#############################################################################
# Checks if a string is a properly formed LDAP URL.
#
sub isURL
{
  return ldap_is_ldap_url($_[0]);
}


#############################################################################
# Print an entry, in LDIF format. This is sort of obsolete, we encourage
# you to use the :;LDAP::LDIF class instead.
#
sub printEntry
{
  my $entry = $_[0];
  my $attr;
  local $_;

  print "dn: ", $entry->{dn},"\n";
  foreach $attr (@{$entry->{_oc_order_}})
    {
      next if ($attr =~ /^_.+_$/);
      next if $entry->{"_${attr}_deleted_"};
      foreach (@{$entry->{$attr}})
	{
	  print "$attr: $_\n";
	}
    }

  print "\n";
}
*printentry = \*printEntry;


#############################################################################
# Perform Base64 encoding, this is based on MIME::Base64.pm, written
# by Gisle Aas <gisle@aas.no>. If possible, use the MIME:: package instead.
#
sub encodeBase64
{
  my $res = "";
  my $eol = "$_[1]";
  my $padding;

  pos($_[0]) = 0;                          # ensure start at the beginning
  while ($_[0] =~ /(.{1,45})/gs) {
    $res .= substr(pack('u', $1), 1);
    chop($res);
  }

  $res =~ tr|` -_|AA-Za-z0-9+/|;               # `# help emacs
  $padding = (3 - length($_[0]) % 3) % 3;
  $res =~ s/.{$padding}$/'=' x $padding/e if $padding;

  if (length $eol) {
    $res =~ s/(.{1,76})/$1$eol/g;
  }

  return $res;
}
 
 
#############################################################################
# Perform Base64 decoding, this is based on MIME::Base64.pm, written
# by Gisle Aas <gisle@aas.no>. If possible, use the MIME:: package instead.
#
sub decodeBase64
{
  my $str = shift;
  my $res = "";
  my $len;
 
  $str =~ tr|A-Za-z0-9+=/||cd;
  Carp::croak("Base64 decoder requires string length to be a multiple of 4")
    if length($str) % 4;

  $str =~ s/=+$//;                        # remove padding
  $str =~ tr|A-Za-z0-9+/| -_|;            # convert to uuencoded format
  while ($str =~ /(.{1,60})/gs)
    {
      $len = chr(32 + length($1)*3/4);
      $res .= unpack("u", $len . $1 );    # uudecode
    }

  return $res;
}


#############################################################################
# Convert a "human" readable string to an LDAP scope value
#
sub str2Scope
{
  my $str = $_[0];

  return $str if ($str =~ /^[0-9]+$/);

  if ($str =~ /^sub/i)
    {
      return LDAP_SCOPE_SUBTREE;
    }
  elsif ($str =~ /^base/i)
    {
      return LDAP_SCOPE_BASE;
    }
  elsif ($str =~ /^one/i)
    {
      return LDAP_SCOPE_ONELEVEL;
    }

  # Default...
  return LDAP_SCOPE_SUBTREE;
}


#################################################################################
# Ask for a password, without displaying it on the TTY. This is very non-
# portable, we need a better solution (using the term package perhaps?).
#
sub askPassword
{
  system('/bin/stty -echo');
  chop($_ = <STDIN>);
  system('/bin/stty echo');
  print "\n";

  return $_;
}


#################################################################################
# Handle some standard LDAP options, and construct a nice little structure that
# we can use later on.
#
sub ldapArgs
{
  my($bind, $base) = @_;
  my(%ld);

  $main::opt_v = $main::opt_n if $main::opt_n;
  $main::opt_p = LDAPS_PORT unless ($main::opt_p || ($main::opt_P eq ""));

  $ld{host} = $main::opt_h || "ldap";
  $ld{port} = $main::opt_p || LDAP_PORT;
  $ld{root} = $main::opt_b || $base || $ENV{'LDAP_BASEDN'};
  $ld{bind} = $main::opt_D || $bind || "";
  $ld{pswd} = $main::opt_w || "";
  $ld{cert} = $main::opt_P || "";
  $ld{scope} = $main::opt_s || LDAP_SCOPE_SUBTREE;

  if (($ld{bind} ne "") && ($ld{pswd} eq ""))
    {
      print "LDAP password: ";
      $ld{pswd} = askPassword();
    }

  return %ld;
}
