#############################################################################
# $Id: Entry.pm,v 1.12 1999/08/24 22:30:44 leif%netscape.com Exp $
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
#    This package defines an object class to manage one single LDAP
#    entry. This entry can either be a newly created one, or one
#    retrieved from an LDAP server, using the Mozilla::LDAP::Conn class.
#
#############################################################################

package Mozilla::LDAP::Entry;

use Mozilla::LDAP::Utils 1.4 qw(normalizeDN);
use Tie::Hash;

use strict;
use vars qw($VERSION @ISA);

@ISA = ('Tie::StdHash');
$VERSION = "1.4";


#############################################################################
# Constructor, for convenience.
#
sub new
{
  my ($class) = shift;
  my (%entry, $obj);

  tie %entry, $class;
  $obj = bless \%entry, $class;

  return $obj;
}


#############################################################################
# Creator, make a new tie hash instance, which will keep track of all
# changes made to the hash array. This is needed so we only update modified
# attributes.
#
sub TIEHASH
{
  my ($class, $self) = (shift, {});

  return bless $self, $class;
}


#############################################################################
# Destructor, free a bunch of memory. This makes a lot more sense now,
# since apparently Perl does not handle self references properly within an
# object(??).
#
sub DESTROY
{
  my ($self) = shift;

  undef %{$self};
  undef $self;
}


#############################################################################
# Store method, to keep track of changes on an entire array of values (per
# attribute, of course).
#
sub STORE
{
  my ($self, $attr, $val) = ($_[$[], lc $_[$[ + 1], $_[$[ + 2]);

  return unless (defined($val) && ($val ne ""));
  return unless (defined($attr) && ($attr ne ""));

  # We don't "track" internal values, or DNs...
  if (($attr =~ /^_.+_$/) || ($attr eq "dn"))
    {
      $self->{$attr} = $val;
      return;
    }

  if (defined($self->{$attr}))
    {
      $self->{"_${attr}_save_"} = [ @{$self->{$attr}} ]
        unless defined($self->{"_${attr}_save_"});
    }

  $self->{$attr} = $val;
  $self->{"_${attr}_modified_"} = 1;
  delete $self->{"_${attr}_deleted_"}
    if defined($self->{"_${attr}_deleted_"});

  # Potentially add the attribute to the OC order list.
  if (! grep(/^$attr$/i, @{$self->{"_oc_order_"}}))
    {
      push(@{$self->{"_oc_order_"}}, $attr);
      $self->{"_oc_numattr_"}++;
    }
}


#############################################################################
# Fetch method, this is case insensitive (since LDAP is...).
#
sub FETCH
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return unless defined($self->{$attr});
  return if defined($self->{"_${attr}_deleted_"});

  return $self->{$attr};
}


#############################################################################
# Delete method, to keep track of changes. Note that we actually don't
# delete the attribute, just mark it as deleted.
#
sub DELETE
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return unless (defined($attr) && ($attr ne ""));
  return unless defined($self->{$attr});

  if ($attr =~ /^_.+_$/)
    {
      delete $self->{$attr};
    }
  else
    {
      $self->{"_${attr}_deleted_"} = 1;
    }
}


#############################################################################
# See if an attribute/key exists in the entry (could still be undefined).
# The exists() (lowercase) is a kludge, kept for backward compatibility.
# Please use the EXISTS method (or just exists ... instead).
#
sub EXISTS
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 if defined($self->{"_${attr}_deleted_"});

  return exists $self->{$attr};
}


sub exists
{
  my ($self, $attr) = @_;

  return exists $self->{$attr};
}


#############################################################################
# Reset the each()/key() session, and return the first key. This honors
# the oc_order, i.e. the order the attributes were returned in.
#
sub FIRSTKEY
{
  my ($self, $idx) = ($_[$[], 0);
  my (@attrs, $key);

  return unless defined($self->{"_oc_order_"});

  @attrs =  @{$self->{"_oc_order_"}};
  while ($idx < $self->{"_oc_numattr_"})
    {
      $key = $attrs[$idx++];
      next if ($key =~ /^_.+_$/);
      next if defined($self->{"_${key}_deleted_"});
      last;
    }
  $self->{"_oc_keyidx_"} = $idx;

  return $key;
}


#############################################################################
# Get the next key, if appropriate.
#
sub NEXTKEY
{
  my ($self) = $_[$[];
  my ($idx) = $self->{"_oc_keyidx_"};
  my (@attrs, $key);

  return unless defined($self->{"_oc_order_"});

  @attrs = @{$self->{"_oc_order_"}};
  while ($idx < $self->{"_oc_numattr_"})
    {
      $key = $attrs[$idx++];
      next if ($key =~ /^_.+_$/);
      next if defined($self->{"_${key}_deleted_"});
      last;
    }
  $self->{"_oc_keyidx_"} = $idx;

  return unless (defined($key) && ($key ne ""));
  return if ($key =~ /^_.+_$/);
  return if defined($self->{"_${key}_deleted_"});
  return $key;
}


#############################################################################
# Mark an attribute as changed. Normally you shouldn't have to use this,
# unless you're doing something really weird...
#
sub attrModified
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});
  return 0 if defined($self->{"_${attr}_deleted_"});
  return 0 if ($attr eq "dn");

  $self->{"_${attr}_save_"} = [ @{$self->{$attr}} ]
    unless defined($self->{"_${attr}_save_"});
  $self->{"_${attr}_modified_"} = 1;
  delete $self->{"_${attr}_deleted_"}
    if defined($self->{"_${attr}_deleted_"});


  return 1;
}
*markModified = \*attrModified;


#############################################################################
# Mark an attribute as "clean", meaning nothing has been changed in it.
# You should probably not use this method, unless you really know what
# you are doing... It is however used heavily by the Conn.pm package.
#
sub attrClean
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 if ($attr eq "dn");

  delete $self->{"_${attr}_modified_"}
    if defined($self->{"_${attr}_modified_"});

  delete $self->{"_${attr}_deleted_"}
    if defined($self->{"_${attr}_deleted_"});

  if (defined($self->{"_${attr}_save_"}))
    {
      undef @{$self->{"_${attr}_save_"}};
      delete $self->{"_${attr}_save_"}; 
    }
}


#############################################################################
# Ask if a particular attribute has been modified already. Return True or
# false depending on the internal status of the attribute.
#
sub isModified
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});
  return 0 unless defined($self->{"_${attr}_modified_"});

  return 1;
}


#############################################################################
# Ask if a particular attribute has been deleted already. Return True or
# false depending on the internal status of the attribute.
#
sub isDeleted
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});
  return 0 unless defined($self->{"_${attr}_deleted_"});

  return 1;
}


#############################################################################
# Test if a attribute name is actually a real attribute, and not part of
# the internal structures.
#
sub isAttr
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});
  return 0 if defined($self->{"_${attr}_deleted_"});

  return ($attr !~ /^_.+_$/);
}


#############################################################################
# Remove an attribute from the entry, basically the same as the DELETE
# method. We also make an alias for "delete" here, just in case (and to be
# somewhat backward compatible).
#
sub remove
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});

  $self->{"_${attr}_deleted_"} = 1;

  return 1;
}
*delete = \*remove;


#############################################################################
# Move (rename) an attribute, return TRUE or FALSE depending on the outcome.
# The first argument is the name of the old attribute (e.g. CN), and the last
# argument is the new name (e.g. SN). Note that the "new" attribute can not
# already exist, and the old attribute must exist.
#
# The "force" argument can be used to override the check if the new
# attribute already exists. This is potentially dangerous.
#
sub move
{
  my ($self, $old, $new, $force) = ($_[$[], lc $_[$[ + 1], lc $_[$[ + 2],
				    $_[$[ + 3]);

  return 0 if ($self->isAttr($new) && (!defined($force) || !$force));
  return 0 unless $self->isAttr($old);

  $self->setValues($new, @{$self->{$old}}) || return 0;
  $self->remove($old);

  return 1;
}
*rename = \*move;


#############################################################################
# Copy an attribute, return TRUE or FALSE depending on the outcome. This
# is almost identical to the move method, except we don't delete the source.
#
sub copy
{
  my ($self, $old, $new, $force) = ($_[$[], lc $_[$[ + 1], lc $_[$[ + 2],
				    $_[$[ + 3]);

  return 0 if ($self->isAttr($new) && (!defined($force) || !$force));
  return 0 unless $self->isAttr($old);

  $self->setValues($new, @{$self->{$old}}) || return 0;

  return 1;
}


#############################################################################
# Undo a remove(), or set of removeValues() fairly useless, to restore an
# attribute to it's original state. This is fairly useless, but hey...
#
sub unRemove
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});
  return 0 if ($attr eq "dn");

  # ToDo: We need to verify that this sucker works...
  delete $self->{"_${attr}_deleted_"};
  if (defined($self->{"_${attr}_save_"}))
    {
      undef @{$self->{$attr}};
      delete $self->{$attr};
      $self->{$attr} = [ @{$self->{"_${attr}_save_"}} ];
      undef @{$self->{"_${attr}_save_"}};
      delete $self->{"_${attr}_save_"};
    }

  return 1;
}
*unDelete = \*unRemove;


#############################################################################
# Delete a value from an attribute, if it exists. NOTE: If it was the last
# value, we'll actually remove the entire attribute! We should then also
# remove it from the _oc_order_ list...
#
sub removeValue
{
  my ($self, $attr, $val, $norm) = ($_[$[], lc $_[$[ + 1], $_[$[ + 2],
				    $_[$[ + 3]);
  my ($i) = 0;
  my ($attrval);
  local $_;

  return 0 unless (defined($val) && ($val ne ""));
  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});
  return 0 if ($attr eq "dn");

  $val = normalizeDN($val) if (defined($norm) && $norm);
  $self->{"_${attr}_save_"} = [ @{$self->{$attr}} ] unless
    defined($self->{"_${attr}_save_"});

  foreach $attrval (@{$self->{$attr}})
    {
      $_ = ((defined($norm) && $norm) ? normalizeDN($attrval) : $attrval);
      if ($_ eq $val)
	{
	  splice(@{$self->{$attr}}, $i, 1);
	  if ($self->size($attr) > 0)
	    {
	      $self->{"_${attr}_modified_"} = 1;
	    }
	  else
	    {
	      $self->{"_${attr}_deleted_"} = 1;
	    }

	  return 1;
	}
      $i++;
    }

  return 0;
}
*deleteValue = \*removeValue;


#############################################################################
# Just like removeValue(), but force the DN normalization of the value.
#
sub removeDNValue
{
  my ($self, $attr, $val) = ($_[$[], lc $_[$[ + 1], $_[$[ + 2]);

  return $self->removeValue($attr, $val, 1);
}
*deleteDNValue = \*removeDNValue;


#############################################################################
# Add a value to an attribute. The optional third argument indicates that
# we should not enforce the uniqueness on this attibute, thus bypassing
# the test and always add the value.
#
sub addValue
{
  my ($self) = shift;
  my ($attr, $val, $force, $norm) = (lc $_[$[], $_[$[ + 1], $_[$[ + 2],
				     $_[$[ + 3]);
  my ($attrval);
  local $_;

  return 0 unless (defined($val) && ($val ne ""));
  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 if ($attr eq "dn");

  if (defined($self->{$attr}) && (!defined($force) || !$force))
    {
      my ($nval) = $val;

      $nval = normalizeDN($val) if (defined($norm) && $norm);
      foreach $attrval (@{$self->{$attr}})
        {
	  $_ = ((defined($norm) && $norm) ? normalizeDN($attrval) : $attrval);
          return 0 if ($_ eq $nval);
        }
    }

  if (defined($self->{$attr}))
    {
      $self->{"_${attr}_save_"} = [ @{$self->{$attr}} ]
        unless defined($self->{"_${attr}_save_"});
    }
  else
    {
      $self->{"_${attr}_save_"} = []
        unless defined($self->{"_${attr}_save_"});
    }

  $self->{"_${attr}_modified_"} = 1;
  if (defined($self->{"_${attr}_deleted_"}))
    {
      delete $self->{"_${attr}_deleted_"};
      $self->{$attr} = [$val];
    }
  else
    {
      push(@{$self->{$attr}}, $val);
    }

  # Potentially add the attribute to the OC order list.
  if (! grep(/^$attr$/i, @{$self->{"_oc_order_"}}))
    {
      push(@{$self->{"_oc_order_"}}, $attr);
      $self->{"_oc_numattr_"}++;
    }

  return 1;
}


#############################################################################
# Just like addValue(), but force the DN normalization of the value. Note
# that we also have an $norm argument here, to normalize the DN value
# before we add it.
#
sub addDNValue
{
  my ($self) = shift;
  my ($attr, $val, $force, $norm) = (lc $_[$[], $_[$[ + 1], $_[$[ + 2],
				     $_[$[ + 3]);

  $val = normalizeDN($val) if (defined($norm) && $norm);
  return $self->addValue($attr, $val, $force, 1);
}


#############################################################################
# Set the entire value of an attribute, removing whatever was already set.
# The arguments are the name of the attribute, and then one or more values,
# passed as scalar or an array (not pointer).
#
sub setValues
{
  my ($self, $attr) = (shift, lc shift);
  my (@vals) = @_;

  local $_;

  return 0 unless (defined(@vals) && ($#vals >= $[));
  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 if ($attr eq "dn");

  if (defined($self->{$attr}))
    {
      $self->{"_self_obj_"}->{"_${attr}_save_"} = [ @{$self->{$attr}} ]
	unless defined($self->{"_${attr}_save_"});
    }
  else
    {
      $self->{"_self_obj_"}->{"_${attr}_save_"} = [ ]
	unless defined($self->{"_${attr}_save_"});
    }

  $self->{$attr} = [ @vals ];
  $self->{"_${attr}_modified_"} = 1;

  delete $self->{"_${attr}_deleted_"}
    if defined($self->{"_${attr}_deleted_"});

  if (! grep(/^$attr$/i, @{$self->{"_oc_order_"}}))
    {
      push(@{$self->{"_oc_order_"}}, $attr);
      $self->{"_oc_numattr_"}++;
    }

  return 1;
}
*setValue = \*setValues;


#############################################################################
# Get the entire array of attribute values. This returns the array, not
# the pointer to the array...
#
sub getValues
{
  my ($self, $attr) = (shift, lc shift);

  return unless (defined($attr) && ($attr ne ""));
  return unless defined($self->{$attr});

  return @{$self->{$attr}};
}
*getValue = \*getValues;


#############################################################################
# Return TRUE or FALSE, if the attribute has the specified value. The
# optional third argument says we should do case insensitive search.
#
sub hasValue
{
  my ($self, $attr, $val, $nocase, $norm) = @_;
  my ($attrval);
  local $_;

  return 0 unless (defined($val) && ($val ne ""));
  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});

  $val = normalizeDN($val) if (defined($norm) && $norm);
  if ($nocase)
    {
      foreach $attrval (@{$self->{$attr}})
	{
	  $_ = ((defined($norm) && $norm) ? normalizeDN($attrval) : $attrval);
	  return 1 if /^\Q$val\E$/i;
	}
    }
  else
    {
      foreach $attrval (@{$self->{$attr}})
	{
	  $_ = ((defined($norm) && $norm) ? normalizeDN($attrval) : $attrval);
	  return 1 if /^\Q$val\E$/;
	}
    }

  return 0;
}


#############################################################################
# Just like hasValue(), but force the DN normalization of the value.
#
sub hasDNValue
{
  my ($self, $attr, $val, $nocase) = @_;

  return $self->hasValue($attr, $val, $nocase, 1);
}


#############################################################################
# Return TRUE or FALSE, if the attribute matches the specified regexp. The
# optional third argument says we should do case insensitive search, and the
# optional fourth argument indicates we should normalize for DN matches.
#
sub matchValue
{
  my ($self, $attr, $reg, $nocase, $norm) = @_;
  my ($attrval);

  return 0 unless (defined($reg) && ($reg ne ""));
  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});

  if ($nocase)
    {
      foreach $attrval (@{$self->{$attr}})
	{
	  $_ = ((defined($norm) && $norm) ? normalizeDN($attrval) : $attrval);
	  return 1 if /$reg/i;
	}
    }
  else
    {
      foreach $attrval (@{$self->{$attr}})
	{
	  $_ = ((defined($norm) && $norm) ? normalizeDN($attrval) : $attrval);
	  return 1 if /$reg/;
	}
    }

  return 0;
}


#############################################################################
# Just like matchValue(), but force the DN normalization of the values.
#
sub matchDNValue
{
  my ($self, $attr, $reg, $nocase) = @_;

  return $self->matchValue($attr, $reg, $nocase, 1);
}


#############################################################################
# Set the DN of this entry.
#
sub setDN
{
  my ($self, $val, $norm) = @_;

  return 0 unless (defined($val) && ($val ne ""));

  $val = normalizeDN($val) if (defined($norm) && $norm);
  $self->{"dn"} = $val;

  return 1;
}


#############################################################################
# Get the DN of this entry.
#
sub getDN
{
  my ($self, $norm) = @_;

  return normalizeDN($self->{"dn"}) if (defined($norm) && $norm);
  return $self->{"dn"};
}


#############################################################################
#
# Return the number of elements in an attribute.
#
sub size
{
  my ($self, $attr) = ($_[$[], lc $_[$[ + 1]);

  return 0 unless (defined($attr) && ($attr ne ""));
  return 0 unless defined($self->{$attr});

  return scalar(@{$self->{$attr}});
}


#############################################################################
#
# Return LDIF entries.
#
sub getLDIFrecords # called from LDIF.pm (at least)
{
  my ($self) = @_;
  my (@record) = (dn => $self->getDN());
  my ($attr, $values);

  while (($attr, $values) = each %$self)
    {
      next if "dn" eq lc $attr; # this shouldn't happen; should it?
      push @record, ($attr => $values);
      # This is dangerous: @record and %$self now both contain
      # references to @$values.  To avoid this, copy it:
      # push @record, ($attr => [@$values]);
      # But that's not necessary, because the array and its
      # contents are not modified as a side-effect of getting
      # other attributes, from this or other objects.
    }

  return \@record;
}


#############################################################################
# Print an entry, in LDIF format.
#
use vars qw($_no_LDIF_module $_tested_LDIF_module);
undef $_no_LDIF_module;
undef $_tested_LDIF_module;

sub printLDIF
{
  my ($self) = @_;


  if (not defined($_tested_LDIF_module))
    {
      eval {require Mozilla::LDAP::LDIF; Mozilla::LDAP::LDIF->VERSION(0.07)};
      $_no_LDIF_module = $@;
      $_tested_LDIF_module = 1;
    }

  if ($_no_LDIF_module)
    {
      my ($record) = $self->getLDIFrecords();
      my ($attr, $values);

      while (($attr, $values) = splice @$record, 0, 2)
	{
	  grep((print "$attr: $_\n"), @$values);
	}
      print "\n";
    }
  else
    {
      Mozilla::LDAP::LDIF::put_LDIF(select(), 78, $self);
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

We define local functions for STORE, FETCH, DELETE, EXISTS, FIRSTKEY and
NEXTKEY in this object class, and inherit the rest from the super
class. Overloading these specific functions is how we can keep track of
what is changing in the entry, which turns out to be very convenient. We
can also easily "loop" over the attribute types, ignoring internal data,
or deleted attributes.

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

=item B<addDNValue>

Just like B<addValue>, except this method assume the value is a DN
attribute. For instance

   $dn = "uid=Leif, dc=Netscape, dc=COM";
   $entry->addDNValue("uniqueMember", $dn);


will only add the DN for "uid=leif" if it does not exist as a DN in the
uniqueMember attribute.

=item B<addValue>

Add a value to an attribute. If the attribute value already exists, or we
couldn't add the value for any other reason, we'll return FALSE (0),
otherwise we return TRUE (1). The first two arguments are the attribute
name, and the value to add.

The optional third argument is a flag, indicating that we want to add the
attribute without checking for duplicates. This is useful if you know the
values are unique already, or if you perhaps want to allow duplicates for
a particular attribute. To add a CN to an existing entry/attribute, do:

    $entry->addValue("cn", "Leif Hedstrom");

=item B<attrModified>

This is an internal function, that can be used to force the API to
consider an attribute (value) to have been modified. The only argument is
the name of the attribute. In almost all situation, you never, ever,
should call this. If you do, please contact the developers, and as us to
fix the API. Example

    $entry->attrModified("cn");

=item B<copy>

Copy the value of one attribute to another.  Requires at least two
arguments.  The first argument is the name of the attribute to copy, and
the second argument is the name of the new attribute to copy to.  The new
attribute can not currently exist in the entry, else the copy will fail.
There is an optional third argument (a boolean flag), which, when set to
1, will force an
override and copy to the new attribute even if it already exists.  Returns TRUE if the copy
was successful.

    $entry->copy("cn", "description");

=item B<exists>

Return TRUE if the specified attribute is defined in the LDAP entry. This
is useful to know if an entry has a particular attribute, regardless of
the value. For instance:

    if ($entry->exists("jpegphoto")) { # do something special }

=item B<getDN>

Return the DN for the entry. For instance

    print "The DN is: ", $entry->getDN(), "\n";

Just like B<setDN>, this method also has an optional argument, which
indicates we should normalize the DN before returning it to the caller.

=item B<getValues>

Returns an entire array of values for the attribute specified.  Note that
this returns an array, and not a pointer to an array.

    @someArray = $entry->getValues("description");

=item B<hasValue>

Return TRUE or FALSE if the attribute has the specified value. A typical
usage is to see if an entry is of a certain object class, e.g.

    if ($entry->hasValue("objectclass", "person", 1)) { # do something }

The (optional) third argument indicates if the string comparison should be
case insensitive or not, and the (optional) fourth argument indicats
wheter we should normalize the string as if it was a DN. The first two
arguments are the name and value of the attribute, respectively.

=item B<hasDNValue>

Exactly like B<hasValue>, except we assume the attribute values are DN
attributes.

=item B<isAttr>

This method can be used to decide if an attribute name really is a valid
LDAP attribute in the current entry. Use of this method is fairly limited,
but could potentially be useful. Usage is like previous examples, like

    if ($entry->isAttr("cn")) { # do something }

The code section will only be executed if these criterias are true:

    1. The name of the attribute is a non-empty string.
    2. The name of the attribute does not begin, and end, with an
       underscore character (_).
    2. The attribute has one or more values in the entry.

=item B<isDeleted>

This is almost identical to B<isModified>, except it tests if an attribute
has been deleted. You use it the same way as above, like

    if (! $entry->isDeleted("cn")) { # do something }

=item B<isModified>

This is a somewhat more useful method, which will return the internal
modification status of a particular attribute. The argument is the name of
the attribute, and the return value is True or False. If the attribute has
been modified, in any way, we return True (1), otherwise we return False
(0). For example:

    if ($entry->isModified("cn")) { # do something }

=item B<matchValue>

This is very similar to B<hasValue>, except it does a regular expression
match instead of a full string match. It takes the same arguments,
including the optional third argument to specify case insensitive
matching. The usage is identical to the example for hasValue, e.g.

    if ($entry->matchValue("objectclass", "pers", 1)) { # do something }

=item B<matchDNValue>

Like B<matchValue>, except the attribute values are considered being DNs.

=item B<move>

Identical to the copy method, except the original attribute is
deleted once the move to the new attribute is complete.

    $entry->move("cn", "sn");

=item B<printLDIF>

Print the entry in a format called LDIF (LDAP Data Interchange
Format, RFC xxxx). An example of an LDIF entry is:

    dn: uid=leif,ou=people,dc=netscape,dc=com
    objectclass: top
    objectclass: person
    objectclass: inetOrgPerson
    uid: leif
    cn: Leif Hedstrom
    mail: leif@netscape.com

The above would be the result of

    $entry->printLDIF();

If you need to write to a file, open and then select() it.
For more useful LDIF functionality, check out the
Mozilla::LDAP::LDIF.pm module.

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

=item B<removeDNValue>

This is almost identical to B<removeValue>, except it will normalize the
attribute values before trying to remove them. This is useful if you know
that the attribute is a DN value, but perhaps the values are not cosistent
in all LDAP entries. For example

   $dn = "uid=Leif, dc=Netscape, dc=COM";
   $entry->removeDNValue("owner", $dn);


will remove the owner "uid=leif,dc=netscape,dc=com", no matter how it's
capitalized and formatted in the entry.

=item B<setDN>

Set the DN to the specified value. Only do this on new entries, it will
not work well if you try to do this on an existing entry. If you wish to
rename an entry, use the Mozilla::Conn::modifyRDN method instead.
Eventually we'll provide a complete "rename" method. To set the DN for a
newly created entry, we can do

    $entry->setDN("uid=leif,ou=people,dc=netscape,dc=com");

There is an optional third argument, a boolean flag, indicating that we
should normalize the DN before setting it. This will assure a consistent
format of your DNs.

=item B<setValues>

Set the specified attribute to the new value (or values), overwriting
whatever old values it had before. This is a little dangerous, since you
can lose attribute values you didn't intend to remove. Therefore, it's
usually recommended to use B<removeValue()> and B<setValues()>. If you know
exactly what the new values should be like, you can use this method like

    $entry->setValues("cn", "Leif Hedstrom", "The Swede");
    $entry->setValues("mail", @mailAddresses);

or if it's a single value attribute,

    $entry->setValues("uidNumber", "12345");

=item B<size>

Return the number of values for a particular attribute. For instance

    $entry->{cn} = [ "Leif Hedstrom", "The Swede" ];
    $numVals = $entry->size("cn");

This will set C<$numVals> to two (2). The only argument is the name of the
attribute, and the return value is the size of the value array.

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

    http://www.mozilla.org/directory/
    Your local CPAN server

=head1 CREDITS

Most of this code was developed by Leif Hedstrom, Netscape Communications
Corporation. 

=head1 BUGS

None. :)

=head1 SEE ALSO

L<Mozilla::LDAP::Conn>, L<Mozilla::LDAP::API>, and of course L<Perl>.

=cut
