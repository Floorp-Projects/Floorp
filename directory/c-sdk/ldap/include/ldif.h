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
 * Copyright (c) 1996 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

/* NOTE: As of mozldap version 6.0.1 the LDIF functions are now
   publicly usable.  The LDIF functions were originally designed for
   "internal use only" purposes and as such the APIs are not very modern
   or safe.  For example, the caller needs to be careful to provide
   adequately sized buffers and so on.
*/

#ifndef _LDIF_H
#define _LDIF_H

#ifdef __cplusplus
extern "C" {
#endif

#define LDIF_VERSION_ONE        1	/* LDIF standard version */

#define LDIF_MAX_LINE_WIDTH      76      /* maximum length of LDIF lines */

/*
 * Macro to calculate maximum number of bytes that the base64 equivalent
 * of an item that is "vlen" bytes long will take up.  Base64 encoding
 * uses one byte for every six bits in the value plus up to two pad bytes.
 */
#define LDIF_BASE64_LEN(vlen)	(((vlen) * 4 / 3 ) + 3)

/*
 * Macro to calculate maximum size that an LDIF-encoded type (length
 * tlen) and value (length vlen) will take up:  room for type + ":: " +
 * first newline + base64 value + continued lines.  Each continued line
 * needs room for a newline and a leading space character.
 */
#define LDIF_SIZE_NEEDED(tlen,vlen) \
    ((tlen) + 4 + LDIF_BASE64_LEN(vlen) \
    + ((LDIF_BASE64_LEN(vlen) + tlen + 3) / LDIF_MAX_LINE_WIDTH * 2 ))

/*
 * Options for ldif_put_type_and_value_with_options() and 
 * ldif_type_and_value_with_options().
 */
#define LDIF_OPT_NOWRAP			0x01UL
#define LDIF_OPT_VALUE_IS_URL		0x02UL
#define LDIF_OPT_MINIMAL_ENCODING	0x04UL

int ldif_parse_line( char *line, char **type, char **value, int *vlen);
char * ldif_getline( char **next );
void ldif_put_type_and_value( char **out, char *t, char *val, int vlen );
void ldif_put_type_and_value_nowrap( char **out, char *t, char *val, int vlen );
void ldif_put_type_and_value_with_options( char **out, char *t, char *val,
	int vlen, unsigned long options );
char *ldif_type_and_value( char *type, char *val, int vlen );
char *ldif_type_and_value_nowrap( char *type, char *val, int vlen );
char *ldif_type_and_value_with_options( char *type, char *val, int vlen,
	unsigned long options );
int ldif_base64_decode( char *src, unsigned char *dst );
int ldif_base64_encode( unsigned char *src, char *dst, int srclen,
	int lenused );
int ldif_base64_encode_nowrap( unsigned char *src, char *dst, int srclen,
	int lenused );
char *ldif_get_entry( FILE *fp, int *lineno );

#ifdef __cplusplus
}
#endif

#endif /* _LDIF_H */
