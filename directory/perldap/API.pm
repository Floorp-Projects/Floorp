#############################################################################
# $Id: API.pm,v 1.14 1999/03/22 04:13:22 leif%netscape.com Exp $
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
#    This is a description.
#
#############################################################################

package Mozilla::LDAP::API;

use strict;
use Carp;
use vars qw($VERSION @ISA %EXPORT_TAGS @EXPORT @EXPORT_OK $AUTOLOAD);

require Exporter;
require DynaLoader;
require AutoLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

%EXPORT_TAGS = (
	constant => [qw(
	LDAPS_PORT
	LDAP_ADMINLIMIT_EXCEEDED
	LDAP_AFFECTS_MULTIPLE_DSAS
	LDAP_ALIAS_DEREF_PROBLEM
	LDAP_ALIAS_PROBLEM
	LDAP_ALL_USER_ATTRS
	LDAP_ALREADY_EXISTS
	LDAP_AUTH_METHOD_NOT_SUPPORTED
	LDAP_AUTH_NONE
	LDAP_AUTH_SASL
	LDAP_AUTH_SIMPLE
	LDAP_AUTH_UNKNOWN
	LDAP_BUSY
	LDAP_CACHE_CHECK
	LDAP_CACHE_LOCALDB
	LDAP_CACHE_POPULATE
	LDAP_CHANGETYPE_ADD
	LDAP_CHANGETYPE_ANY
	LDAP_CHANGETYPE_DELETE
	LDAP_CHANGETYPE_MODDN
	LDAP_CHANGETYPE_MODIFY
	LDAP_CLIENT_LOOP
	LDAP_COMPARE_FALSE
	LDAP_COMPARE_TRUE
	LDAP_CONFIDENTIALITY_REQUIRED
	LDAP_CONNECT_ERROR
	LDAP_CONSTRAINT_VIOLATION
	LDAP_CONTROL_ENTRYCHANGE
	LDAP_CONTROL_MANAGEDSAIT
	LDAP_CONTROL_NOT_FOUND
	LDAP_CONTROL_PERSISTENTSEARCH
	LDAP_CONTROL_PWEXPIRED
	LDAP_CONTROL_PWEXPIRING
	LDAP_CONTROL_REFERRALS
	LDAP_CONTROL_SORTREQUEST
	LDAP_CONTROL_SORTRESPONSE
	LDAP_CONTROL_VLVREQUEST
	LDAP_CONTROL_VLVRESPONSE
	LDAP_DECODING_ERROR
	LDAP_DEREF_ALWAYS
	LDAP_DEREF_FINDING
	LDAP_DEREF_NEVER
	LDAP_DEREF_SEARCHING
	LDAP_ENCODING_ERROR
	LDAP_FILTER_ERROR
	LDAP_FILT_MAXSIZ
	LDAP_INAPPROPRIATE_AUTH
	LDAP_INAPPROPRIATE_MATCHING
	LDAP_INSUFFICIENT_ACCESS
	LDAP_INVALID_CREDENTIALS
	LDAP_INVALID_DN_SYNTAX
	LDAP_INVALID_SYNTAX
	LDAP_IS_LEAF
	LDAP_LOCAL_ERROR
	LDAP_LOOP_DETECT
	LDAP_MOD_ADD
	LDAP_MOD_BVALUES
	LDAP_MOD_DELETE
	LDAP_MOD_REPLACE
	LDAP_MORE_RESULTS_TO_RETURN
	LDAP_MSG_ALL
	LDAP_MSG_ONE
	LDAP_MSG_RECEIVED
	LDAP_NAMING_VIOLATION
	LDAP_NOT_ALLOWED_ON_NONLEAF
	LDAP_NOT_ALLOWED_ON_RDN
	LDAP_NOT_SUPPORTED
	LDAP_NO_ATTRS
	LDAP_NO_LIMIT
	LDAP_NO_MEMORY
	LDAP_NO_OBJECT_CLASS_MODS
	LDAP_NO_RESULTS_RETURNED
	LDAP_NO_SUCH_ATTRIBUTE
	LDAP_NO_SUCH_OBJECT
	LDAP_OBJECT_CLASS_VIOLATION
	LDAP_OPERATIONS_ERROR
	LDAP_OPT_CACHE_ENABLE
	LDAP_OPT_CACHE_FN_PTRS
	LDAP_OPT_CACHE_STRATEGY
	LDAP_OPT_CLIENT_CONTROLS
	LDAP_OPT_DEREF
	LDAP_OPT_DESC
	LDAP_OPT_DNS
	LDAP_OPT_DNS_FN_PTRS
	LDAP_OPT_ERROR_NUMBER
	LDAP_OPT_ERROR_STRING
	LDAP_OPT_HOST_NAME
	LDAP_OPT_IO_FN_PTRS
	LDAP_OPT_MEMALLOC_FN_PTRS
	LDAP_OPT_OFF
	LDAP_OPT_ON
	LDAP_OPT_PREFERRED_LANGUAGE
	LDAP_OPT_PROTOCOL_VERSION
	LDAP_OPT_REBIND_ARG
	LDAP_OPT_REBIND_FN
	LDAP_OPT_RECONNECT
	LDAP_OPT_REFERRALS
	LDAP_OPT_REFERRAL_HOP_LIMIT
	LDAP_OPT_RESTART
	LDAP_OPT_RETURN_REFERRALS
	LDAP_OPT_SERVER_CONTROLS
	LDAP_OPT_SIZELIMIT
	LDAP_OPT_SSL
	LDAP_OPT_THREAD_FN_PTRS
	LDAP_OPT_TIMELIMIT
	LDAP_OTHER
	LDAP_PARAM_ERROR
	LDAP_PARTIAL_RESULTS
	LDAP_PORT
	LDAP_PORT_MAX
	LDAP_PROTOCOL_ERROR
	LDAP_REFERRAL
	LDAP_REFERRAL_LIMIT_EXCEEDED
	LDAP_RESULTS_TOO_LARGE
	LDAP_RES_ADD
	LDAP_RES_ANY
	LDAP_RES_BIND
	LDAP_RES_COMPARE
	LDAP_RES_DELETE
	LDAP_RES_EXTENDED
	LDAP_RES_MODIFY
	LDAP_RES_MODRDN
	LDAP_RES_RENAME
	LDAP_RES_SEARCH_ENTRY
	LDAP_RES_SEARCH_REFERENCE
	LDAP_RES_SEARCH_RESULT
	LDAP_ROOT_DSE
	LDAP_SASL_BIND_IN_PROGRESS
	LDAP_SASL_EXTERNAL
	LDAP_SASL_SIMPLE
	LDAP_SCOPE_BASE
	LDAP_SCOPE_ONELEVEL
	LDAP_SCOPE_SUBTREE
	LDAP_SECURITY_NONE
	LDAP_SERVER_DOWN
	LDAP_SIZELIMIT_EXCEEDED
	LDAP_SORT_CONTROL_MISSING
	LDAP_STRONG_AUTH_NOT_SUPPORTED
	LDAP_STRONG_AUTH_REQUIRED
	LDAP_SUCCESS
	LDAP_TIMELIMIT_EXCEEDED
	LDAP_TIMEOUT
	LDAP_TYPE_OR_VALUE_EXISTS
	LDAP_UNAVAILABLE
	LDAP_UNAVAILABLE_CRITICAL_EXTENSION
	LDAP_UNDEFINED_TYPE
	LDAP_UNWILLING_TO_PERFORM
	LDAP_URL_ERR_BADSCOPE
	LDAP_URL_ERR_MEM
	LDAP_URL_ERR_NODN
	LDAP_URL_ERR_NOTLDAP
	LDAP_URL_ERR_PARAM
	LDAP_URL_OPT_SECURE
	LDAP_USER_CANCELLED
	LDAP_VERSION
	LDAP_VERSION1
	LDAP_VERSION2
	LDAP_VERSION3
	LDAP_VERSION_MAX)],
	api => [qw(
	ldap_abandon ldap_add ldap_add_s ldap_ber_free ldap_build_filter
	ldap_compare ldap_compare_s ldap_count_entries ldap_create_filter
	ldap_delete ldap_delete_s ldap_dn2ufn ldap_err2string
	ldap_explode_dn ldap_explode_rdn ldap_first_attribute
	ldap_first_entry ldap_free_friendlymap ldap_free_sort_keylist
	ldap_free_urldesc ldap_friendly_name ldap_get_dn ldap_getfilter_free
	ldap_getfirstfilter ldap_get_lang_values ldap_get_lang_values_len
	ldap_get_lderrno ldap_getnextfilter ldap_get_option ldap_get_values
	ldap_get_values_len ldap_init ldap_init_getfilter
	ldap_init_getfilter_buf ldap_is_ldap_url ldap_memcache_destroy
	ldap_memcache_flush ldap_memcache_get ldap_memcache_init
	ldap_memcache_set ldap_memcache_update ldap_memfree ldap_modify
	ldap_modify_s ldap_modrdn ldap_modrdn_s ldap_modrdn2 ldap_modrdn2_s
	ldap_mods_free ldap_msgfree ldap_msgid ldap_msgtype
	ldap_multisort_entries ldap_next_attribute ldap_next_entry
	ldap_perror ldap_result ldap_result2error ldap_search ldap_search_s
	ldap_search_st ldap_set_filter_additions ldap_set_lderrno
	ldap_set_option ldap_set_rebind_proc ldap_set_default_rebind_proc
	ldap_simple_bind ldap_simple_bind_s ldap_sort_entries ldap_unbind
	ldap_unbind_s ldap_url_parse ldap_url_search ldap_url_search_s
	ldap_url_search_st ldap_version)],
	apiv3 => [qw(
	ldap_abandon_ext ldap_add_ext ldap_add_ext_s ldap_compare_ext
	ldap_compare_ext_s ldap_control_free ldap_controls_count
	ldap_controls_free ldap_count_messages ldap_count_references
	ldap_create_persistentsearch_control ldap_create_sort_control
	ldap_create_sort_keylist ldap_create_virtuallist_control
	ldap_delete_ext ldap_delete_ext_s ldap_extended_operation
	ldap_extended_operation_s ldap_first_message ldap_first_reference
	ldap_get_entry_controls ldap_modify_ext ldap_modify_ext_s
	ldap_next_message ldap_next_reference ldap_parse_entrychange_control
	ldap_parse_extended_result ldap_parse_reference ldap_parse_result
	ldap_parse_sasl_bind_result ldap_parse_sort_control
	ldap_parse_virtuallist_control ldap_rename ldap_rename_s
	ldap_sasl_bind ldap_sasl_bind_s ldap_search_ext ldap_search_ext_s)],
	ssl => [qw(
	ldapssl_client_init
	ldapssl_enable_clientauth
	ldapssl_clientauth_init
	ldapssl_init
	ldapssl_install_routines)]
);

# Add Everything in %EXPORT_TAGS to @EXPORT_OK
Exporter::export_ok_tags(keys %EXPORT_TAGS);

$VERSION = '1.2.1';

# The XS 'constant' routine returns an integer.  There are all constants
# we want to return something else.
my %OVERRIDE_CONST = (
   "LDAP_ALL_USER_ATTRS","*",
   "LDAP_CONTROL_ENTRYCHANGE","2.16.840.1.113730.3.4.7",
   "LDAP_CONTROL_MANAGEDSAIT","2.16.840.1.113730.3.4.2",
   "LDAP_CONTROL_PERSISTENTSEARCH","2.16.840.1.113730.3.4.3",
   "LDAP_CONTROL_PWEXPIRED","2.16.840.1.113730.3.4.4",
   "LDAP_CONTROL_PWEXPIRING","2.16.840.1.113730.3.4.5",
   "LDAP_CONTROL_REFERRALS","1.2.840.113556.1.4.616",
   "LDAP_CONTROL_SORTREQUEST","1.2.840.113556.1.4.473",
   "LDAP_CONTROL_SORTRESPONSE","1.2.840.113556.1.4.474",
   "LDAP_CONTROL_VLVREQUEST","2.16.840.1.113730.3.4.9",
   "LDAP_CONTROL_VLVRESPONSE","2.16.840.1.113730.3.4.10",
   "LDAP_NO_ATTRS","1.1",
   "LDAP_OPT_OFF",0,
   "LDAP_OPT_ON",1,
   "LDAP_ROOT_DSE","",
   "LDAP_SASL_EXTERNAL","EXTERNAL",
);

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my $val;
    if (($val = $OVERRIDE_CONST{$constname}))
    {
        eval "sub $AUTOLOAD { $val }";
        goto &$AUTOLOAD;
    }
    $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
		croak "Your vendor has not defined Mozilla::LDAP macro $constname";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap Mozilla::LDAP::API $VERSION;

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

  Mozilla::LDAP::API - Perl methods for LDAP C API calls

=head1 SYNOPSIS

  use Mozilla::LDAP::API;
       or
  use Mozilla::LDAP::API qw(:api :ssl :constant);
       or
  use Mozilla::LDAP::API qw(:api :ssl :apiv3 :constant);

=head1 DESCRIPTION

This package offers a direct interface to the LDAP C API calls from Perl.
It is used internally by the other Mozilla::LDAP modules.  It is highly
suggested that you use the object oriented interface in
Mozilla::LDAP::Conn and Mozilla::LDAP::Entry unless you need to use
asynchronous calls or other functionality not available in the OO interface.

=head1 THIS DOCUMENT

This document has a number of known errors that will be corrected in the
next revision.  Since it is not expected that users will need to use this
interface frequently, priority was placed on other documents.  You can find
examples of how to actually use the API calls under the test_api directory.

=head1 CREATING AN ADD/MODIFY HASH

For the add and modify routines you will need to generate
a list of attributes and values.

You will do this by creating a HASH table.  Each attribute in the
hash contains associated values.  These values can be one of three
things.

  - SCALAR VALUE    (ex. "Clayton Donley")
  - ARRAY REFERENCE (ex. ["Clayton Donley","Clay Donley"])
  - HASH REFERENCE  (ex. {"r",["Clayton Donley"]}
       note:  the value inside the HASH REFERENCE must currently
               be an ARRAY REFERENCE.

The key inside the HASH REFERENCE must be one of the following for a
modify operation:
  - "a" for LDAP_MOD_ADD (Add these values to the attribute)
  - "r" for LDAP_MOD_REPLACE (Replace these values in the attribute)
  - "d" for LDAP_MOD_DELETE (Delete these values from the attribute)

Additionally, in add and modify operations, you may specify "b" if the
attributes you are adding are BINARY (ex. "rb" to replace binary).

Currently, it is only possible to do one operation per add/modify
operation, meaning you can't do something like:

   {"d",["Clayton"],"a",["Clay"]}   <-- WRONG!

Using any combination of the above value types, you can do things like:

%ldap_modifications = (
   "cn", "Clayton Donley",                    # Replace 'cn' values
   "givenname", ["Clayton","Clay"],           # Replace 'givenname' values
   "mail", {"a",["donley\@cig.mcel.mot.com"],  #Add 'mail' values
   "jpegphoto", {"rb",[$jpegphotodata]},      # Replace Binary jpegPhoto
);

Then remember to call the add or modify operations with a REFERENCE to
this HASH.

=head1 API Methods

The following are the available API methods for Mozilla::LDAP::API.  Many
of these items have bad examples and OUTPUT information.  Other information
should be correct.

=item B<ldap_abandon>(ld,msgid)

DESCRIPTION:

Abandon an asynchronous LDAP operation

INPUT:
  ld - LDAP Session Handle
  msgid - Integer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_abandon($ld,$msgid);


=item B<ldap_abandon_ext>(ld,msgid,serverctrls,clientctrls)

DESCRIPTION:

Abandon an asynchronous LDAP operation w/ Controls

INPUT:
  ld - LDAP Session Handle
  msgid - Integer
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_abandon_ext($ld,$msgid,$serverctrls,$clientctrls);


=item B<ldap_add>(ld,dn,attrs)

DESCRIPTION:

Asynchronously add a LDAP entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  attrs - LDAP Add/Modify Hash

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_add($ld,$dn,$attrs);


=item B<ldap_add_ext>(ld,dn,attrs,serverctrls,clientctrls,msgidp)

DESCRIPTION:

Asynchronously add a LDAP entry w/ Controls

INPUT:
  ld - LDAP Session Handle
  dn - String
  attrs - LDAP Add/Modify Hash
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  msgidp - Integer

OUTPUT:
  status - Integer
  msgidp - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_add_ext($ld,$dn,$attrs,$serverctrls,$clientctrls,$msgidp);


=item B<ldap_add_ext_s>(ld,dn,attrs,serverctrls,clientctrls)

DESCRIPTION:

Synchronously add a LDAP entry w/ Controls

INPUT:
  ld - LDAP Session Handle
  dn - String
  attrs - LDAP Add/Modify Hash
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_add_ext_s($ld,$dn,$attrs,$serverctrls,$clientctrls);


=item B<ldap_add_s>(ld,dn,attrs)

DESCRIPTION:

Synchronously add a LDAP entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  attrs - LDAP Add/Modify Hash

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_add_s($ld,$dn,$attrs);


=item B<ldap_ber_free>(ber,freebuf)

DESCRIPTION:

Free a BER element pointer

INPUT:
  ber - BER Element Pointer
  freebuf - Integer

OUTPUT:
  status - NONE

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_ber_free($ber,$freebuf);


=item B<ldap_bind>(ld,dn,passwd,authmethod)

DESCRIPTION:

Asynchronously bind to the LDAP server

INPUT:
  ld - LDAP Session Handle
  dn - String
  passwd - String
  authmethod - Integer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_bind($ld,$dn,$passwd,$authmethod);


=item B<ldap_bind_s>(ld,dn,passwd,authmethod)

DESCRIPTION:

Synchronously bind to a LDAP server

INPUT:
  ld - LDAP Session Handle
  dn - String
  passwd - String
  authmethod - Integer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_bind_s($ld,$dn,$passwd,$authmethod);


=item B<ldap_compare>(ld,dn,attr,value)

DESCRIPTION:

Asynchronously compare an attribute/value pair and an entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  attr - String
  value - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_compare($ld,$dn,$attr,$value);


=item B<ldap_compare_ext>(ld,dn,attr,bvalue,serverctrls,clientctrls,msgidp)

DESCRIPTION:

Asynchronously compare an attribute/value pair and an entry w/ Controls

INPUT:
  ld - LDAP Session Handle
  dn - String
  attr - String
  bvalue - Binary String
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  msgidp - Integer

OUTPUT:
  status - Integer
  msgidp - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_compare_ext($ld,$dn,$attr,$bvalue,$serverctrls,$clientctrls,$msgidp);


=item B<ldap_compare_ext_s>(ld,dn,attr,bvalue,serverctrls,clientctrls)

DESCRIPTION:

Synchronously compare an attribute/value pair to an entry w/ Controls

INPUT:
  ld - LDAP Session Handle
  dn - String
  attr - String
  bvalue - Binary String
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_compare_ext_s($ld,$dn,$attr,$bvalue,$serverctrls,$clientctrls);


=item B<ldap_compare_s>(ld,dn,attr,value)

DESCRIPTION:

Synchronously compare an attribute/value pair to an entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  attr - String
  value - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_compare_s($ld,$dn,$attr,$value);


=item B<ldap_control_free>(ctrl)

DESCRIPTION:

Free a LDAP control pointer

INPUT:
  ctrl - LDAP Control Pointer

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_control_free($ctrl);


=item B<ldap_controls_count>(ctrls)

DESCRIPTION:

Count the number of LDAP controls in a LDAP Control List

INPUT:
  ctrls - LDAP Control List Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_controls_count($ctrls);


=item B<ldap_controls_free>(ctrls)

DESCRIPTION:

Free a list of LDAP controls

INPUT:
  ctrls - LDAP Control List Pointer

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_controls_free($ctrls);


=item B<ldap_count_entries>(ld,result)

DESCRIPTION:

Count the number of LDAP entries returned

INPUT:
  ld - LDAP Session Handle
  result - LDAP Message Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $count = ldap_count_entries($ld,$result);


=item B<ldap_count_messages>(ld,result)

DESCRIPTION:

Count the number of LDAP messages returned

INPUT:
  ld - LDAP Session Handle
  result - LDAP Message Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_count_messages($ld,$result);


=item B<ldap_count_references>(ld,result)

DESCRIPTION:

Count the number of LDAP references returned

INPUT:
  ld - LDAP Session Handle
  result - LDAP Message Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_count_references($ld,$result);


=item B<ldap_create_filter>(buf,buflen,pattern,prefix,suffix,attr,value,valwords)

DESCRIPTION:

Create a LDAP search filter

INPUT:
  buf - String
  buflen - Integer
  pattern - String
  prefix - String
  suffix - String
  attr - String
  value - String
  valwords - List Reference

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_create_filter($buf,$buflen,$pattern,$prefix,$suffix,$attr,$value,$valwords);


=item B<ldap_create_persistentsearch_control>(ld,changetypes,changesonly,return_echg_ctrls,ctrl_iscritical,ctrlp)

DESCRIPTION:

Create a persistent search control

INPUT:
  ld - LDAP Session Handle
  changetypes - Integer
  changesonly - Integer
  return_echg_ctrls - Integer
  ctrl_iscritical - Integer
  ctrlp - LDAP Control List Pointer

OUTPUT:
  status - Integer
  ctrlp - LDAP Control List Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_create_persistentsearch_control($ld,$changetypes,$changesonly,$return_echg_ctrls,$ctrl_iscritical,$ctrlp);


=item B<ldap_create_sort_control>(ld,sortKeyList,ctrl_iscritical,ctrlp)

DESCRIPTION:

Create a LDAP sort control

INPUT:
  ld - LDAP Session Handle
  sortKeyList - Sort Key Pointer
  ctrl_iscritical - Integer
  ctrlp - LDAP Control List Pointer

OUTPUT:
  status - Integer
  ctrlp - LDAP Control List Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_create_sort_control($ld,$sortKeyList,$ctrl_iscritical,$ctrlp);


=item B<ldap_create_sort_keylist>(sortKeyList,string_rep)

DESCRIPTION:

Create a list of keys to be used by a sort control

INPUT:
  sortKeyList - Sort Key Pointer
  string_rep - String

OUTPUT:
  status - Integer
  sortKeyList - Sort Key Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_create_sort_keylist($sortKeyList,$string_rep);


=item B<ldap_create_virtuallist_control>(ld,ldvlistp,ctrlp)

DESCRIPTION:

Create a LDAP virtual list control

INPUT:
  ld - LDAP Session Handle
  ctrlp - LDAP Control List Pointer

OUTPUT:
  status - Integer
  ctrlp - LDAP Control List Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_create_virtuallist_control($ld,$ldvlistp,$ctrlp);


=item B<ldap_delete>(ld,dn)

DESCRIPTION:

Asynchronously delete a LDAP entry

INPUT:
  ld - LDAP Session Handle
  dn - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_delete($ld,$dn);


=item B<ldap_delete_ext>(ld,dn,serverctrls,clientctrls,msgidp)

DESCRIPTION:

Asynchronously delete a LDAP entry w/ Controls

INPUT:
  ld - LDAP Session Handle
  dn - String
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  msgidp - Integer

OUTPUT:
  status - Integer
  msgidp - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_delete_ext($ld,$dn,$serverctrls,$clientctrls,$msgidp);


=item B<ldap_delete_ext_s>(ld,dn,serverctrls,clientctrls)

DESCRIPTION:

Synchronously delete a LDAP entry w/ Controls

INPUT:
  ld - LDAP Session Handle
  dn - String
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_delete_ext_s($ld,$dn,$serverctrls,$clientctrls);


=item B<ldap_delete_s>(ld,dn)

DESCRIPTION:

Synchronously delete a LDAP entry

INPUT:
  ld - LDAP Session Handle
  dn - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_delete_s($ld,$dn);


=item B<ldap_dn2ufn>(dn)

DESCRIPTION:

Change a DN to a "Friendly" name

INPUT:
  dn - String

OUTPUT:
  status - String

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_dn2ufn($dn);


=item B<ldap_err2string>(err)

DESCRIPTION:

Return the string value of a LDAP error code

INPUT:
  err - Integer

OUTPUT:
  status - String

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_err2string($err);


=item B<ldap_explode_dn>(dn,notypes)

DESCRIPTION:

Split a given DN into its components.  Setting 'notypes' to 1 returns the
components without their type names.

INPUT:
  dn - String
  notypes - Integer

OUTPUT:
  status - NONE

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_explode_dn($dn,$notypes);


=item B<ldap_explode_rdn>(dn,notypes)

DESCRIPTION:

Split a Relative DN into its components

INPUT:
  dn - String
  notypes - Integer

OUTPUT:
  status - NONE

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_explode_rdn($dn,$notypes);


=item B<ldap_extended_operation>(ld,requestoid,requestdata,serverctrls,clientctrls,msgidp)

DESCRIPTION:

Perform an asynchronous extended operation

INPUT:
  ld - LDAP Session Handle
  requestoid - String
  requestdata - Binary String
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  msgidp - Integer

OUTPUT:
  status - Integer
  msgidp - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_extended_operation($ld,$requestoid,$requestdata,$serverctrls,$clientctrls,$msgidp);


=item B<ldap_extended_operation_s>(ld,requestoid,requestdata,serverctrls,clientctrls,retoidp,retdatap)

DESCRIPTION:

Perform a synchronous extended operation

INPUT:
  ld - LDAP Session Handle
  requestoid - String
  requestdata - Binary String
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  retoidp - String

OUTPUT:
  status - Integer
  retoidp - Return OID
  retdatap - Return Data

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_extended_operation_s($ld,$requestoid,$requestdata,$serverctrls,$clientctrls,$retoidp,$retdatap);


=item B<ldap_first_attribute>(ld,entry,ber)

DESCRIPTION:

Return the first attribute returned for a LDAP entry

INPUT:
  ld - LDAP Session Handle
  entry - LDAP Message Pointer
  ber - Ber Element Pointer

OUTPUT:
  status - String
  ber - Ber Element Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_first_attribute($ld,$entry,$ber);


=item B<ldap_first_entry>(ld,chain)

DESCRIPTION:

Return the first entry in a LDAP result chain

INPUT:
  ld - LDAP Session Handle
  chain - LDAP Message Pointer

OUTPUT:
  status - LDAP Message Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_first_entry($ld,$chain);


=item B<ldap_first_message>(ld,res)

DESCRIPTION:

Return the first message in a LDAP result

INPUT:
  ld - LDAP Session Handle
  res - LDAP Message Pointer

OUTPUT:
  status - LDAP Message Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_first_message($ld,$res);


=item B<ldap_first_reference>(ld,res)

DESCRIPTION:

Return the first reference in a LDAP result

INPUT:
  ld - LDAP Session Handle
  res - LDAP Message Pointer

OUTPUT:
  status - LDAP Message Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_first_reference($ld,$res);


=item B<ldap_free_friendlymap>(map)

DESCRIPTION:

Free a LDAP friendly map pointer

INPUT:
  map - Friendly Map Pointer

OUTPUT:
  status - NONE

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_free_friendlymap($map);


=item B<ldap_free_sort_keylist>(sortKeyList)

DESCRIPTION:

Free a LDAP sort key pointer

INPUT:
  sortKeyList - Sort Key Pointer

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_free_sort_keylist($sortKeyList);


=item B<ldap_free_urldesc>(ludp)

DESCRIPTION:

Free a LDAP URL description hash reference

INPUT:
  ludp - URL Description Hash Reference

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_free_urldesc($ludp);


=item B<ldap_friendly_name>(filename,name,map)

DESCRIPTION:

Create a LDAP friendly name map

INPUT:
  filename - String
  name - String
  map - Friendly Map Pointer

OUTPUT:
  status - String

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_friendly_name($filename,$name,$map);


=item B<ldap_get_dn>(ld,entry)

DESCRIPTION:

Return the distinguished name for an entry

INPUT:
  ld - LDAP Session Handle
  entry - LDAP Message Pointer

OUTPUT:
  status - String

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_get_dn($ld,$entry);


=item B<ldap_get_entry_controls>(ld,entry,serverctrlsp)

DESCRIPTION:

Return the controls for a LDAP entry

INPUT:
  ld - LDAP Session Handle
  entry - LDAP Message Pointer
  serverctrlsp - LDAP Control List Pointer

OUTPUT:
  status - Integer
  serverctrlsp - LDAP Control List Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_get_entry_controls($ld,$entry,$serverctrlsp);


=item B<ldap_getfilter_free>(lfdp)

DESCRIPTION:

Free a LDAP filter

INPUT:
  lfdp - LDAP Filter Description Pointer

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_getfilter_free($lfdp);


=item B<ldap_getfirstfilter>(lfdp,tagpat,value)

DESCRIPTION:

Get the first generated filter

INPUT:
  lfdp - LDAP Filter Description Pointer
  tagpat - String

OUTPUT:
  status - LDAP Filter Information Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_getfirstfilter($lfdp,$tagpat,$value);


=item B<ldap_get_lang_values>(ld,entry,target,type)

DESCRIPTION:

Get values for an entry

INPUT:
  ld - LDAP Session Handle
  entry - LDAP Message Pointer
  target - String
  type - String

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_get_lang_values($ld,$entry,$target,$type);


=item B<ldap_get_lang_values_len>(ld,entry,target,type)

DESCRIPTION:

Get binary values for an entry

INPUT:
  ld - LDAP Session Handle
  entry - LDAP Message Pointer
  target - String
  type - String

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_get_lang_values_len($ld,$entry,$target,$type);


=item B<ldap_get_lderrno>(ld,m,s)

DESCRIPTION:


INPUT:
  ld - LDAP Session Handle
  m  - String Reference (or undef)
  s  - String Reference (or undef)

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_get_lderrno($ld,\$m,\$s);


=item B<ldap_getnextfilter>(lfdp)

DESCRIPTION:

Get the next generated LDAP filter

INPUT:
  lfdp - LDAP Filter Information Pointer
 
OUTPUT:
  status - LDAP Filter Information Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_getnextfilter($lfdp);


=item B<ldap_get_option>(ld,option,optdata)

DESCRIPTION:

Get an option for a LDAP session

INPUT:
  ld - LDAP Session Handle
  option - Integer
  optdata - Integer

OUTPUT:
  status - Integer
  optdata - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_get_option($ld,$option,$optdata);


=item B<ldap_get_values>(ld,entry,target)

DESCRIPTION:

Get the values for a LDAP entry and attribute

INPUT:
  ld - LDAP Session Handle
  entry - LDAP Message Pointer
  target - String

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_get_values($ld,$entry,$target);


=item B<ldap_get_values_len>(ld,entry,target)

DESCRIPTION:

Get the binary values for a LDAP entry and attribute

INPUT:
  ld - LDAP Session Handle
  entry - LDAP Message Pointer
  target - String

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_get_values_len($ld,$entry,$target);


=item B<ldap_init>(host,port)

DESCRIPTION:

Initialize a LDAP session

INPUT:
  host - String
  port - Integer

OUTPUT:
  status - LDAP Session Handle

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_init($host,$port);


=item B<ldap_init_getfilter>(fname)

DESCRIPTION:

Initialize the LDAP filter generation routines to a filename

INPUT:
  fname - Filename String

OUTPUT:
  status - LDAP Filter Description Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_init_getfilter($fname);


=item B<ldap_init_getfilter_buf>(buf,buflen)

DESCRIPTION:

Initialize the LDAP filter generation routines to a buffer

INPUT:
  buf - String
  buflen - Integer

OUTPUT:
  status - LDAP Filter Description Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_init_getfilter_buf($buf,$buflen);


=item B<ldap_is_ldap_url>(url)

DESCRIPTION:

Return 1 if an the argument is a valid LDAP URL

INPUT:
  url - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_is_ldap_url($url);


=item B<ldap_memcache_destroy>(cache)

DESCRIPTION:

Destroy a memory cache

INPUT:
  cache - LDAP Memory Cache Pointer

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_memcache_destroy($cache);


=item B<ldap_memcache_flush>(cache,dn,scope)

DESCRIPTION:

Flush a specific DN from the memory cache

INPUT:
  cache - LDAP Memory Cache Pointer
  dn - String
  scope - Integer

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_memcache_flush($cache,$dn,$scope);


=item B<ldap_memcache_get>(ld,cachep)

DESCRIPTION:

Get the memory cache for a LDAP session

INPUT:
  ld - LDAP Session Handle
  cachep - LDAP Memory Cache Pointer

OUTPUT:
  status - Integer
  cachep - LDAP Memory Cache Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_memcache_get($ld,$cachep);


=item B<ldap_memcache_init>(ttl,size,baseDNs,cachep)

DESCRIPTION:

Initialize a LDAP memory cache

INPUT:
  ttl - Integer
  size - Integer
  baseDNs - List Reference
  cachep - LDAP Memory Cache Pointer

OUTPUT:
  status - Integer
  cachep - LDAP Memory Cache Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_memcache_init($ttl,$size,$baseDNs,$cachep);


=item B<ldap_memcache_set>(ld,cache)

DESCRIPTION:

Set the LDAP memory cache for the session

INPUT:
  ld - LDAP Session Handle
  cache - LDAP Memory Cache Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_memcache_set($ld,$cache);


=item B<ldap_memcache_update>(cache)

DESCRIPTION:

Update the specified memory cache

INPUT:
  cache - LDAP Memory Cache Pointer

OUTPUT:
  status - NONE

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_memcache_update($cache);


=item B<ldap_memfree>(p)

DESCRIPTION:

Free memory allocated by the LDAP C API

INPUT:
  p - Pointer

OUTPUT:
  status - NONE

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_memfree($p);


=item B<ldap_modify>(ld,dn,mods)

DESCRIPTION:

Asynchronously modify a LDAP entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  mods - LDAP Add/Modify Hash

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_modify($ld,$dn,$mods);


=item B<ldap_modify_ext>(ld,dn,mods,serverctrls,clientctrls,msgidp)

DESCRIPTION:

Asynchronously modify a LDAP entry w/ Controls

INPUT:
  ld - LDAP Session Handle
  dn - String
  mods - LDAP Add/Modify Hash
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  msgidp - Integer

OUTPUT:
  status - Integer
  msgidp - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_modify_ext($ld,$dn,$mods,$serverctrls,$clientctrls,$msgidp);


=item B<ldap_modify_ext_s>(ld,dn,mods,serverctrls,clientctrls)

DESCRIPTION:

Synchronously modify a LDAP entry w/ Controls

INPUT:
  ld - LDAP Session Handle
  dn - String
  mods - LDAP Add/Modify Hash
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_modify_ext_s($ld,$dn,$mods,$serverctrls,$clientctrls);


=item B<ldap_modify_s>(ld,dn,mods)

DESCRIPTION:

Synchronously modify a LDAP entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  mods - LDAP Add/Modify Hash

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_modify_s($ld,$dn,$mods);


=item B<ldap_modrdn>(ld,dn,newrdn)

DESCRIPTION:

Asynchronously modify the relative distinguished name of an entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  newrdn - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_modrdn($ld,$dn,$newrdn);


=item B<ldap_modrdn_s>(ld,dn,newrdn)

DESCRIPTION:

Synchronously modify the relative distinguished name of an entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  newrdn - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_modrdn_s($ld,$dn,$newrdn);


=item B<ldap_modrdn2>(ld,dn,newrdn,deleteoldrdn)

DESCRIPTION:

Asynchronously modify the relative distinguished name of an entry.

INPUT:
  ld - LDAP Session Handle
  dn - String
  newrdn - String
  deleteoldrdn - Integer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_modrdn2($ld,$dn,$newrdn,$deleteoldrdn);


=item B<ldap_modrdn2_s>(ld,dn,newrdn,deleteoldrdn)

DESCRIPTION:

Synchronously modify the relative distinguished name of an entry.

INPUT:
  ld - LDAP Session Handle
  dn - String
  newrdn - String
  deleteoldrdn - Integer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_modrdn2_s($ld,$dn,$newrdn,$deleteoldrdn);


=item B<ldap_msgfree>(lm)

DESCRIPTION:

Free memory allocated by a LDAP Message

INPUT:
  lm - LDAP Message Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_msgfree($lm);


=item B<ldap_msgid>(lm)

DESCRIPTION:

Get the message id number from a LDAP message

INPUT:
  lm - LDAP Message Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_msgid($lm);


=item B<ldap_msgtype>(lm)

DESCRIPTION:

Get the message type of a LDAP message

INPUT:
  lm - LDAP Message Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_msgtype($lm);


=item B<ldap_multisort_entries>(ld,chain,attr)

DESCRIPTION:

Sort entries by multiple keys

INPUT:
  ld - LDAP Session Handle
  chain - LDAP Message Pointer
  attr - List Reference

OUTPUT:
  status - Integer
  chain - LDAP Message Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_multisort_entries($ld,$chain,$attr);


=item B<ldap_next_attribute>(ld,entry,ber)

DESCRIPTION:

Get the next attribute for a LDAP entry

INPUT:
  ld - LDAP Session Handle
  entry - LDAP Message Pointer
  ber - Ber Element Pointer

OUTPUT:
  status - String
  ber - BER Element Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_next_attribute($ld,$entry,$ber);


=item B<ldap_next_entry>(ld,entry)

DESCRIPTION:

Get the next entry in the result chain

INPUT:
  ld - LDAP Session Handle
  entry - LDAP Message Pointer

OUTPUT:
  status - LDAP Message Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_next_entry($ld,$entry);


=item B<ldap_next_message>(ld,msg)

DESCRIPTION:

Get the next message in the result chain

INPUT:
  ld - LDAP Session Handle
  msg - LDAP Message Pointer

OUTPUT:
  status - LDAP Message Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_next_message($ld,$msg);


=item B<ldap_next_reference>(ld,ref)

DESCRIPTION:

Get the next reference in the result chain

INPUT:
  ld - LDAP Session Handle
  ref - LDAP Message Pointer

OUTPUT:
  status - LDAP Message Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_next_reference($ld,$ref);


=item B<ldap_parse_entrychange_control>(ld,ctrls,chgtypep,prevdnp,chgnumpresentp,chgnump)

DESCRIPTION:

Parse a LDAP entry change control

INPUT:
  ld - LDAP Session Handle
  ctrls - LDAP Control List Pointer
  chgtypep - Integer
  prevdnp - String
  chgnumpresentp - Integer
  chgnump - Integer

OUTPUT:
  status - Integer
  chgtypep - Integer
  prevdnp - String
  chgnumpresentp - Integer
  chgnump - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_parse_entrychange_control($ld,$ctrls,$chgtypep,$prevdnp,$chgnumpresentp,$chgnump);


=item B<ldap_parse_extended_result>(ld,res,retoidp,retdatap,freeit)

DESCRIPTION:

Parse a LDAP extended result

INPUT:
  ld - LDAP Session Handle
  res - LDAP Message Pointer
  retoidp - String
  freeit - Integer

OUTPUT:
  status - Integer
  retoidp - String
  retdatap - Binary List Reference

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_parse_extended_result($ld,$res,$retoidp,$retdatap,$freeit);


=item B<ldap_parse_reference>(ld,ref,referalsp,serverctrlsp,freeit)

DESCRIPTION:

Parse a LDAP Reference

INPUT:
  ld - LDAP Session Handle
  ref - LDAP Message Pointer
  referalsp - List Reference
  serverctrlsp - LDAP Control List Pointer
  freeit - Integer

OUTPUT:
  status - Integer
  referalsp - List Reference
  serverctrlsp - LDAP Control List Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_parse_reference($ld,$ref,$referalsp,$serverctrlsp,$freeit);


=item B<ldap_parse_result>(ld,res,errcodep,matcheddnp,errmsgp,referralsp,serverctrlsp,freeit)

DESCRIPTION:

Parse a LDAP result

INPUT:
  ld - LDAP Session Handle
  res - LDAP Message Pointer
  errcodep - Integer
  matcheddnp - String
  errmsgp - String
  referralsp - List Reference
  serverctrlsp - LDAP Control List Pointer
  freeit - Integer

OUTPUT:
  status - Integer
  errcodep - Integer
  matcheddnp - String
  errmsgp - String
  referralsp - List Reference
  serverctrlsp - LDAP Control List Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_parse_result($ld,$res,$errcodep,$matcheddnp,$errmsgp,$referralsp,$serverctrlsp,$freeit);


=item B<ldap_parse_sasl_bind_result>(ld,res,servercredp,freeit)

DESCRIPTION:

Parse the results of an SASL bind operation

INPUT:
  ld - LDAP Session Handle
  res - LDAP Message Pointer
  freeit - Integer

OUTPUT:
  status - Integer
  servercredp - 

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_parse_sasl_bind_result($ld,$res,$servercredp,$freeit);


=item B<ldap_parse_sort_control>(ld,ctrls,result,attribute)

DESCRIPTION:

Parse a LDAP sort control

INPUT:
  ld - LDAP Session Handle
  ctrls - LDAP Control List Pointer
  result - LDAP Message Pointer
  attribute - String

OUTPUT:
  status - Integer
  result - LDAP Message Pointer
  attribute - String

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_parse_sort_control($ld,$ctrls,$result,$attribute);


=item B<ldap_parse_virtuallist_control>(ld,ctrls,target_posp,list_sizep,errcodep)

DESCRIPTION:

Parse a LDAP virtual list control

INPUT:
  ld - LDAP Session Handle
  ctrls - LDAP Control List Pointer
  target_posp - Integer
  list_sizep - Integer
  errcodep - Integer

OUTPUT:
  status - Integer
  target_posp - Integer
  list_sizep - Integer
  errcodep - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_parse_virtuallist_control($ld,$ctrls,$target_posp,$list_sizep,$errcodep);


=item B<ldap_perror>(ld,s)

DESCRIPTION:

Print a LDAP error message

INPUT:
  ld - LDAP Session Handle
  s - String

OUTPUT:
  status - NONE

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_perror($ld,$s);


=item B<ldap_rename>(ld,dn,newrdn,newparent,deleteoldrdn,serverctrls,clientctrls,msgidp)

DESCRIPTION:

Asynchronously rename a LDAP entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  newrdn - String
  newparent - String
  deleteoldrdn - Integer
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  msgidp - Integer

OUTPUT:
  status - Integer
  msgidp - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_rename($ld,$dn,$newrdn,$newparent,$deleteoldrdn,$serverctrls,$clientctrls,$msgidp);


=item B<ldap_rename_s>(ld,dn,newrdn,newparent,deleteoldrdn,serverctrls,clientctrls)

DESCRIPTION:

Synchronously rename a LDAP entry

INPUT:
  ld - LDAP Session Handle
  dn - String
  newrdn - String
  newparent - String
  deleteoldrdn - Integer
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer

OUTPUT:
  status - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_rename_s($ld,$dn,$newrdn,$newparent,$deleteoldrdn,$serverctrls,$clientctrls);


=item B<ldap_result>(ld,msgid,all,timeout,result)

DESCRIPTION:

Get the result for an asynchronous LDAP operation

INPUT:
  ld - LDAP Session Handle
  msgid - Integer
  all - Integer
  timeout - Time in Seconds
  result - LDAP Message Pointer

OUTPUT:
  status - Integer
  result - LDAP Message Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_result($ld,$msgid,$all,$timeout,$result);


=item B<ldap_result2error>(ld,r,freeit)

DESCRIPTION:

Get the error number for a given result

INPUT:
  ld - LDAP Session Handle
  r - LDAP Message Pointer
  freeit - Integer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_result2error($ld,$r,$freeit);


=item B<ldap_sasl_bind>(ld,dn,mechanism,cred,serverctrls,clientctrls,msgidp)

DESCRIPTION:

Asynchronously bind to the LDAP server using a SASL mechanism

INPUT:
  ld - LDAP Session Handle
  dn - String
  mechanism - String
  cred - Binary String
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  msgidp - Integer

OUTPUT:
  status - Integer
  msgidp - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_sasl_bind($ld,$dn,$mechanism,$cred,$serverctrls,$clientctrls,$msgidp);


=item B<ldap_sasl_bind_s>(ld,dn,mechanism,cred,serverctrls,clientctrls,servercredp)

DESCRIPTION:

Synchronously bind to a LDAP server using a SASL mechanism

INPUT:
  ld - LDAP Session Handle
  dn - String
  mechanism - String
  cred - Binary String
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer

OUTPUT:
  status - Integer
  servercredp - 

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_sasl_bind_s($ld,$dn,$mechanism,$cred,$serverctrls,$clientctrls,$servercredp);


=item B<ldap_search>(ld,base,scope,filter,attrs,attrsonly)

DESCRIPTION:

Asynchronously search the LDAP server

INPUT:
  ld - LDAP Session Handle
  base - String
  scope - Integer
  filter - String
  attrs - List Reference
  attrsonly - Integer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_search($ld,$base,$scope,$filter,$attrs,$attrsonly);


=item B<ldap_search_ext>(ld,base,scope,filter,attrs,attrsonly,serverctrls,clientctrls,timeoutp,sizelimit,msgidp)

DESCRIPTION:

Asynchronously search the LDAP server w/ Controls

INPUT:
  ld - LDAP Session Handle
  base - String
  scope - Integer
  filter - String
  attrs - List Reference
  attrsonly - Integer
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  timeoutp - Time in Seconds
  sizelimit - Integer
  msgidp - Integer

OUTPUT:
  status - Integer
  msgidp - Integer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_search_ext($ld,$base,$scope,$filter,$attrs,$attrsonly,$serverctrls,$clientctrls,$timeoutp,$sizelimit,$msgidp);


=item B<ldap_search_ext_s>(ld,base,scope,filter,attrs,attrsonly,serverctrls,clientctrls,timeoutp,sizelimit,res)

DESCRIPTION:

Synchronously search the LDAP server w/ Controls

INPUT:
  ld - LDAP Session Handle
  base - String
  scope - Integer
  filter - String
  attrs - List Reference
  attrsonly - Integer
  serverctrls - LDAP Control List Pointer
  clientctrls - LDAP Control List Pointer
  timeoutp - Time in Seconds
  sizelimit - Integer
  res - LDAP Message Pointer

OUTPUT:
  status - Integer
  res - LDAP Message Pointer

AVAILABILITY: V3

EXAMPLE:

  $status = ldap_search_ext_s($ld,$base,$scope,$filter,$attrs,$attrsonly,$serverctrls,$clientctrls,$timeoutp,$sizelimit,$res);


=item B<ldap_search_s>(ld,base,scope,filter,attrs,attrsonly,res)

DESCRIPTION:

Synchronously search the LDAP server

INPUT:
  ld - LDAP Session Handle
  base - String
  scope - Integer
  filter - String
  attrs - List Reference
  attrsonly - Integer
  res - LDAP Message Pointer

OUTPUT:
  status - Integer
  res - LDAP Message Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_search_s($ld,$base,$scope,$filter,$attrs,$attrsonly,$res);


=item B<ldap_search_st>(ld,base,scope,filter,attrs,attrsonly,timeout,res)

DESCRIPTION:

Synchronously search the LDAP server w/ Timeout

INPUT:
  ld - LDAP Session Handle
  base - String
  scope - Integer
  filter - String
  attrs - List Reference
  attrsonly - Integer
  timeout - Time in Seconds
  res - LDAP Message Pointer

OUTPUT:
  status - Integer
  res - LDAP Message Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_search_st($ld,$base,$scope,$filter,$attrs,$attrsonly,$timeout,$res);


=item B<ldap_set_filter_additions>(lfdp,prefix,suffix)

DESCRIPTION:

Add a prefix and suffix for filter generation

INPUT:
  lfdp - LDAP Filter Description Pointer
  prefix - String
  suffix - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_set_filter_additions($lfdp,$prefix,$suffix);


=item B<ldap_set_lderrno>(ld,e,m,s)

DESCRIPTION:

Set the LDAP error structure

INPUT:
  ld - LDAP Session Handle
  e - Integer
  m - String
  s - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_set_lderrno($ld,$e,$m,$s);


=item B<ldap_set_option>(ld,option,optdata)

DESCRIPTION:

Set a LDAP session option

INPUT:
  ld - LDAP Session Handle
  option - Integer
  optdata - Integer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_set_option($ld,$option,$optdata);


=item B<ldap_set_rebind_proc>(ld,rebindproc)

DESCRIPTION:

Set the LDAP rebind process

INPUT:
  ld - LDAP Session Handle

OUTPUT:
  status - NONE

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_set_rebind_proc($ld,$rebindproc);


=item B<ldap_simple_bind>(ld,who,passwd)

DESCRIPTION:

Asynchronously bind to the LDAP server using simple authentication

INPUT:
  ld - LDAP Session Handle
  who - String
  passwd - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_simple_bind($ld,$who,$passwd);


=item B<ldap_simple_bind_s>(ld,who,passwd)

DESCRIPTION:

Synchronously bind to the LDAP server using simple authentication

INPUT:
  ld - LDAP Session Handle
  who - String
  passwd - String

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_simple_bind_s($ld,$who,$passwd);


=item B<ldap_sort_entries>(ld,chain,attr)

DESCRIPTION:

Sort the results of a LDAP search

INPUT:
  ld - LDAP Session Handle
  chain - LDAP Message Pointer
  attr - String

OUTPUT:
  status - Integer
  chain - LDAP Message Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_sort_entries($ld,$chain,$attr);


=item B<ldap_unbind>(ld)

DESCRIPTION:

Asynchronously unbind from the LDAP server

INPUT:
  ld - LDAP Session Handle

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_unbind($ld);


=item B<ldap_unbind_s>(ld)

DESCRIPTION:

Synchronously unbind from a LDAP server

INPUT:
  ld - LDAP Session Handle

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_unbind_s($ld);


=item B<ldap_url_parse>(url)

DESCRIPTION:

Parse a LDAP URL, returning a HASH of its components

INPUT:
  url - String

OUTPUT:
  status - 

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_url_parse($url);


=item B<ldap_url_search>(ld,url,attrsonly)

DESCRIPTION:

Asynchronously search using a LDAP URL

INPUT:
  ld - LDAP Session Handle
  url - String
  attrsonly - Integer

OUTPUT:
  status - Integer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_url_search($ld,$url,$attrsonly);


=item B<ldap_url_search_s>(ld,url,attrsonly,res)

DESCRIPTION:

Synchronously search using a LDAP URL

INPUT:
  ld - LDAP Session Handle
  url - String
  attrsonly - Integer
  res - LDAP Message Pointer

OUTPUT:
  status - Integer
  res - LDAP Message Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_url_search_s($ld,$url,$attrsonly,$res);


=item B<ldap_url_search_st>(ld,url,attrsonly,timeout,res)

DESCRIPTION:

Synchronously search using a LDAP URL w/ timeout

INPUT:
  ld - LDAP Session Handle
  url - String
  attrsonly - Integer
  timeout - Time in Seconds
  res - LDAP Message Pointer

OUTPUT:
  status - Integer
  res - LDAP Message Pointer

AVAILABILITY: V2/V3

EXAMPLE:

  $status = ldap_url_search_st($ld,$url,$attrsonly,$timeout,$res);

=head1 CREDITS

Most of the Perl API module was written by Clayton Donley to interface with
C API routines from Netscape Communications Corp., Inc.

=head1 BUGS

Documentation needs much work.
LDAPv3 calls not tested or supported in this version.
NT can not use Perl Rebind processes, must use 'ldap_set_default_rebindproc'.
Possible memory leak in ldap_search* is being investigated.

=head1 SEE ALSO

L<Mozilla::LDAP::Conn>, L<Mozilla::LDAP::Entry>, and L<Perl>

=cut
