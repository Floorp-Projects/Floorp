/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */
/*
 *  Copyright (c) 1990 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  getvalues.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"


static void **
internal_ldap_get_values( LDAP *ld, LDAPMessage *entry, const char *target,
	int lencall )
{
	struct berelement	ber;
	char		        *attr;
	int			        rc;
	void			    **vals;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_get_values\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( NULL );	/* punt */
	}
	if ( target == NULL ||
	    !NSLDAPI_VALID_LDAPMESSAGE_ENTRY_POINTER( entry )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( NULL );
	}

	ber = *entry->lm_ber;

	/* skip sequence, dn, sequence of, and snag the first attr */
	if ( ber_scanf( &ber, "{x{{a", &attr ) == LBER_ERROR ) {
		LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
		return( NULL );
	}

	rc = strcasecmp( (char *)target, attr );
	NSLDAPI_FREE( attr );
	if ( rc != 0 ) {
		while ( 1 ) {
			if ( ber_scanf( &ber, "x}{a", &attr ) == LBER_ERROR ) {
				LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR,
				    NULL, NULL );
				return( NULL );
			}

			rc = strcasecmp( (char *)target, attr );
			if ( rc == 0 ) {
				NSLDAPI_FREE( attr );
				break;
			}
			NSLDAPI_FREE( attr );
		}
	}

	/* 
	 * if we get this far, we've found the attribute and are sitting
	 * just before the set of values.
	 */

	if ( lencall ) {
		rc = ber_scanf( &ber, "[V]", &vals );
	} else {
		rc = ber_scanf( &ber, "[v]", &vals );
	}

	if ( rc == LBER_ERROR ) {
		rc = LDAP_DECODING_ERROR;
	} else {
		rc = LDAP_SUCCESS;
	}

	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );

	return(( rc == LDAP_SUCCESS ) ? vals : NULL );
}


/* For language-sensitive attribute matching, we are looking for a
   language tag that looks like one of the following:

   cn
   cn;lang-en
   cn;lang-en-us
   cn;lang-ja
   cn;lang-ja-JP-kanji

   The base language specification consists of two letters following
   "lang-". After that, there may be additional language-specific
   narrowings preceded by a "-". In our processing we go from the
   specific to the general, preferring a complete subtype match, but
   accepting a partial one. For example:

   For a request for "cn;lang-en-us", we would return cn;lang-en-us
   if present, otherwise cn;lang-en if present, otherwise cn.

   Besides the language subtype, there may be other subtypes:

   cn;lang-ja;binary         (Unlikely!)
   cn;lang-ja;phonetic

   If not in the target, they are ignored. If they are in the target,
   they must be in the attribute to match.
*/
#define LANG_SUBTYPE_INDEX_NONE      -1
#define LANG_SUBTYPE_INDEX_DUPLICATE -2

typedef struct {
	int    start;
	int    length;
} _SubStringIndex;

static int
parse_subtypes( const char *target, int *baseLenp, char **langp,
				_SubStringIndex **subs, int *nsubtypes )
{
	int nSubtypes = 0;
	int ind = 0;
	char *nextToken;
 	_SubStringIndex *result = NULL;
	int langIndex;
	int targetLen;
	int subtypeStart;

	langIndex = LANG_SUBTYPE_INDEX_NONE;
	*subs = NULL;
	*langp = NULL;
	*baseLenp = 0;
	*nsubtypes = 0;
	targetLen = strlen( target );

	/* Parse past base attribute */
	nextToken = strchr( target, ';' );
	if ( NULL != nextToken ) {
		subtypeStart = nextToken - target + 1;
		*baseLenp = subtypeStart - 1;
	}
	else {
		subtypeStart = targetLen;
		*baseLenp = subtypeStart;
	}
	ind = subtypeStart;

	/* How many subtypes? */
	nextToken = (char *)target + subtypeStart;
	while ( nextToken && *nextToken ) {
		char *thisToken = nextToken;
		nextToken = strchr( thisToken, ';' );
		if ( NULL != nextToken )
			nextToken++;
		if ( 0 == strncasecmp( thisToken, "lang-", 5 ) ) {
			/* If there was a previous lang tag, this is illegal! */
			if ( langIndex != LANG_SUBTYPE_INDEX_NONE ) {
				langIndex = LANG_SUBTYPE_INDEX_DUPLICATE;
				return langIndex;
			}
			else {
				langIndex = nSubtypes;
			}
		} else {
			nSubtypes++;
		}
	}
	/* No language subtype? */
	if ( langIndex < 0 )
		return langIndex;

	/* Allocate array of non-language subtypes */
	if ( nSubtypes > 0 ) {
		result = (_SubStringIndex *)NSLDAPI_MALLOC( sizeof(*result)
		    * nSubtypes );
		memset( result, 0, sizeof(*result) * nSubtypes );
	}
	ind = 0;
	nSubtypes = 0;
	ind = subtypeStart;
	nextToken = (char *)target + subtypeStart;
	while ( nextToken && *nextToken ) {
		char *thisToken = nextToken;
		int len;
		nextToken = strchr( thisToken, ';' );
		if ( NULL != nextToken ) {
			len = nextToken - thisToken;
			nextToken++;
		}
		else {
			nextToken = (char *)target + targetLen;
			len = nextToken - thisToken;
		}
		if ( 0 == strncasecmp( thisToken, "lang-", 5 ) ) {
			int i;
			*langp = (char *)NSLDAPI_MALLOC( len + 1 );
			for( i = 0; i < len; i++ )
				(*langp)[i] = toupper( target[ind+i] );
			(*langp)[len] = 0;
		}
		else {
			result[nSubtypes].start = thisToken - target;
			result[nSubtypes].length = len;
			nSubtypes++;
		}
	}
	*subs = result;
	*nsubtypes = nSubtypes;
	return langIndex;
}
		

static int
check_lang_match( const char *target, const char *baseTarget,
				  _SubStringIndex *targetTypes,
				  int ntargetTypes, char *targetLang, char *attr )
{
	int langIndex;
 	_SubStringIndex *subtypes;
	int baseLen;
	char *lang;
	int nsubtypes;
	int mismatch = 0;
	int match = -1;
	int i;

	/* Get all subtypes in the attribute name */
 	langIndex = parse_subtypes( attr, &baseLen, &lang, &subtypes, &nsubtypes );

	/* Check if there any required non-language subtypes which are
	   not in this attribute */
	for( i = 0; i < ntargetTypes; i++ ) {
		char *t = (char *)target+targetTypes[i].start;
		int tlen = targetTypes[i].length;
		int j;
		for( j = 0; j < nsubtypes; j++ ) {
			char *a = attr + subtypes[j].start;
			int alen = subtypes[j].length;
			if ( (tlen == alen) && !strncasecmp( t, a, tlen ) )
				break;
		}
		if ( j >= nsubtypes ) {
			mismatch = 1;
			break;
		}
	}
	if ( mismatch ) {
	    if ( NULL != subtypes )
		    NSLDAPI_FREE( subtypes );
		if ( NULL != lang )
		    NSLDAPI_FREE( lang );
		return -1;
	}

	/* If there was no language subtype... */
	if ( langIndex < 0 ) {
	    if ( NULL != subtypes )
		    NSLDAPI_FREE( subtypes );
		if ( NULL != lang )
		    NSLDAPI_FREE( lang );
		if ( LANG_SUBTYPE_INDEX_NONE == langIndex )
			return 0;
		else
			return -1;
	}

	/* Okay, now check the language subtag */
	i = 0;
	while( targetLang[i] && lang[i] &&
			(toupper(targetLang[i]) == toupper(lang[i])) )
		i++;

	/* The total length can't be longer than the requested subtype */
	if ( !lang[i] || (lang[i] == ';') ) {
		/* If the found subtype is shorter than the requested one, the next
		   character in the requested one should be "-" */
		if ( !targetLang[i] || (targetLang[i] == '-') )
			match = i;
	}
	return match;
}

static int check_base_match( const char *target, char *attr )
{
    int i = 0;
	int rc;
	while( target[i] && attr[i] && (toupper(target[i]) == toupper(attr[i])) )
	    i++;
	rc = ( !target[i] && (!attr[i] || (';' == attr[i])) );
	return rc;
}

static void **
internal_ldap_get_lang_values( LDAP *ld, LDAPMessage *entry,
							const char *target, char **type, int lencall )
{
	struct berelement	ber;
	char		        *attr = NULL;
	int			        rc;
	void			    **vals = NULL;
	int                 langIndex;
 	_SubStringIndex     *subtypes;
	int                 nsubtypes;
	char                *baseTarget = NULL;
	int                 bestMatch = 0;
	char                *lang = NULL;
	int                 len;
	int					firstAttr = 1;
	char				*bestType = NULL;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_get_values\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( NULL );
	}
	if ( (target == NULL) ||
	    !NSLDAPI_VALID_LDAPMESSAGE_ENTRY_POINTER( entry )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( NULL );
	}

	/* A language check was requested, so see if there really is a
	   language subtype in the attribute spec */
	langIndex = parse_subtypes( target, &len, &lang,
							   &subtypes, &nsubtypes );
	if ( langIndex < 0 ) {
		if ( NULL != subtypes ) {
			NSLDAPI_FREE( subtypes );
			subtypes = NULL;
		}
		vals = internal_ldap_get_values( ld, entry, target, lencall );
		if ( NULL != type )
			*type = nsldapi_strdup( target );
		return vals;
	} else {
		/* Get just the base attribute name */
		baseTarget = (char *)NSLDAPI_MALLOC( len + 1 );
		memcpy( baseTarget, target, len );
		baseTarget[len] = 0;
	}

	ber = *entry->lm_ber;

	/* Process all attributes in the entry */
	while ( 1 ) {
		int foundMatch = 0;
		if ( NULL != attr )
			NSLDAPI_FREE( attr );
		if ( firstAttr ) {
			firstAttr = 0;
			/* skip sequence, dn, sequence of, and snag the first attr */
			if ( ber_scanf( &ber, "{x{{a", &attr ) == LBER_ERROR ) {
				break;
			}
		} else {
			if ( ber_scanf( &ber, "{a", &attr ) == LBER_ERROR ) {
				break;
			}
		}

		if ( check_base_match( (const char *)baseTarget, attr ) ) {
			int thisMatch = check_lang_match( target, baseTarget,
											  subtypes, nsubtypes, lang, attr );
			if ( thisMatch > bestMatch ) {
				if ( vals )
					NSLDAPI_FREE( vals );
				foundMatch = 1;
				bestMatch = thisMatch;
				if ( NULL != bestType )
					NSLDAPI_FREE( bestType );
				bestType = attr;
				attr = NULL;
			}
		}
		if ( foundMatch ) {
			if ( lencall ) {
				rc = ber_scanf( &ber, "[V]}", &vals );
			} else {
				rc = ber_scanf( &ber, "[v]}", &vals );
			}
		} else {
			ber_scanf( &ber, "x}" );
		}
	}

	NSLDAPI_FREE( lang );
	NSLDAPI_FREE( baseTarget );
	NSLDAPI_FREE( subtypes );

	if ( NULL != type )
		*type = bestType;
	else if ( NULL != bestType )
		NSLDAPI_FREE( bestType );

	if ( NULL == vals ) {
		rc = LDAP_DECODING_ERROR;
	} else {
		rc = LDAP_SUCCESS;
	}

	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );

	return( vals );
}


char **
LDAP_CALL
ldap_get_values( LDAP *ld, LDAPMessage *entry, const char *target )
{
	return( (char **) internal_ldap_get_values( ld, entry, target, 0 ) );
}

struct berval **
LDAP_CALL
ldap_get_values_len( LDAP *ld, LDAPMessage *entry, const char *target )
{
	return( (struct berval **) internal_ldap_get_values( ld, entry, target,
	    1 ) );
}

char **
LDAP_CALL
ldap_get_lang_values( LDAP *ld, LDAPMessage *entry, const char *target,
						char **type )
{
	return( (char **) internal_ldap_get_lang_values( ld, entry,
											target, type, 0 ) );
}

struct berval **
LDAP_CALL
ldap_get_lang_values_len( LDAP *ld, LDAPMessage *entry, const char *target,
						char **type )
{
	return( (struct berval **) internal_ldap_get_lang_values( ld, entry,
										   target, type, 1 ) );
}

