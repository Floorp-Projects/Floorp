#############################################################################
# $Id: API.pm,v 1.8 1998/08/10 21:56:09 clayton Exp $
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
	ldap_set_option ldap_set_rebind_proc ldap_simple_bind
	ldap_simple_bind_s ldap_sort_entries ldap_unbind ldap_unbind_s
	ldap_url_parse ldap_url_search ldap_url_search_s ldap_url_search_st
	ldap_version)],
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

$VERSION = '1.00';

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

Mozilla::LDAP::API - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Mozilla::LDAP::API;
  blah blah blah

=head1 DESCRIPTION

=head1 API Calls

=item B<ldap_abandon>

Description:

Abandon an LDAP operation.

Input:
  o LD - LDAP Connection Handle
  o MSGID - LDAP Message ID

Output:

  o STATUS

Availability: v2/v3

Example:

  $status = ldap_abandon($ld,$msgid);

=item B<ldap_abandon_ext>

Description:

  Abandon an LDAP operation w/ Controls

Input:
  o LD - LDAP Connection Handle
  o MSGID - LDAP Message ID
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control

Output:

  o STATUS

Availability: v3

Example:

  $status = ldap_abandon_ext($ld,$msgid,$serverctrls,$clientctrls);

=item B<ldap_add>

Description:

Asynchronously add an LDAP entry

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name to Add
  o ATTRS - LDAP Add/Modify Hash

Output:

  o MSGID

Availability: v2/v3

Example:

  $msgid = ldap_add($ld,$dn,$attrs);

=item B<ldap_add_ext>

Description:

Asynchronously add an LDAP entry w/ Controls

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o ATTRS - LDAP Add/Modify Hash
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o MSGID - LDAP Message ID

Output:

  o STATUS
  o MSGID

Availability: v3

Example:

  $status = ldap_add_ext($ld,$dn,$attrs,$serverctrls,$clientctrls,$msgid);

=item B<ldap_add_ext_s>

Description:

Synchronously add an LDAP entry w/ Controls.

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o ATTRS - LDAP Add/Modify Hash
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control

Output:

  o STATUS

Availability: v3

Example:

  $status = ldap_add_ext_s($ld,$dn,$attrs,$serverctrls,$clientctrls);

=item B<ldap_add_s>

Description:

Synchronously add an LDAP entry.

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o ATTRS - Attribute List Reference

Output:

  o STATUS

Availability: v2/v3

Example:

  $status = ldap_add_s($ld,$dn,$attrs);

=item B<ldap_ber_free>

Description:

Free a BER Structure

Input:
  o BER - Opaque BER Structure
  o FREEBUF - 1 to Free Buffer

Output:

  o NONE

Availability: v2/v3

Example:

  ldap_ber_free($ber,1);

=item B<ldap_bind>

Description:

Asynchronously bind to LDAP server.

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o PASSWD - Credentials
  o AUTHMETHOD - LDAP_AUTH_SIMPLE or other defined AUTH methods

Output:

  o MSGID

Availability: v2/v3

Example:

  $msgid = ldap_bind($ld,$dn,$passwd,$authmethod);

=item B<ldap_bind_s>

Description:

Synchronously bind to the LDAP server.

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o PASSWD - Authentication Credentials
  o AUTHMETHOD - LDAP_AUTH_SIMPLE or other defined AUTH methods

Output:

  o STATUS

Availability: v2/v3

Example:

  $status = ldap_bind_s($ld,$dn,$passwd,$authmethod);

=item B<ldap_compare>

Description:

Asynchronously compare an attribute/value pair to those in an LDAP entry.

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o ATTR - Attribute String
  o VALUE - Value String

Output:

  o MSGID

Availability: v2/v3

Example:

  $msgid = ldap_compare($ld,$dn,$attr,$value);

=item B<ldap_compare_ext>

Description:

Asynchronously compare an attribute/value pair to those in an LDAP entry.
Allow Controls.


Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o ATTR - Attribute String
  o BVALUE - Value Binary String
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o MSGID - LDAP Message ID

Output:

  o STATUS

Availability: v3

Example:

  $status = ldap_compare_ext($ld,$dn,$attr,$bvalue,$serverctrls,$clientctrls,
     $msgid);

=item B<ldap_compare_ext_s>

Description:

Synchronously compare an attribute/value pair to those in an LDAP entry.
Allow Controls.

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o ATTR - Attribute String
  o BVALUE - Value Binary String
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control

Output:

  o STATUS

Availability: v3

Example:

  $status = ldap_compare_ext_s($ld,$dn,$attr,$bvalue,$serverctrls,$clientctrls);

=item B<ldap_compare_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o ATTR - ....will be defined....
  o VALUE - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_compare_s($ld,$dn,$attr,$value);

=item B<ldap_control_free>

Description:

  Blah...Blah...

Input:
  o CTRL - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_control_free($ctrl);

=item B<ldap_controls_count>

Description:

  Blah...Blah...

Input:
  o CTRLS - LDAP Control

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_controls_count($ctrls);

=item B<ldap_controls_free>

Description:

  Blah...Blah...

Input:
  o CTRLS - LDAP Control

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_controls_free($ctrls);

=item B<ldap_count_entries>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o RESULT - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_count_entries($ld,$result);

=item B<ldap_count_messages>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o RESULT - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_count_messages($ld,$result);

=item B<ldap_count_references>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o RESULT - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_count_references($ld,$result);

=item B<ldap_create_filter>

Description:

  Blah...Blah...

Input:
  o BUF - ....will be defined....
  o BUFLEN - ....will be defined....
  o PATTERN - ....will be defined....
  o PREFIX - ....will be defined....
  o SUFFIX - ....will be defined....
  o ATTR - ....will be defined....
  o VALUE - ....will be defined....
  o VALWORDS - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_create_filter($buf,$buflen,$pattern,$prefix,$suffix,$attr,$value,$valwords);

=item B<ldap_create_persistentsearch_control>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o CHANGETYPES - ....will be defined....
  o CHANGESONLY - ....will be defined....
  o RETURN_ECHG_CTRLS - LDAP Control
  o CTRL_ISCRITICAL - ....will be defined....
  o CTRLP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_create_persistentsearch_control($ld,$changetypes,$changesonly,$return_echg_ctrls,$ctrl_iscritical,$ctrlp);

=item B<ldap_create_sort_control>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o SORTKEYLIST - ....will be defined....
  o CTRL_ISCRITICAL - ....will be defined....
  o CTRLP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_create_sort_control($ld,$sortKeyList,$ctrl_iscritical,$ctrlp);

=item B<ldap_create_sort_keylist>

Description:

  Blah...Blah...

Input:
  o SORTKEYLIST - ....will be defined....
  o STRING_REP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_create_sort_keylist($sortKeyList,$string_rep);

=item B<ldap_create_virtuallist_control>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o LDVLISTP - ....will be defined....
  o CTRLP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_create_virtuallist_control($ld,$ldvlistp,$ctrlp);

=item B<ldap_delete>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_delete($ld,$dn);

=item B<ldap_delete_ext>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o MSGIDP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_delete_ext($ld,$dn,$serverctrls,$clientctrls,$msgidp);

=item B<ldap_delete_ext_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_delete_ext_s($ld,$dn,$serverctrls,$clientctrls);

=item B<ldap_delete_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_delete_s($ld,$dn);

=item B<ldap_dn2ufn>

Description:

  Blah...Blah...

Input:
  o DN - Distinguished Name

Output:

  o char *

Availability: v2/v3

Example:

  $status = ldap_dn2ufn($dn);

=item B<ldap_err2string>

Description:

  Blah...Blah...

Input:
  o ERR - ....will be defined....

Output:

  o char *

Availability: v2/v3

Example:

  $status = ldap_err2string($err);

=item B<ldap_explode_dn>

Description:

  Blah...Blah...

Input:
  o DN - Distinguished Name
  o NOTYPES - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_explode_dn($dn,$notypes);

=item B<ldap_explode_rdn>

Description:

  Blah...Blah...

Input:
  o DN - Distinguished Name
  o NOTYPES - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_explode_rdn($dn,$notypes);

=item B<ldap_extended_operation>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o REQUESTOID - ....will be defined....
  o REQUESTDATA - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o MSGIDP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_extended_operation($ld,$requestoid,$requestdata,$serverctrls,$clientctrls,$msgidp);

=item B<ldap_extended_operation_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o REQUESTOID - ....will be defined....
  o REQUESTDATA - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o RETOIDP - ....will be defined....
  o RETDATAP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_extended_operation_s($ld,$requestoid,$requestdata,$serverctrls,$clientctrls,$retoidp,$retdatap);

=item B<ldap_first_attribute>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o ENTRY - ....will be defined....
  o BER - ....will be defined....

Output:

  o char *

Availability: v2/v3

Example:

  $status = ldap_first_attribute($ld,$entry,$ber);

=item B<ldap_first_entry>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o CHAIN - ....will be defined....

Output:

  o LDAPMessage *

Availability: v2/v3

Example:

  $status = ldap_first_entry($ld,$chain);

=item B<ldap_first_message>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o RES - ....will be defined....

Output:

  o LDAPMessage *

Availability: v2/v3

Example:

  $status = ldap_first_message($ld,$res);

=item B<ldap_first_reference>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o RES - ....will be defined....

Output:

  o LDAPMessage *

Availability: v2/v3

Example:

  $status = ldap_first_reference($ld,$res);

=item B<ldap_free_friendlymap>

Description:

  Blah...Blah...

Input:
  o MAP - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_free_friendlymap($map);

=item B<ldap_free_sort_keylist>

Description:

  Blah...Blah...

Input:
  o SORTKEYLIST - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_free_sort_keylist($sortKeyList);

=item B<ldap_free_urldesc>

Description:

  Blah...Blah...

Input:
  o LUDP - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_free_urldesc($ludp);

=item B<ldap_friendly_name>

Description:

  Blah...Blah...

Input:
  o FILENAME - ....will be defined....
  o NAME - ....will be defined....
  o MAP - ....will be defined....

Output:

  o char *

Availability: v2/v3

Example:

  $status = ldap_friendly_name($filename,$name,$map);

=item B<ldap_get_dn>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o ENTRY - ....will be defined....

Output:

  o char *

Availability: v2/v3

Example:

  $status = ldap_get_dn($ld,$entry);

=item B<ldap_get_entry_controls>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o ENTRY - ....will be defined....
  o SERVERCTRLSP - LDAP Control

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_get_entry_controls($ld,$entry,$serverctrlsp);

=item B<ldap_getfilter_free>

Description:

  Blah...Blah...

Input:
  o LFDP - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_getfilter_free($lfdp);

=item B<ldap_getfirstfilter>

Description:

  Blah...Blah...

Input:
  o LFDP - ....will be defined....
  o TAGPAT - ....will be defined....
  o VALUE - ....will be defined....

Output:

  o LDAPFiltInfo *

Availability: v2/v3

Example:

  $status = ldap_getfirstfilter($lfdp,$tagpat,$value);

=item B<ldap_get_lang_values>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o ENTRY - ....will be defined....
  o TARGET - ....will be defined....
  o TYPE - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_get_lang_values($ld,$entry,$target,$type);

=item B<ldap_get_lang_values_len>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o ENTRY - ....will be defined....
  o TARGET - ....will be defined....
  o TYPE - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_get_lang_values_len($ld,$entry,$target,$type);

=item B<ldap_get_lderrno>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o  ... - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_get_lderrno($ld,$ ...);

=item B<ldap_getnextfilter>

Description:

  Blah...Blah...

Input:
  o LFDP - ....will be defined....

Output:

  o LDAPFiltInfo *

Availability: v2/v3

Example:

  $status = ldap_getnextfilter($lfdp);

=item B<ldap_get_option>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o OPTION - ....will be defined....
  o OPTDATA - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_get_option($ld,$option,$optdata);

=item B<ldap_get_values>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o ENTRY - ....will be defined....
  o TARGET - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_get_values($ld,$entry,$target);

=item B<ldap_get_values_len>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o ENTRY - ....will be defined....
  o TARGET - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_get_values_len($ld,$entry,$target);

=item B<ldap_init>

Description:

  Blah...Blah...

Input:
  o HOST - ....will be defined....
  o PORT - ....will be defined....

Output:

  o LDAP *

Availability: v2/v3

Example:

  $status = ldap_init($host,$port);

=item B<ldap_init_getfilter>

Description:

  Blah...Blah...

Input:
  o FNAME - ....will be defined....

Output:

  o LDAPFiltDesc *

Availability: v2/v3

Example:

  $status = ldap_init_getfilter($fname);

=item B<ldap_init_getfilter_buf>

Description:

  Blah...Blah...

Input:
  o BUF - ....will be defined....
  o BUFLEN - ....will be defined....

Output:

  o LDAPFiltDesc *

Availability: v2/v3

Example:

  $status = ldap_init_getfilter_buf($buf,$buflen);

=item B<ldap_is_ldap_url>

Description:

  Blah...Blah...

Input:
  o URL - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_is_ldap_url($url);

=item B<ldap_memcache_destroy>

Description:

  Blah...Blah...

Input:
  o CACHE - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_memcache_destroy($cache);

=item B<ldap_memcache_flush>

Description:

  Blah...Blah...

Input:
  o CACHE - ....will be defined....
  o DN - Distinguished Name
  o SCOPE - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_memcache_flush($cache,$dn,$scope);

=item B<ldap_memcache_get>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o CACHEP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_memcache_get($ld,$cachep);

=item B<ldap_memcache_init>

Description:

  Blah...Blah...

Input:
  o TTL - ....will be defined....
  o SIZE - ....will be defined....
  o BASEDNS - ....will be defined....
  o CACHEP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_memcache_init($ttl,$size,$baseDNs,$cachep);

=item B<ldap_memcache_set>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o CACHE - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_memcache_set($ld,$cache);

=item B<ldap_memcache_update>

Description:

  Blah...Blah...

Input:
  o CACHE - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_memcache_update($cache);

=item B<ldap_memfree>

Description:

  Blah...Blah...

Input:
  o P - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_memfree($p);

=item B<ldap_modify>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o MODS - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_modify($ld,$dn,$mods);

=item B<ldap_modify_ext>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o MODS - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o MSGIDP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_modify_ext($ld,$dn,$mods,$serverctrls,$clientctrls,$msgidp);

=item B<ldap_modify_ext_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o MODS - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_modify_ext_s($ld,$dn,$mods,$serverctrls,$clientctrls);

=item B<ldap_modify_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o MODS - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_modify_s($ld,$dn,$mods);

=item B<ldap_modrdn>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o NEWRDN - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_modrdn($ld,$dn,$newrdn);

=item B<ldap_modrdn_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o NEWRDN - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_modrdn_s($ld,$dn,$newrdn);

=item B<ldap_modrdn2>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o NEWRDN - ....will be defined....
  o DELETEOLDRDN - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_modrdn2($ld,$dn,$newrdn,$deleteoldrdn);

=item B<ldap_modrdn2_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o NEWRDN - ....will be defined....
  o DELETEOLDRDN - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_modrdn2_s($ld,$dn,$newrdn,$deleteoldrdn);

=item B<ldap_mods_free>

Description:

  Blah...Blah...

Input:
  o MODS - ....will be defined....
  o FREEMODS - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_mods_free($mods,$freemods);

=item B<ldap_msgfree>

Description:

  Blah...Blah...

Input:
  o LM - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_msgfree($lm);

=item B<ldap_msgid>

Description:

  Blah...Blah...

Input:
  o LM - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_msgid($lm);

=item B<ldap_msgtype>

Description:

  Blah...Blah...

Input:
  o LM - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_msgtype($lm);

=item B<ldap_multisort_entries>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o CHAIN - ....will be defined....
  o ATTR - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_multisort_entries($ld,$chain,$attr);

=item B<ldap_next_attribute>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o ENTRY - ....will be defined....
  o BER - ....will be defined....

Output:

  o char *

Availability: v2/v3

Example:

  $status = ldap_next_attribute($ld,$entry,$ber);

=item B<ldap_next_entry>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o ENTRY - ....will be defined....

Output:

  o LDAPMessage *

Availability: v2/v3

Example:

  $status = ldap_next_entry($ld,$entry);

=item B<ldap_next_message>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o MSG - ....will be defined....

Output:

  o LDAPMessage *

Availability: v2/v3

Example:

  $status = ldap_next_message($ld,$msg);

=item B<ldap_next_reference>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o REF - ....will be defined....

Output:

  o LDAPMessage *

Availability: v2/v3

Example:

  $status = ldap_next_reference($ld,$ref);

=item B<ldap_parse_entrychange_control>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o CTRLS - LDAP Control
  o CHGTYPEP - ....will be defined....
  o PREVDNP - ....will be defined....
  o CHGNUMPRESENTP - ....will be defined....
  o CHGNUMP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_parse_entrychange_control($ld,$ctrls,$chgtypep,$prevdnp,$chgnumpresentp,$chgnump);

=item B<ldap_parse_extended_result>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o RES - ....will be defined....
  o RETOIDP - ....will be defined....
  o RETDATAP - ....will be defined....
  o FREEIT - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_parse_extended_result($ld,$res,$retoidp,$retdatap,$freeit);

=item B<ldap_parse_reference>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o REF - ....will be defined....
  o REFERALSP - ....will be defined....
  o SERVERCTRLSP - LDAP Control
  o FREEIT - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_parse_reference($ld,$ref,$referalsp,$serverctrlsp,$freeit);

=item B<ldap_parse_result>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o RES - ....will be defined....
  o ERRCODEP - ....will be defined....
  o MATCHEDDNP - ....will be defined....
  o ERRMSGP - ....will be defined....
  o REFERRALSP - ....will be defined....
  o SERVERCTRLSP - LDAP Control
  o FREEIT - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_parse_result($ld,$res,$errcodep,$matcheddnp,$errmsgp,$referralsp,$serverctrlsp,$freeit);

=item B<ldap_parse_sasl_bind_result>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o RES - ....will be defined....
  o SERVERCREDP - ....will be defined....
  o FREEIT - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_parse_sasl_bind_result($ld,$res,$servercredp,$freeit);

=item B<ldap_parse_sort_control>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o CTRLS - LDAP Control
  o RESULT - ....will be defined....
  o ATTRIBUTE - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_parse_sort_control($ld,$ctrls,$result,$attribute);

=item B<ldap_parse_virtuallist_control>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o CTRLS - LDAP Control
  o TARGET_POSP - ....will be defined....
  o LIST_SIZEP - ....will be defined....
  o ERRCODEP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_parse_virtuallist_control($ld,$ctrls,$target_posp,$list_sizep,$errcodep);

=item B<ldap_perror>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o S - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_perror($ld,$s);

=item B<ldap_rename>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o NEWRDN - ....will be defined....
  o NEWPARENT - ....will be defined....
  o DELETEOLDRDN - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o MSGIDP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_rename($ld,$dn,$newrdn,$newparent,$deleteoldrdn,$serverctrls,$clientctrls,$msgidp);

=item B<ldap_rename_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o NEWRDN - ....will be defined....
  o NEWPARENT - ....will be defined....
  o DELETEOLDRDN - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_rename_s($ld,$dn,$newrdn,$newparent,$deleteoldrdn,$serverctrls,$clientctrls);

=item B<ldap_result>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o MSGID - LDAP Message ID
  o ALL - ....will be defined....
  o TIMEOUT - ....will be defined....
  o RESULT - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_result($ld,$msgid,$all,$timeout,$result);

=item B<ldap_result2error>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o R - ....will be defined....
  o FREEIT - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_result2error($ld,$r,$freeit);

=item B<ldap_sasl_bind>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o MECHANISM - ....will be defined....
  o CRED - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o MSGIDP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_sasl_bind($ld,$dn,$mechanism,$cred,$serverctrls,$clientctrls,$msgidp);

=item B<ldap_sasl_bind_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o DN - Distinguished Name
  o MECHANISM - ....will be defined....
  o CRED - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o SERVERCREDP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_sasl_bind_s($ld,$dn,$mechanism,$cred,$serverctrls,$clientctrls,$servercredp);

=item B<ldap_search>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o BASE - ....will be defined....
  o SCOPE - ....will be defined....
  o FILTER - ....will be defined....
  o ATTRS - Attribute List Reference
  o ATTRSONLY - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_search($ld,$base,$scope,$filter,$attrs,$attrsonly);

=item B<ldap_search_ext>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o BASE - ....will be defined....
  o SCOPE - ....will be defined....
  o FILTER - ....will be defined....
  o ATTRS - Attribute List Reference
  o ATTRSONLY - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o TIMEOUTP - ....will be defined....
  o SIZELIMIT - ....will be defined....
  o MSGIDP - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_search_ext($ld,$base,$scope,$filter,$attrs,$attrsonly,$serverctrls,$clientctrls,$timeoutp,$sizelimit,$msgidp);

=item B<ldap_search_ext_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o BASE - ....will be defined....
  o SCOPE - ....will be defined....
  o FILTER - ....will be defined....
  o ATTRS - Attribute List Reference
  o ATTRSONLY - ....will be defined....
  o SERVERCTRLS - LDAP Control
  o CLIENTCTRLS - LDAP Control
  o TIMEOUTP - ....will be defined....
  o SIZELIMIT - ....will be defined....
  o RES - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_search_ext_s($ld,$base,$scope,$filter,$attrs,$attrsonly,$serverctrls,$clientctrls,$timeoutp,$sizelimit,$res);

=item B<ldap_search_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o BASE - ....will be defined....
  o SCOPE - ....will be defined....
  o FILTER - ....will be defined....
  o ATTRS - Attribute List Reference
  o ATTRSONLY - ....will be defined....
  o RES - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_search_s($ld,$base,$scope,$filter,$attrs,$attrsonly,$res);

=item B<ldap_search_st>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o BASE - ....will be defined....
  o SCOPE - ....will be defined....
  o FILTER - ....will be defined....
  o ATTRS - Attribute List Reference
  o ATTRSONLY - ....will be defined....
  o TIMEOUT - ....will be defined....
  o RES - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_search_st($ld,$base,$scope,$filter,$attrs,$attrsonly,$timeout,$res);

=item B<ldap_set_filter_additions>

Description:

  Blah...Blah...

Input:
  o LFDP - ....will be defined....
  o PREFIX - ....will be defined....
  o SUFFIX - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_set_filter_additions($lfdp,$prefix,$suffix);

=item B<ldap_set_lderrno>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o E - ....will be defined....
  o M - ....will be defined....
  o S - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_set_lderrno($ld,$e,$m,$s);

=item B<ldap_set_option>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o OPTION - ....will be defined....
  o OPTDATA - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_set_option($ld,$option,$optdata);

=item B<ldap_set_rebind_proc>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o REBINDPROC - ....will be defined....

Output:

  o void

Availability: v2/v3

Example:

  $status = ldap_set_rebind_proc($ld,$rebindproc);

=item B<ldap_simple_bind>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o WHO - ....will be defined....
  o PASSWD - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_simple_bind($ld,$who,$passwd);

=item B<ldap_simple_bind_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o WHO - ....will be defined....
  o PASSWD - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_simple_bind_s($ld,$who,$passwd);

=item B<ldap_sort_entries>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o CHAIN - ....will be defined....
  o ATTR - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_sort_entries($ld,$chain,$attr);

=item B<ldap_unbind>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_unbind($ld);

=item B<ldap_unbind_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_unbind_s($ld);

=item B<ldap_url_parse>

Description:

  Blah...Blah...

Input:
  o URL - ....will be defined....

Output:

  o SV *

Availability: v2/v3

Example:

  $status = ldap_url_parse($url);

=item B<ldap_url_search>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o URL - ....will be defined....
  o ATTRSONLY - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_url_search($ld,$url,$attrsonly);

=item B<ldap_url_search_s>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o URL - ....will be defined....
  o ATTRSONLY - ....will be defined....
  o RES - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_url_search_s($ld,$url,$attrsonly,$res);

=item B<ldap_url_search_st>

Description:

  Blah...Blah...

Input:
  o LD - LDAP Connection Handle
  o URL - ....will be defined....
  o ATTRSONLY - ....will be defined....
  o TIMEOUT - ....will be defined....
  o RES - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_url_search_st($ld,$url,$attrsonly,$timeout,$res);

=item B<ldap_version>

Description:

  Blah...Blah...

Input:
  o VER - ....will be defined....

Output:

  o int

Availability: v2/v3

Example:

  $status = ldap_version($ver);

=head1 AUTHOR INFORMATION

Address bug reports and comments to:
xxx@netscape.com

=head1 CREDITS

Most of the Perl API code was developed by Clayton Donley.

=head1 BUGS

LDAPv3 calls have not been tested at this point.  Use them at your own risk.

=head1 SEE ALSO

L<Mozilla::LDAP::Conn>, L<Mozilla::LDAP::Entry>, L<Mozilla::LDAP::LDIF>,
and of course L<Perl>.

=cut
