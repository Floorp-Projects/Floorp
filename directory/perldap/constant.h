/*
 *******************************************************************************
 * $Id: constant.h,v 1.8 2000/10/05 19:47:32 leif%netscape.com Exp $
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is PerLDAP. The Initial Developer of the Original
 * Code is Netscape Communications Corp. and Clayton Donley. Portions
 * created by Netscape are Copyright (C) Netscape Communications
 * Corp., portions created by Clayton Donley are Copyright (C) Clayton
 * Donley. All Rights Reserved.
 *
 * Contributor(s):
 *
 * DESCRIPTION
 *    Constants.
 *
 *******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef __cplusplus
}
#endif

#include <ldap.h>

static int
not_here(s)
char *s;
{
    croak("%s not implemented on this architecture", s);
    return -1;
}

double
constant(name, arg)
char *name;
int arg;
{
    errno = 0;
    if (name[0] == 'L' && name[1] == 'D' && name[2] == 'A' && name[3] == 'P'
       && name[4] == '_')
    {
      switch (name[5]) {
       case 'A':
	if (strEQ(name, "LDAP_ADMINLIMIT_EXCEEDED"))
#ifdef LDAP_ADMINLIMIT_EXCEEDED
	    return LDAP_ADMINLIMIT_EXCEEDED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_AFFECTS_MULTIPLE_DSAS"))
#ifdef LDAP_AFFECTS_MULTIPLE_DSAS
	    return LDAP_AFFECTS_MULTIPLE_DSAS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_ALIAS_DEREF_PROBLEM"))
#ifdef LDAP_ALIAS_DEREF_PROBLEM
	    return LDAP_ALIAS_DEREF_PROBLEM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_ALIAS_PROBLEM"))
#ifdef LDAP_ALIAS_PROBLEM
	    return LDAP_ALIAS_PROBLEM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_ALREADY_EXISTS"))
#ifdef LDAP_ALREADY_EXISTS
	    return LDAP_ALREADY_EXISTS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_AUTH_METHOD_NOT_SUPPORTED"))
#ifdef LDAP_AUTH_METHOD_NOT_SUPPORTED
	    return LDAP_AUTH_METHOD_NOT_SUPPORTED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_AUTH_NONE"))
#ifdef LDAP_AUTH_NONE
	    return LDAP_AUTH_NONE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_AUTH_SASL"))
#ifdef LDAP_AUTH_SASL
	    return LDAP_AUTH_SASL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_AUTH_SIMPLE"))
#ifdef LDAP_AUTH_SIMPLE
	    return LDAP_AUTH_SIMPLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_AUTH_UNKNOWN"))
#ifdef LDAP_AUTH_UNKNOWN
	    return LDAP_AUTH_UNKNOWN;
#else
	    goto not_there;
#endif
        break;
      case 'B':
	if (strEQ(name, "LDAP_BUSY"))
#ifdef LDAP_BUSY
	    return LDAP_BUSY;
#else
	    goto not_there;
#endif
	break;
      case 'C':
	if (strEQ(name, "LDAP_CACHE_CHECK"))
#ifdef LDAP_CACHE_CHECK
	    return LDAP_CACHE_CHECK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CACHE_LOCALDB"))
#ifdef LDAP_CACHE_LOCALDB
	    return LDAP_CACHE_LOCALDB;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CACHE_POPULATE"))
#ifdef LDAP_CACHE_POPULATE
	    return LDAP_CACHE_POPULATE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CHANGETYPE_ADD"))
#ifdef LDAP_CHANGETYPE_ADD
	    return LDAP_CHANGETYPE_ADD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CHANGETYPE_ANY"))
#ifdef LDAP_CHANGETYPE_ANY
	    return LDAP_CHANGETYPE_ANY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CHANGETYPE_DELETE"))
#ifdef LDAP_CHANGETYPE_DELETE
	    return LDAP_CHANGETYPE_DELETE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CHANGETYPE_MODDN"))
#ifdef LDAP_CHANGETYPE_MODDN
	    return LDAP_CHANGETYPE_MODDN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CHANGETYPE_MODIFY"))
#ifdef LDAP_CHANGETYPE_MODIFY
	    return LDAP_CHANGETYPE_MODIFY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CLIENT_LOOP"))
#ifdef LDAP_CLIENT_LOOP
	    return LDAP_CLIENT_LOOP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_COMPARE_FALSE"))
#ifdef LDAP_COMPARE_FALSE
	    return LDAP_COMPARE_FALSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_COMPARE_TRUE"))
#ifdef LDAP_COMPARE_TRUE
	    return LDAP_COMPARE_TRUE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CONFIDENTIALITY_REQUIRED"))
#ifdef LDAP_CONFIDENTIALITY_REQUIRED
	    return LDAP_CONFIDENTIALITY_REQUIRED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CONNECT_ERROR"))
#ifdef LDAP_CONNECT_ERROR
	    return LDAP_CONNECT_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_CONSTRAINT_VIOLATION"))
#ifdef LDAP_CONSTRAINT_VIOLATION
	    return LDAP_CONSTRAINT_VIOLATION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_DECODING_ERROR"))
#ifdef LDAP_DECODING_ERROR
	    return LDAP_DECODING_ERROR;
#else
	    goto not_there;
#endif
	break;
      case 'D':
	if (strEQ(name, "LDAP_DEREF_ALWAYS"))
#ifdef LDAP_DEREF_ALWAYS
	    return LDAP_DEREF_ALWAYS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_DEREF_FINDING"))
#ifdef LDAP_DEREF_FINDING
	    return LDAP_DEREF_FINDING;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_DEREF_NEVER"))
#ifdef LDAP_DEREF_NEVER
	    return LDAP_DEREF_NEVER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_DEREF_SEARCHING"))
#ifdef LDAP_DEREF_SEARCHING
	    return LDAP_DEREF_SEARCHING;
#else
	    goto not_there;
#endif
	break;
      case 'E':
	if (strEQ(name, "LDAP_ENCODING_ERROR"))
#ifdef LDAP_ENCODING_ERROR
	    return LDAP_ENCODING_ERROR;
#else
	    goto not_there;
#endif
	break;
      case 'F':
	if (strEQ(name, "LDAP_FILTER_ERROR"))
#ifdef LDAP_FILTER_ERROR
	    return LDAP_FILTER_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_FILT_MAXSIZ"))
#ifdef LDAP_FILT_MAXSIZ
	    return LDAP_FILT_MAXSIZ;
#else
	    goto not_there;
#endif
	break;
      case 'I':
	if (strEQ(name, "LDAP_INAPPROPRIATE_AUTH"))
#ifdef LDAP_INAPPROPRIATE_AUTH
	    return LDAP_INAPPROPRIATE_AUTH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_INAPPROPRIATE_MATCHING"))
#ifdef LDAP_INAPPROPRIATE_MATCHING
	    return LDAP_INAPPROPRIATE_MATCHING;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_INSUFFICIENT_ACCESS"))
#ifdef LDAP_INSUFFICIENT_ACCESS
	    return LDAP_INSUFFICIENT_ACCESS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_INVALID_CREDENTIALS"))
#ifdef LDAP_INVALID_CREDENTIALS
	    return LDAP_INVALID_CREDENTIALS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_INVALID_DN_SYNTAX"))
#ifdef LDAP_INVALID_DN_SYNTAX
	    return LDAP_INVALID_DN_SYNTAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_INVALID_SYNTAX"))
#ifdef LDAP_INVALID_SYNTAX
	    return LDAP_INVALID_SYNTAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_IS_LEAF"))
#ifdef LDAP_IS_LEAF
	    return LDAP_IS_LEAF;
#else
	    goto not_there;
#endif
	break;
      case 'L':
	if (strEQ(name, "LDAP_LOCAL_ERROR"))
#ifdef LDAP_LOCAL_ERROR
	    return LDAP_LOCAL_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_LOOP_DETECT"))
#ifdef LDAP_LOOP_DETECT
	    return LDAP_LOOP_DETECT;
#else
	    goto not_there;
#endif
	break;
      case 'M':
	if (strEQ(name, "LDAP_MOD_ADD"))
#ifdef LDAP_MOD_ADD
	    return LDAP_MOD_ADD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_MOD_BVALUES"))
#ifdef LDAP_MOD_BVALUES
	    return LDAP_MOD_BVALUES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_MOD_DELETE"))
#ifdef LDAP_MOD_DELETE
	    return LDAP_MOD_DELETE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_MOD_REPLACE"))
#ifdef LDAP_MOD_REPLACE
	    return LDAP_MOD_REPLACE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_MORE_RESULTS_TO_RETURN"))
#ifdef LDAP_MORE_RESULTS_TO_RETURN
	    return LDAP_MORE_RESULTS_TO_RETURN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_MSG_ALL"))
#ifdef LDAP_MSG_ALL
	    return LDAP_MSG_ALL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_MSG_ONE"))
#ifdef LDAP_MSG_ONE
	    return LDAP_MSG_ONE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_MSG_RECEIVED"))
#ifdef LDAP_MSG_RECEIVED
	    return LDAP_MSG_RECEIVED;
#else
	    goto not_there;
#endif
	break;
      case 'N':
	if (strEQ(name, "LDAP_NAMING_VIOLATION"))
#ifdef LDAP_NAMING_VIOLATION
	    return LDAP_NAMING_VIOLATION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_NOT_ALLOWED_ON_NONLEAF"))
#ifdef LDAP_NOT_ALLOWED_ON_NONLEAF
	    return LDAP_NOT_ALLOWED_ON_NONLEAF;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_NOT_ALLOWED_ON_RDN"))
#ifdef LDAP_NOT_ALLOWED_ON_RDN
	    return LDAP_NOT_ALLOWED_ON_RDN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_NOT_SUPPORTED"))
#ifdef LDAP_NOT_SUPPORTED
	    return LDAP_NOT_SUPPORTED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_NO_LIMIT"))
#ifdef LDAP_NO_LIMIT
	    return LDAP_NO_LIMIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_NO_MEMORY"))
#ifdef LDAP_NO_MEMORY
	    return LDAP_NO_MEMORY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_NO_OBJECT_CLASS_MODS"))
#ifdef LDAP_NO_OBJECT_CLASS_MODS
	    return LDAP_NO_OBJECT_CLASS_MODS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_NO_RESULTS_RETURNED"))
#ifdef LDAP_NO_RESULTS_RETURNED
	    return LDAP_NO_RESULTS_RETURNED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_NO_SUCH_ATTRIBUTE"))
#ifdef LDAP_NO_SUCH_ATTRIBUTE
	    return LDAP_NO_SUCH_ATTRIBUTE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_NO_SUCH_OBJECT"))
#ifdef LDAP_NO_SUCH_OBJECT
	    return LDAP_NO_SUCH_OBJECT;
#else
	    goto not_there;
#endif
	break;
      case 'O':
	if (strEQ(name, "LDAP_OBJECT_CLASS_VIOLATION"))
#ifdef LDAP_OBJECT_CLASS_VIOLATION
	    return LDAP_OBJECT_CLASS_VIOLATION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPERATIONS_ERROR"))
#ifdef LDAP_OPERATIONS_ERROR
	    return LDAP_OPERATIONS_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_CACHE_ENABLE"))
#ifdef LDAP_OPT_CACHE_ENABLE
	    return LDAP_OPT_CACHE_ENABLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_CACHE_FN_PTRS"))
#ifdef LDAP_OPT_CACHE_FN_PTRS
	    return LDAP_OPT_CACHE_FN_PTRS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_CACHE_STRATEGY"))
#ifdef LDAP_OPT_CACHE_STRATEGY
	    return LDAP_OPT_CACHE_STRATEGY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_CLIENT_CONTROLS"))
#ifdef LDAP_OPT_CLIENT_CONTROLS
	    return LDAP_OPT_CLIENT_CONTROLS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_DEREF"))
#ifdef LDAP_OPT_DEREF
	    return LDAP_OPT_DEREF;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_DESC"))
#ifdef LDAP_OPT_DESC
	    return LDAP_OPT_DESC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_DNS"))
#ifdef LDAP_OPT_DNS
	    return LDAP_OPT_DNS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_DNS_FN_PTRS"))
#ifdef LDAP_OPT_DNS_FN_PTRS
	    return LDAP_OPT_DNS_FN_PTRS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_ERROR_NUMBER"))
#ifdef LDAP_OPT_ERROR_NUMBER
	    return LDAP_OPT_ERROR_NUMBER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_ERROR_STRING"))
#ifdef LDAP_OPT_ERROR_STRING
	    return LDAP_OPT_ERROR_STRING;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_HOST_NAME"))
#ifdef LDAP_OPT_HOST_NAME
	    return LDAP_OPT_HOST_NAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_IO_FN_PTRS"))
#ifdef LDAP_OPT_IO_FN_PTRS
	    return LDAP_OPT_IO_FN_PTRS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_MEMALLOC_FN_PTRS"))
#ifdef LDAP_OPT_MEMALLOC_FN_PTRS
	    return LDAP_OPT_MEMALLOC_FN_PTRS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_PREFERRED_LANGUAGE"))
#ifdef LDAP_OPT_PREFERRED_LANGUAGE
	    return LDAP_OPT_PREFERRED_LANGUAGE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_PROTOCOL_VERSION"))
#ifdef LDAP_OPT_PROTOCOL_VERSION
	    return LDAP_OPT_PROTOCOL_VERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_REBIND_ARG"))
#ifdef LDAP_OPT_REBIND_ARG
	    return LDAP_OPT_REBIND_ARG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_REBIND_FN"))
#ifdef LDAP_OPT_REBIND_FN
	    return LDAP_OPT_REBIND_FN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_RECONNECT"))
#ifdef LDAP_OPT_RECONNECT
	    return LDAP_OPT_RECONNECT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_REFERRALS"))
#ifdef LDAP_OPT_REFERRALS
	    return LDAP_OPT_REFERRALS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_REFERRAL_HOP_LIMIT"))
#ifdef LDAP_OPT_REFERRAL_HOP_LIMIT
	    return LDAP_OPT_REFERRAL_HOP_LIMIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_RESTART"))
#ifdef LDAP_OPT_RESTART
	    return LDAP_OPT_RESTART;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_RETURN_REFERRALS"))
#ifdef LDAP_OPT_RETURN_REFERRALS
	    return LDAP_OPT_RETURN_REFERRALS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_SERVER_CONTROLS"))
#ifdef LDAP_OPT_SERVER_CONTROLS
	    return LDAP_OPT_SERVER_CONTROLS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_SIZELIMIT"))
#ifdef LDAP_OPT_SIZELIMIT
	    return LDAP_OPT_SIZELIMIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_SSL"))
#ifdef LDAP_OPT_SSL
	    return LDAP_OPT_SSL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_THREAD_FN_PTRS"))
#ifdef LDAP_OPT_THREAD_FN_PTRS
	    return LDAP_OPT_THREAD_FN_PTRS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OPT_TIMELIMIT"))
#ifdef LDAP_OPT_TIMELIMIT
	    return LDAP_OPT_TIMELIMIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_OTHER"))
#ifdef LDAP_OTHER
	    return LDAP_OTHER;
#else
	    goto not_there;
#endif
	break;
      case 'P':
	if (strEQ(name, "LDAP_PARAM_ERROR"))
#ifdef LDAP_PARAM_ERROR
	    return LDAP_PARAM_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_PARTIAL_RESULTS"))
#ifdef LDAP_PARTIAL_RESULTS
	    return LDAP_PARTIAL_RESULTS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_PORT"))
#ifdef LDAP_PORT
	    return LDAP_PORT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_PORT_MAX"))
#ifdef LDAP_PORT_MAX
	    return LDAP_PORT_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_PROTOCOL_ERROR"))
#ifdef LDAP_PROTOCOL_ERROR
	    return LDAP_PROTOCOL_ERROR;
#else
	    goto not_there;
#endif
	break;
      case 'R':
	if (strEQ(name, "LDAP_REFERRAL"))
#ifdef LDAP_REFERRAL
	    return LDAP_REFERRAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_REFERRAL_LIMIT_EXCEEDED"))
#ifdef LDAP_REFERRAL_LIMIT_EXCEEDED
	    return LDAP_REFERRAL_LIMIT_EXCEEDED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RESULTS_TOO_LARGE"))
#ifdef LDAP_RESULTS_TOO_LARGE
	    return LDAP_RESULTS_TOO_LARGE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_ADD"))
#ifdef LDAP_RES_ADD
	    return LDAP_RES_ADD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_ANY"))
#ifdef LDAP_RES_ANY
	    return LDAP_RES_ANY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_BIND"))
#ifdef LDAP_RES_BIND
	    return LDAP_RES_BIND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_COMPARE"))
#ifdef LDAP_RES_COMPARE
	    return LDAP_RES_COMPARE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_DELETE"))
#ifdef LDAP_RES_DELETE
	    return LDAP_RES_DELETE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_EXTENDED"))
#ifdef LDAP_RES_EXTENDED
	    return LDAP_RES_EXTENDED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_MODIFY"))
#ifdef LDAP_RES_MODIFY
	    return LDAP_RES_MODIFY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_MODRDN"))
#ifdef LDAP_RES_MODRDN
	    return LDAP_RES_MODRDN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_RENAME"))
#ifdef LDAP_RES_RENAME
	    return LDAP_RES_RENAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_SEARCH_ENTRY"))
#ifdef LDAP_RES_SEARCH_ENTRY
	    return LDAP_RES_SEARCH_ENTRY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_SEARCH_REFERENCE"))
#ifdef LDAP_RES_SEARCH_REFERENCE
	    return LDAP_RES_SEARCH_REFERENCE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_RES_SEARCH_RESULT"))
#ifdef LDAP_RES_SEARCH_RESULT
	    return LDAP_RES_SEARCH_RESULT;
#else
	    goto not_there;
#endif
	break;
      case 'S':
	if (strEQ(name, "LDAP_SASL_BIND_IN_PROGRESS"))
#ifdef LDAP_SASL_BIND_IN_PROGRESS
	    return LDAP_SASL_BIND_IN_PROGRESS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_SASL_SIMPLE"))
#ifdef LDAP_SASL_SIMPLE
	    return LDAP_SASL_SIMPLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_SCOPE_BASE"))
#ifdef LDAP_SCOPE_BASE
	    return LDAP_SCOPE_BASE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_SCOPE_ONELEVEL"))
#ifdef LDAP_SCOPE_ONELEVEL
	    return LDAP_SCOPE_ONELEVEL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_SCOPE_SUBTREE"))
#ifdef LDAP_SCOPE_SUBTREE
	    return LDAP_SCOPE_SUBTREE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_SECURITY_NONE"))
#ifdef LDAP_SECURITY_NONE
	    return LDAP_SECURITY_NONE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_SERVER_DOWN"))
#ifdef LDAP_SERVER_DOWN
	    return LDAP_SERVER_DOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_SIZELIMIT_EXCEEDED"))
#ifdef LDAP_SIZELIMIT_EXCEEDED
	    return LDAP_SIZELIMIT_EXCEEDED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_SORT_CONTROL_MISSING"))
#ifdef LDAP_SORT_CONTROL_MISSING
	    return LDAP_SORT_CONTROL_MISSING;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_STRONG_AUTH_NOT_SUPPORTED"))
#ifdef LDAP_STRONG_AUTH_NOT_SUPPORTED
	    return LDAP_STRONG_AUTH_NOT_SUPPORTED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_STRONG_AUTH_REQUIRED"))
#ifdef LDAP_STRONG_AUTH_REQUIRED
	    return LDAP_STRONG_AUTH_REQUIRED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_SUCCESS"))
#ifdef LDAP_SUCCESS
	    return LDAP_SUCCESS;
#else
	    goto not_there;
#endif
	break;
      case 'T':
	if (strEQ(name, "LDAP_TIMELIMIT_EXCEEDED"))
#ifdef LDAP_TIMELIMIT_EXCEEDED
	    return LDAP_TIMELIMIT_EXCEEDED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_TIMEOUT"))
#ifdef LDAP_TIMEOUT
	    return LDAP_TIMEOUT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_TYPE_OR_VALUE_EXISTS"))
#ifdef LDAP_TYPE_OR_VALUE_EXISTS
	    return LDAP_TYPE_OR_VALUE_EXISTS;
#else
	    goto not_there;
#endif
	break;
      case 'U':
	if (strEQ(name, "LDAP_UNAVAILABLE"))
#ifdef LDAP_UNAVAILABLE
	    return LDAP_UNAVAILABLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_UNAVAILABLE_CRITICAL_EXTENSION"))
#ifdef LDAP_UNAVAILABLE_CRITICAL_EXTENSION
	    return LDAP_UNAVAILABLE_CRITICAL_EXTENSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_UNDEFINED_TYPE"))
#ifdef LDAP_UNDEFINED_TYPE
	    return LDAP_UNDEFINED_TYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_UNWILLING_TO_PERFORM"))
#ifdef LDAP_UNWILLING_TO_PERFORM
	    return LDAP_UNWILLING_TO_PERFORM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_URL_ERR_BADSCOPE"))
#ifdef LDAP_URL_ERR_BADSCOPE
	    return LDAP_URL_ERR_BADSCOPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_URL_ERR_MEM"))
#ifdef LDAP_URL_ERR_MEM
	    return LDAP_URL_ERR_MEM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_URL_ERR_NODN"))
#ifdef LDAP_URL_ERR_NODN
	    return LDAP_URL_ERR_NODN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_URL_ERR_NOTLDAP"))
#ifdef LDAP_URL_ERR_NOTLDAP
	    return LDAP_URL_ERR_NOTLDAP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_URL_ERR_PARAM"))
#ifdef LDAP_URL_ERR_PARAM
	    return LDAP_URL_ERR_PARAM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_URL_OPT_SECURE"))
#ifdef LDAP_URL_OPT_SECURE
	    return LDAP_URL_OPT_SECURE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_USER_CANCELLED"))
#ifdef LDAP_USER_CANCELLED
	    return LDAP_USER_CANCELLED;
#else
	    goto not_there;
#endif
	break;
      case 'V':
	if (strEQ(name, "LDAP_VERSION"))
#ifdef LDAP_VERSION
	    return LDAP_VERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_VERSION1"))
#ifdef LDAP_VERSION1
	    return LDAP_VERSION1;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_VERSION2"))
#ifdef LDAP_VERSION2
	    return LDAP_VERSION2;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_VERSION3"))
#ifdef LDAP_VERSION3
	    return LDAP_VERSION3;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LDAP_VERSION_MAX"))
#ifdef LDAP_VERSION_MAX
	    return LDAP_VERSION_MAX;
#else
	    goto not_there;
#endif
	break;
      }
    } else {
	if (strEQ(name, "LDAPS_PORT"))
#ifdef LDAPS_PORT
	    return LDAPS_PORT;
#else
	    goto not_there;
#endif
    }

    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}
