#############################################################################
# $Id: Utils.pm,v 1.3 1998/07/23 05:25:09 leif Exp $
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

require Exporter;
require Mozilla::LDAP::API;

@ISA = qw(Exporter);
@EXPORT_OK = qw(normalizeDN printEntry printentry encodeBase64
		decodeBase64);


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
# Print an entry, in LDIF format. This is sort of obsolete, we encourage
# you to use the :;LDAP::LDIF class instead.
#
sub printEntry
{
  my ($self, $entry) = @_;
  my $attr;
  local $_;

  print "dn: ", $entry->{dn},"\n";
  foreach $attr (@{$entry->{_oc_order_}})
    {
      next if ($attr eq "_oc_order_");
      next if $entry->{"_${attr}_deleted"};
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
