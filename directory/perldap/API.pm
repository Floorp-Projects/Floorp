#############################################################################
# $Id: API.pm,v 1.7 1998/08/03 00:29:25 clayton Exp $
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
# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

Mozilla::LDAP::API - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Mozilla::LDAP::API;
  blah blah blah

=head1 DESCRIPTION


=head1 AUTHOR

=head1 SEE ALSO


=cut
