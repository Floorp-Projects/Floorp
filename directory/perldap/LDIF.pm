#############################################################################
# $Id: LDIF.pm,v 1.6 1999/01/21 23:52:42 leif%netscape.com Exp $
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
#    Simple routines to read and write LDIF style files. You should open
#    the input/output file manually, or use STDIN/STDOUT.
#
#############################################################################

package Mozilla::LDAP::LDIF;

use Mozilla::LDAP::Entry;
use Mozilla::LDAP::Utils(qw(decodeBase64));


#############################################################################
# Creator, the argument (optional) is the file handle.
#
sub new
{
  my ($class, $fh) = @_;
  my $self = {};

  if ($fh)
    {
      $self->{"_fh_"} = $fh;
      $self->{"_canRead_"} = 1;
      $self->{"_canWrite_"} = 1;
    }
  else
    {
      $self->{"_fh_"} = STDIN;
      $self->{"_canRead_"} = 1;
      $self->{"_canWrite_"} = 0;
    }

  return bless $self, $class;
}


#############################################################################
# Destructor, close file descriptors etc. (???)
#
#sub DESTROY
#{
#  my $self = shift;
#}


#############################################################################
# Read the next $entry from an ::LDIF object. No arguments
#
sub readOneEntry
{
  my ($self) = @_;
  my ($attr, $val, $entry, $base64, $fh);
  local $_;

  return unless $self->{"_canRead_"};
  return unless defined($self->{"_fh_"});

  # Skip leading empty lines.
  $fh = $self->{"_fh_"};
  while (<$fh>)
    {
      chop;
      last unless /^\s*$/;
    }
  return if /^$/;		# EOF

  $self->{"_canWrite_"} = 0 if $self->{"_canWrite_"};

  $entry = new Mozilla::LDAP::Entry();
  do
    {
      # See if it's a continuation line.
      if (/^ /o)
	{
	  $val .= substr($_, 1);
	}
      else
	{
	  if ($val && $attr)
	    {
	      if ($attr eq "dn")
		{
		  $entry->setDN($val);
		}
	      else
		{
		  $val = decodeBase64($val) if $base64;
		  $entry->addValue($attr, "$val", 1);
		}
	    }
	  ($attr, $val) = split(/:\s+/, $_, 2);
	  $attr = lc $attr;

	  # Handle base64'ed data.
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
      $val = decodeBase64($val) if $base64;
      $entry->addValue($attr, "$val", 1);
    }

  return $entry;
}
*readEntry = \readOneEntry;


#############################################################################
# Print one entry, to the file handle. Note that we actually use some
# internals from the ::Entry object here, which is a no-no... Also, we need
# to support Base64 encoding of Binary attributes here.
#
sub writeOneEntry
{
  my ($self, $entry) = @_;
  my ($fh, $attr);

  return unless $self->{"_canWrite_"};
  $self->{"_canRead_"} = 0 if $self->{"_canRead_"};

  $fh = $self->{"_fh_"};
  print $fh "dn: ", $entry->getDN(),"\n";
  foreach $attr (@{$entry->{"_oc_order_"}})
    {
      next if ($attr =~ /^_.+_$/);
      next if $entry->{"_${attr}_deleted_"};
      # TODO: Add support for Binary attributes.
      grep((print $fh "$attr: $_\n"), @{$entry->{$attr}});
    }

  print $fh "\n";
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

  return if (defined($num) && ($num ne "") && ($num <= 0));
  $num = (-1) unless defined($num);

  do 
    {
      $entry = $self->readOneEntry();
      push(@entries, $entry) if ($entry);
      $num--;
    } until (! $entry || $num == 0);
      
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

This package is used to read and write LDIF information from files
(actually, file handles). It can also be used to generate LDIF modify
files from changes made to an entry.

=head1 DESCRIPTION

The LDIF format is a simple, yet useful, text representation of an LDAP
database. The goal of this package is to make it as easy as possible to
read, parse and use LDIF data, possible generated from other information
sources.

=head1 EXAMPLES

There are plenty of examples to look at, in the examples directory. We are
adding more examples every day (almost).

=head1 INSTALLATION

Installing this package is part of the Makefile supplied in the
package. See the installation procedures which are part of this package.

=head1 AVAILABILITY

This package can be retrieved from a number of places, including:

    http://www.mozilla.org/directory/
    Your local CPAN server

=head1 CREDITS

Most of this code was developed by Leif Hedstrom, Netscape Communications
Corporation. 

=head1 BUGS

None. :)

=head1 SEE ALSO

L<Mozilla::LDAP::Conn>, L<Mozilla::LDAP::Entry>, L<Mozilla::LDAP::API>,
and of course L<Perl>.

=cut
