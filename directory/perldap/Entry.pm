#############################################################################
# $Id: Entry.pm,v 1.6 1998/08/09 01:16:53 leif Exp $
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
#    This package defines an object class to manage one single LDAP
#    entry. This entry can either be a newly created one, or one
#    retrieved from an LDAP server, using the Mozilla::LDAP::Conn class.
#
#############################################################################

package Mozilla::LDAP::Entry;

require Tie::Hash;
@ISA = (Tie::StdHash);


#############################################################################
# Creator, make a new tie hash instance, which will keep track of all
# changes made to the hash array. This is needed so we only update modified
# attributes.
#
sub TIEHASH
{
  my $class = shift;
  my $self = {};

  return bless $self, $class;
}


#############################################################################
# Destructor, does nothing really...
#
#sub DESTROY
#{
#}


#############################################################################
# Store method, to keep track of changes.
#
sub STORE
{
  my ($self, $attr, $val) = ($_[$[], lc $_[$[ + 1], $_[$[ + 2]);

  return if (($val eq "") || ($attr eq ""));

  @{$self->{"_${attr}_save_"}} = @{$self->{$attr}}
    unless $self->{"_${attr}_save_"};
  $self->{$attr} = $val;
  $self->{"_${attr}_modified_"} = 1;

  # Potentially add the attribute to the OC order list.
  if (($attr ne "dn") && !grep(/^$attr$/, @{$self->{_oc_order_}}))
    {
      push(@{$self->{_oc_order_}}, $attr);
    }
}


#############################################################################
# Fetch method, this is case insensitive (since LDAP is...).
#
sub FETCH
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return unless defined($self->{$attr});

  return $self->{$attr};
}


#############################################################################
# Delete method, to keep track of changes.
#
sub DELETE
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return if ($attr eq "");
  return unless defined($self->{$attr});

  $self->{"_${attr}_deleted_"} = 1;
  undef $self->{$attr};
}


#############################################################################
# Mark an attribute as changed. Normally you shouldn't have to use this,
# unless you're doing something really weird...
#
sub attrModified
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 if ($attr eq "");
  return 0 unless defined($self->{$attr});

  @{$self->{"_${attr}_save_"}} = @{$self->{$attr}}
    unless $self->{"_${attr}_save_"};
  $self->{_self_obj_}->{"_${attr}_modified_"} = 1;

  return 1;
}


#############################################################################
# Ask if a particular attribute has been modified already. Return True or
# false depending on the internal status of the attribute.
#
sub isModified
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 if ($attr eq ""); return 0 unless defined($self->{$attr});
  return $self->{_self_obj_}->{"_${attr}_modified_"};
}


#############################################################################
# Remove an attribute from the entry, basically the same as the DELETE
# method. We also make an alias for "delete" here, just in case (and to be
# somewhat backward compatible).
#
sub remove
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 if ($attr eq "");
  return 0 unless defined($self->{$attr});

  $self->{_self_obj_}->{"_${attr}_deleted_"} = 1;
  undef $self->{_self_obj_}->{$attr};

  return 1;
}
*delete = \*remove;


#############################################################################
# Delete a value from an attribute, if it exists. NOTE: If it was the last
# value, we'll actually remove the entire attribute!
#
sub removeValue
{
  my ($self, $attr, $val) = ($_[$[], lc $_[$[ + 1], $_[$[ + 2]);
  my $i = 0;
  local $_;

  return 0 if (($val eq "") || ($attr eq ""));
  return 0 unless defined($self->{$attr});

  @{$self->{"_${attr}_save_"}} = @{$self->{$attr}}
    unless $self->{"_${attr}_save_"};
  foreach (@{$self->{$attr}})
    {
      if ($_ eq $val)
	{
	  splice(@{$self->{$attr}}, $i, 1);
	  $action = ($self->size($attr) > 0 ? "modified" : "deleted");
	  $self->{_self_obj_}->{"_${attr}_${action}_"} = 1;

	  return 1;
	}
      $i++;
    }

  return 0;
}
*deleteValue = \*removeValue;


#############################################################################
# Add a value to an attribute. The optional third argument indicates that
# we should not enforce the uniqueness on this attibute, thus bypassing
# the test and always add the value.
#
sub addValue
{
  my $self = shift;
  my ($attr, $val, $force) = (lc $_[$[], $_[$[ + 1], $_[$[ + 2]);
  local $_;

  return 0 if (($val eq "") || ($attr eq ""));
  if (!$force)
    {
      foreach (@{$self->{$attr}})
        {
          return 0 if ($_ eq $val);
        }
    }

  @{$self->{"_${attr}_save_"}} = @{$self->{$attr}}
    unless $self->{"_${attr}_save_"};

  $self->{_self_obj_}->{"_${attr}_modified_"} = 1;
  push(@{$self->{$attr}}, $val);

  return 1;
}


#############################################################################
# Return TRUE or FALSE, if the attribute has the specified value. The
# optional third argument says we should do case insensitive search.
#
sub hasValue
{
  my($self, $attr, $val, $nocase) = @_;

  return 0 if (($val eq "") || ($attr eq ""));
  return 0 unless defined($self->{$attr});
  return grep(/^\Q$val\E$/i, @{$self->{$attr}}) if $nocase;
  return grep(/^\Q$val\E$/, @{$self->{$attr}});
}


#############################################################################
# Return TRUE or FALSE, if the attribute matches the specified regexp. The
# optional third argument says we should do case insensitive search.
#
sub matchValue
{
  my($self, $attr, $reg, $nocase) = @_;

  return 0 if (($reg eq "") || ($attr eq ""));
  return 0 unless defined($self->{$attr});
  return grep(/$reg/i, @{$self->{$attr}}) if $nocase;
  return grep(/$reg/, @{$self->{$attr}});
}


#############################################################################
# Set the DN of this entry.
#
sub setDN
{
  my ($self, $val) = @_;

  return 0 if ($val eq "");

  $self->{dn} = $val;

  return 1;
}


#############################################################################
# Get the DN of this entry.
#
sub getDN
{
  my ($self) = @_;

  return "$self->{dn}";
}


#############################################################################
#
# Return the number of elements in an attribute.
#
sub size
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);
  my @val;

  return 0 if ($attr eq "");
  return 0 unless defined($self->{$attr});

  @val = @{$self->{$attr}};
  return $#val + 1;
}


#############################################################################
#
# Return TRUE if the attribute name is in the LDAP entry.
#
sub exists
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 if ($attr eq "");
  return defined($self->{$attr});
}


#############################################################################
# Print an entry, in LDIF format. This is idential to the Utils::printEntry
# function, but this is sort of neat... Note that the support for Base64
# encoding isn't finished.
#
sub printLDIF
{
  my ($self, $base64) = @_;
  my $attr;

  print "dn: ", $self->getDN(),"\n";
  foreach $attr (@{$self->{_oc_order_}})
    {
      next if ($attr =~ /^_.+_$/);
      next if $self->{"_${attr}_deleted_"};
      grep((print "$attr: $_\n"), @{$self->{$attr}});
    }

  print "\n";
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

  Mozilla::LDAP::Entry.pm - Object class to hold one LDAP entry.

=head1 SYNOPSIS

  use Mozilla::LDAP::Conn;
  use Mozilla::LDAP::Entry;

=head1 ABSTRACT

The LDAP::Conn object is used to perform LDAP searches, updates, adds and
deletes. All such functions works on LDAP::Entry objects only. All
modifications and additions you'll do to an LDAP entry, will be done
through this object class.

=head1 DESCRIPTION

The LDAP::Entry object class is built on top of the Tie::Hash standard
object class. This gives us several powerful features, the main one being
to keep track of what is changing in the LDAP entry. This makes it very
easy to write LDAP clients that needs to update/modify entries, since
you'll just do the changes, and this object class will take care of the
rest.

We define local functions for STORE, FETCH and DELETE in this object
class, and inherit the rest from the super class. Overloading these
specific functions is how we can keep track of what is changing in the
entry, which turns out to be very convenient.

Most of the methods here either return the requested LDAP value, or a
status code. The status code (either 0 or 1) indicates the failure or
success of a certain operation. 0 (False) meaning the operation failed,
and a return code of 1 (True) means complete success.

One thing to remember is that in LDAP, attribute names are case
insensitive. All methods in this class are aware of this, and will convert
all attribute name arguments to lower case before performing any
operations. This does not mean that the values are case insensitive. On
the contrary, all values are considered case sensitive by this module,
even if the LDAP server itself treats it as a CIS attribute.

=head1 OBJECT CLASS METHODS

The LDAP::Entry class implements many methods you can use to access and
modify LDAP entries. It is strongly recommended that you use this API as
much as possible, and avoid using the internals of the class
directly. Failing to do so may actually break the functionality.

=head2 Creating a new entry

To create a completely new entry, use the B<new> method, for instance

    $entry = new Mozilla::LDAP::Entry()
    $entry->setDN("uid=leif,ou=people,dc=netscape,dc=com");
    $entry->{objectclass} = [ "top", "person", "inetOrgPerson" ];
    $entry->addValue("cn", "Leif Hedstrom");
    $entry->addValue("sn", "Hedstrom");
    $entry->addValue("givenName", "Leif");
    $entry->addValue("mail", "leif@netscape.com);

    $conn->add($entry);

This is the minimum requirements for an LDAP entry. It must have a DN, and
it must have at least one objectclass. As it turns out, by adding the
I<person> and I<inetOrgPerson> classes, we also must provide some more
attributes, like I<CN> and I<SN>. This is because the object classes have
these attributes marked as "required", and we'd get a schema violation
without those values.

In the example above we use both native API methods to add values, and
setting an attribute entire value set directly. Note that the value set is
a pointer to an array, and not the array itself. In the example above, the
object classes are set using an anonymous array, which the API handles
properly. It's important to be aware that the attribute value list is
indeed a pointer.

Finally, as you can see there's only only one way to add new LDAP entries,
and it's called add(). It normally takes an LDAP::Entry object instance as
argument, but it can also be called with a regular hash array if so
desired.

=head2 Adding and removing attributes and values

This is the main functionality of this module. Use these methods to do any
modifications and updates to your LDAP entries.

=over 13

=item B<attrModified>

This is an internal function, that can be used to force the API to
consider an attribute (value) to have been modified. The only argument is
the name of the attribute. In almost all situation, you never, ever,
should call this. If you do, please contact the developers, and as us to
fix the API.

=item B<isModified>

This is a somewhat more useful method, which will return the internal
modification status of a particular attribute. The argument is the name of
the attribute, and the return value is True or False. If the attribute has
been modified, in any way, we return True (1), otherwise we return False
(0).

=item B<remove>

This will remove the entire attribute, including all it's values, from the
entry. The only argument is the name of the attribute to remove. Let's say
you want to nuke all I<mailAlternateAddress> values (i.e. the entire
attribute should be removed from the entry):

    $entry->remove("mailAlternateAddress");

=item B<removeValue>

Remove a value from an attribute, if it exists. Of course, if the
attribute has no such value, we won't try to remove it, and instead return
a False (0) status code. The arguments are the name of the attribute, and
the particular value to remove. Note that values are considered case
sensitive, so make sure you preserve case properly. An example is:

    $entry->removeValue("objectclass", "nscpPerson");

=item B<addValue>

Add a value to an attribute. If the attribute value already exists, or we
couldn't add the value for any other reason, we'll return FALSE (0),
otherwise we return TRUE (1). The first two arguments are the attribute
name, and the value to add.

The optional third argument is a flag, indicating that we want to add the
attribute without checking for duplicates. This is useful if you know the
values are unique already, or if you perhaps want to allow duplicates for
a particular attribute.

=item B<hasValue>

Return TRUE or FALSE if the attribute has the specified value. A typical
usage is to see if an entry is of a certain object class, e.g.

    if ($entry->hasValue("objectclass", "person", 1)) { # do something }

The (optional) third argument indicates if the string comparison should be
case insensitive or not. The first two arguments are the name and value of
the attribute, as usual.

=item B<matchValue>

This is very similar to B<hasValue>, except it does a regular expression
match instead of a full string match. It takes the same arguments,
including the optional third argument to specify case insensitive
matching.

=item B<setDN>

Set the DN to the specified value. Only do this on new entries, it will
not work well if you try to do this on an existing entry. If you wish to
renamed an entry, use the Mozilla::Conn::modifyRDN method instead.
Eventually we'll provide a complete "rename" method.

=item B<getDN>

Return the DN for the entry.

=item B<size>

Return the number of values for a particular attribute. For instance

    $entry->{cn} = [ "Leif Hedstrom", "The Swede" ];
    $numVals = $entry->size("cn");

This will set C<$numVals> to two (2). The only argument is the name of the
attribute, and the return value is the size of the value array.

=item B<exists>

Return TRUE if the specified attribute is defined in the LDAP entry.

=item B<printLDIF>

Print the entry (on STDOUT) in a format called LDIF (LDAP Data Interchange
Format, RFC xxxx). An example of an LDIF entry is:

    dn: uid=leif,ou=people,dc=netscape,dc=com
    objectclass: top
    objectclass: person
    objectclass: inetOrgPerson
    uid: leif
    cn: Leif Hedstrom
    mail: leif@netscape.com

If you need to write to a file, close STDOUT, and open up a file with that
file handle instead. For more useful LDIF functionality, check out the
Mozilla::LDAP::LDIF.pm module.

=back

=head2 Deleting entries

To delete an LDAP entry from the LDAP server, you have to use the
B<delete> method from the Mozilla::LDAP::Conn module. It will actually
delete any entry, if you provide an legitimate DN.

=head2 Renaming entries

Again, there's no functionality in this object class to rename the entry
(i.e. changing it's DN). For now, there is a way to modify the RDN
component of a DN through the Mozilla::LDAP::Conn module, with
B<modifyRDN>. Eventually we hope to have a complete B<rename> method,
which should be capable of renaming any entry, in any way, including
moving it to a different part of the DIT (Directory Information Tree).

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

L<Mozilla::LDAP::Conn>, L<Mozilla::LDAP::API>, and of course L<Perl>.

=cut
