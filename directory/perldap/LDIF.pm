#############################################################################
# $Id: LDIF.pm,v 1.3 1998/08/09 01:16:54 leif Exp $
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
# Creator, the argument (optional) is the file handle.
#
sub new
{
  my ($class, $fh) = @_;
  my $self = {};

  if ($fh)
    {
      $self->{_fh_} = $fh;
      $self->{_canRead_} = 1;
      $self->{_canWrite_} = 1;
    }
  else
    {
      $self->{_fh_} = STDOUT;
      $self->{_canRead_} = 1;
      $self->{_canWrite_} = 0;
    }

  return bless $self, $class;
}


#############################################################################
# Destructor, close file descriptors etc. (???)
#
sub DESTROY
{
  my $self = shift;
}


#############################################################################
# Read the next $entry from an ::LDIF object. No arguments
#
sub readOneEntry
{
  my ($self) = @_;
  my ($attr, $val, $entry, $base64);
  local $_;

  # Skip leading empty lines.
  $entry = new Mozilla::LDAP::Entry;
  while (<$self->{_fh_}>)
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

      $_ = <$self->{_fh_}>;
      chop;
    } until /^\s*$/;

  # Do the last attribute... Icky.
  if ($attr && ($attr ne "dn"))
    {
      $val = decode_base64($val) if $base64;
      $entry->addValue($attr, $val);
    }

  return $entry;
}
*readEntry = \readOneEntry;


#############################################################################
# Print one entry, to the file handle. Note that we actually use some
# internals from the ::Entry object here, which is a no-no...
#
sub writeOneEntry
{
  my ($self, $entry) = @_;

  print $self->{_fh_} "dn: ", $self->getDN(),"\n";
  foreach $attr (@{$self->{_oc_order_}})
    {
      next if ($attr =~ /^_.+_$/);
      next if $self->{"_${attr}_deleted_"};
      grep((print $self->{_fh_} "$attr: $_\n"), @{$self->{$attr}});
    }

  print $self->{_fh_} "\n";
}
*writeEntry = \writeOneEntry;


#############################################################################
# Read multiple entries, and return an array of Entry objects. The argument
# is the number to read, or read them all if not specified.
#
sub readEntries
{
  my ($self, $num) = @_;
  my $entry;
  my (@entries);

  return @entries;
}


#############################################################################
# Write multiple entries, the argument is the array of Entry objects.
#
sub writeEntries
{
  my ($self, @entries) = @_;
  local $_;
  
  foreach (@entries)
    {
      $self->writeOneEntry($_);
    }
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

There are plenty of examples to look at, in the examples directory. We are
adding more examples every day (almost).

=head1 INSTALLATION

Installing this package is part of the Makefile supplied in the
package. See the installation procedures which are part of this package.

=head1 AVAILABILITY

This package can be retrieved from a number of places, including:

    http://www.mozilla.org/
    Your local CPAN server

=head1 AUTHOR INFORMATION

Address bug reports and comments to:
xxx@netscape.com

=head1 CREDITS

Most of this code was developed by Leif Hedstrom, Netscape Communications
Corporation. 

=head1 BUGS

None. :)

=head1 SEE ALSO

L<Mozilla::LDAP::Conn>, L<Mozilla::LDAP::Entry>, L<Mozilla::LDAP::API>,
and of course L<Perl>.

=cut
