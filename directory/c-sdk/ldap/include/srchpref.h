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
 * Copyright (c) 1993, 1994 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 * searchpref.h:  display template library defines
 */


#ifndef _SRCHPREF_H
#define _SRCHPREF_H

#ifdef __cplusplus
extern "C" {
#endif

/* calling conventions used by library */
#ifndef LDAP_CALL
#if defined( _WINDOWS ) || defined( _WIN32 )
#define LDAP_C __cdecl
#ifndef _WIN32 
#define __stdcall _far _pascal
#define LDAP_CALLBACK _loadds
#else
#define LDAP_CALLBACK
#endif /* _WIN32 */
#define LDAP_PASCAL __stdcall
#define LDAP_CALL LDAP_PASCAL
#else /* _WINDOWS */
#define LDAP_C
#define LDAP_CALLBACK
#define LDAP_PASCAL
#define LDAP_CALL
#endif /* _WINDOWS */
#endif /* LDAP_CALL */

struct ldap_searchattr {
	char				*sa_attrlabel;
	char				*sa_attr;
					/* max 32 matchtypes for now */
	unsigned long			sa_matchtypebitmap;
	char				*sa_selectattr;
	char				*sa_selecttext;
	struct ldap_searchattr		*sa_next;
};

struct ldap_searchmatch {
	char				*sm_matchprompt;
	char				*sm_filter;
	struct ldap_searchmatch		*sm_next;
};

struct ldap_searchobj {
	char				*so_objtypeprompt;
	unsigned long			so_options;
	char				*so_prompt;
	short				so_defaultscope;
	char				*so_filterprefix;
	char				*so_filtertag;
	char				*so_defaultselectattr;
	char				*so_defaultselecttext;
	struct ldap_searchattr		*so_salist;
	struct ldap_searchmatch		*so_smlist;
	struct ldap_searchobj		*so_next;
};

#define NULLSEARCHOBJ			((struct ldap_searchobj *)0)

/*
 * global search object options
 */
#define LDAP_SEARCHOBJ_OPT_INTERNAL	0x00000001

#define LDAP_IS_SEARCHOBJ_OPTION_SET( so, option )	\
	(((so)->so_options & option ) != 0 )

#define LDAP_SEARCHPREF_VERSION_ZERO	0
#define LDAP_SEARCHPREF_VERSION		1

#define LDAP_SEARCHPREF_ERR_VERSION	1
#define LDAP_SEARCHPREF_ERR_MEM		2
#define LDAP_SEARCHPREF_ERR_SYNTAX	3
#define LDAP_SEARCHPREF_ERR_FILE	4


LDAP_API(int)
LDAP_CALL
ldap_init_searchprefs( char *file, struct ldap_searchobj **solistp );

LDAP_API(int)
LDAP_CALL
ldap_init_searchprefs_buf( char *buf, long buflen,
	struct ldap_searchobj **solistp );

LDAP_API(void)
LDAP_CALL
ldap_free_searchprefs( struct ldap_searchobj *solist );

LDAP_API(struct ldap_searchobj *)
LDAP_CALL
ldap_first_searchobj( struct ldap_searchobj *solist );

LDAP_API(struct ldap_searchobj *)
LDAP_CALL
ldap_next_searchobj( struct ldap_searchobj *sollist,
	struct ldap_searchobj *so );

#ifdef __cplusplus
}
#endif
#endif /* _SRCHPREF_H */
