#############################################################################
# $Id: LDIF.pm,v 1.2 1998/07/23 05:31:22 leif Exp $
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
#    This is a description.
#
#############################################################################

package Mozilla::LDAP::LDIF;


#############################################################################
# Read the next $entry from an ::LDIF object.
#
sub readEntry
{
  my($fh) = @_;
  my($attr, $val, $entry, $base64);
  local $_;

  # Skip leading empty lines.
  $entry = new Ldapp::Entry;
  while (<$fh>)
    {
      chop;
      last unless /^\s*$/;
    }
  return if /^$/;		# EOF

  $base64 = 0;
  do
    {
      # See if it's a continuation line.
      if (/^ /o)
	{
	  $val .= substr($_, 1);
	}
      else
	{
	  if ($attr eq "dn")
	    {
	      $entry->setDN($val);
	    }
	  elsif ($attr && $val)
	    {
	      $val = decode_base64($val) if $base64;
	      $entry->addValue($attr, "$val");
	    }
	  ($attr, $val) = split(/:\s+/, $_, 2);
	  $attr = lc $attr;
	  # Handle base-64'ed data.
	  if ($attr =~ /:$/o)
	    {
	      $base64 = 1;
	      chop($attr);
	    }
	  else
	    {
	      $base64 = 0;
	    }
	}

      $_ = <$fh>;
      chop;
    } until /^\s*$/;

  # Do the last attribute...
  if ($attr && ($attr ne "dn"))
    {
      $val = decode_base64($val) if $base64;
      $entry->addValue($attr, $val);
    }

  return $entry;
}


#############################################################################
# Mandatory TRUE return value.
#
1;


#############################################################################
# POD documentation...
#
__END__

=head1 NAME

Mozilla::LDAP::LDIF - Read, write and modify LDIF files.

=head1 SYNOPSIS

  use Mozilla::LDAP::LDIF;

=head1 ABSTRACT

This package is the...

=head1 DESCRIPTION

LDIF rules...

=head1 EXAMPLES

There are plenty of examples to look at, in the is-ldap/perl directory. If
you don't have these files, get the distribution from one of the sites below. 

=head1 INSTALLATION

Installing this package is part of the Makefile supplied in the
package. To install and use this module, you'll first need to download and
install the LDAP SDK (e.g. from http://developer.netscape.com). To build
the Perl modules, just do

    perl5 Makefile.PL
    make
    make install

You'll get some questions during the first step, asking you to specify
where your LDAP SDK installed, and how to configure the package.

=head1 AVAILABILITY

The latest version of this script, and other related packages, are
available from

  
=head1 AUTHOR INFORMATION

Address bug reports and comments to:

=head1 CREDITS



=head1 BUGS and TODO

None :).

=over 4

=item *

Finish it.

=back

=head1 SEE ALSO

L<Mozilla::LDAP::API>, L<Mozilla::LDAP::Entry> and L<Perl>

=cut
