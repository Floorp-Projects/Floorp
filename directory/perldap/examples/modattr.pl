#!/usr/bin/perl5
#############################################################################
# $Id: modattr.pl,v 1.8 1999/01/21 23:52:46 leif%netscape.com Exp $
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
#    This script can be used to do a number of different modification
#    operations on a script. Like adding/deleting values, or entire
#    attributes.
#
#############################################################################

use Getopt::Std;			# To parse command line arguments.
use Mozilla::LDAP::Conn;		# Main "OO" layer for LDAP
use Mozilla::LDAP::Utils;		# LULU, utilities.

use strict;
no strict "vars";


#############################################################################
# Constants, shouldn't have to edit these...
#
$APPNAM	= "modattr";
$USAGE	= "$APPNAM [-dnvW] -b base -h host -D bind -w pswd -P cert attr=value filter";


#############################################################################
# Check arguments, and configure some parameters accordingly..
#
if (!getopts('adnvWb:h:D:p:s:w:P:'))
{
  print "usage: $APPNAM $USAGE\n";
  exit;
}
%ld = Mozilla::LDAP::Utils::ldapArgs();
Mozilla::LDAP::Utils::userCredentials(\%ld) unless $opt_n;


#############################################################################
# Let's process the changes requested, and commit them unless the "-n"
# option was given.
#
$conn = new Mozilla::LDAP::Conn(\%ld);
die "Could't connect to LDAP server $ld{host}" unless $conn;

$conn->setDefaultRebindProc($ld{bind}, $ld{pswd});

($change, $search) = @ARGV;
if (($change eq "") || ($search eq ""))
{
  print "usage: $APPNAM $USAGE\n";
  exit;
}
($attr, $value) = split(/=/, $change, 2);

$entry = $conn->search($ld{root}, $ld{scope}, $search);
while ($entry)
{
  $changed = 0;

  if ($opt_d)
    {
      if (defined $entry->{$attr})
	{
	  if ($value)
	    {
	      $changed = $entry->removeValue($attr, $value);
	      if ($changed && $opt_v)
		{
		  print "Removed value from ", $entry->getDN(), "\n" if $opt_v;
		}
	    }
	  else
	    {
	      delete $entry->{$attr};
	      print "Deleted attribute $attr for ", $entry->getDN(), "\n" if $opt_v;
	      $changed = 1;
	    }
	}
      else
	{
	  print "No attribute values for: $attr\n";
	}
    }
  else
    {
      if (!defined($value) || !$value)
	{
	  print "No value provided for the attribute $attr\n";
	}
      elsif ($opt_a)
	{
	  $changed = $entry->addValue($attr, $value);
	  if ($changed && $opt_v)
	    {
	      print "Added attribute to ", $entry->getDN(), "\n" if $opt_v;
	    }
	}
      else
	{
	  $entry->setValue($attr, $value);
	  $changed = 1;
	  print "Set attribute for ", $entry->getDN(), "\n" if $opt_v;
	}
    }
  if ($changed && ! $opt_n)
    {
      $conn->update($entry);
      $conn->printError() if $conn->getErrorCode();
    }

  $entry = $conn->nextEntry();
}


#############################################################################
# Close the connection.
#
$conn->close if $conn;


#############################################################################
# POD documentation...
#
__END__

=head1 NAME

  modattr - Modify an attribute for one or more LDAP entries

=head1 SYNOPSIS

  modattr [-adnvW] -b base -h host -D bind DN -w pwd -P cert attr=value filter

=head1 ABSTRACT

This command line utility can be used to modify one attribute for one or
more LDAP entries. As simple as this sounds, this turns out to be a very
common operation. For instance, let's say you want to change "mailHost"
for all users on a machine named I<dredd>, to be I<judge>. With this
script all you have to do is

    modattr mailHost=judge '(mailHost=dredd)'

=head1 DESCRIPTION

There are four primary operations that can be made with this utility:

=over 4

=item *

Set an attribute to a (single) specified value.

=item *

Add a value to an attribute (for multi-value attributes).

=item *

Delete a value from an attribute. If it's the last value (or if it's a
single value), this will remove the entire attribute.

=item *

Delete an entire attribute, even if it has multiple values.

=back

The first three requires an option of the form B<attr=value>, while the
last one only takes the name of the attribute as the option. The last
argument is always an LDAP search filter, specifying which entries the
operation should be applied to.

=head1 OPTIONS

All but the first two command line options for this tool are standard LDAP
options, to set parameters for the LDAP connection. The two new options
are I<-a> and I<-d> to add and remove attribute values.

Without either of these two options specified (they are both optional),
the default action is to set the attribute to the specified value. That
will effectively remove any existing values for this attribute.

=over 12

=item -a

Specify that the operation is an I<add>, to add a value to the
attribute. If there is no existing value for this attribute, we'll create
a new attribute, otherwise we add the new value if it's not already there.

=item -d

Delete the attribute value, or the entire attribute if there's no value
specified. As you can see this option has two forms, and it's function
depends on the last arguments. Be careful here, if you forget to specify
the value to delete, you will remove all of them.

=item -h <host>

Name of the LDAP server to connect to.

=item -p <port>

TCP port for the LDAP connection.

=item -b <DN>

Base DN for the search

=item -D <bind>

User (DN) to bind as. We support a few convenience shortcuts here, like
I<root>, I<user> and I<repl>.

=item -w <passwd>

This specifies the password to use when connecting to the LDAP
server. This is strongly discouraged, and without this option the script
will ask for the password interactively.

=item -s <scope>

Search scope, default is I<sub>, the other possible values are I<base> and
I<one>. You can also specify the numeric scopes, I<0>, I<1> or I<2>.

=item -P

Use SSL for the LDAP connection, using the specified cert.db file for
certificate information.

=item -n

Don't do anything, only show the changes that would have been made. This
is very convenient, and can save you from embarrassing mistakes.

=item -v

Verbose output.

=back

The last two arguments are special for this script. The first
argument specifies the attribute (and possibly the value) to operate on,
and the last argument is a properly formed LDAP search filter.

=head1 EXAMPLES

We'll give one example for each of the four operations this script can
currently handle. Since the script itself is quite flexible, you'll
probably find you can use this script for a lot of other applications, or
call it from other scripts. Note that we don't specify any LDAP specific
options here, we assume you have configured your defaults properly.

To set the I<description> attribute for user "leif", you would do

    modattr 'description=Company Swede' '(uid=leif)'

The examples shows how to use this command without either of the I<-a> or
the I<-d> argument. To add an e-mail alias (alternate address) to the same
user, you would do

    modattr -a 'mailAlternateAddress=theSwede@netscape.com' '(uid=leif)'

To remove an object class from all entries which uses it, you could do

    modattr -d 'objectclass=dummyClass' '(objectclass=dummyClass)'

This example is not great, since unless you've assured that no entries
uses any of the attributes in this class, you'll get schema
violations. But don't despair, you can use this tool to clean up all
entries first! To completely remove all usage of an attribute named
I<dummyAttr>, you'd simply do

    modattr -d dummyAttr '(dummyAttr=*)'

This shows the final format of this command, notice how we don't specify a
value, to assure that the entire attribute is removed. This is potentially
dangerous, so again be careful.

=head1 INSTALLATION

In order to use this script, you'll need Perl version 5.004 or later, the
LDAP SDK, and also the LDAP Perl module (aka PerLDAP). Once you've installed
these packages, just copy this file to where you keep your admin binaries,
e.g. /usr/local/bin.

In order to get good performance, you should make sure you have indexes on
the attributes you typically use with this script. Our experience has been
that in most cases the standard indexes in the Directory Server are
sufficient, e.g. I<CN>, I<UID> and I<MAIL>.

=head1 AVAILABILITY

This package can be retrieved from a number of places, including:

    http://www.mozilla.org/directory/
    Your local CPAN server

=head1 CREDITS

This little tool was developed internally at Netscape, by Leif Hedstrom.

=head1 BUGS

None, of course...

=head1 SEE ALSO

L<Mozilla::LDAP::API> and L<Perl>

=cut
