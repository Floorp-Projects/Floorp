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
 *  LDAP tools fileurl.h -- defines for file URL functions.
 *  Used by ldapmodify.
 */

/*
 * ldaptool_fileurl2path() convert a file URL to a local path.
 *
 * If successful, LDAPTOOL_FILEURL_SUCCESS is returned and *localpathp is
 * set point to an allocated string.  If not, a different LDAPTOOL_FILEURL_
 * error code is returned.
 */
int ldaptool_fileurl2path( const char *fileurl, char **localpathp );


/*
 * Convert a local path to a file URL.
 *
 * If successful, LDAPTOOL_FILEURL_SUCCESS is returned and *urlp is
 * set point to an allocated string.  If not, a different LDAPTOOL_FILEURL_
 * error code is returned.  At present, the only possible error is
 * LDAPTOOL_FILEURL_NOMEMORY.
 *
 */
int ldaptool_path2fileurl( char *path, char **urlp );


/*
 * Populate *bvp from "value" of length "vlen."
 *
 * If recognize_url_syntax is non-zero, :<fileurl is recognized.
 * If always_try_file is recognized and no file URL was found, an
 * attempt is made to stat and read the value as if it were the name
 * of a file.
 *
 * If reporterrs is non-zero, specific error messages are printed to
 * stderr.
 *
 * If successful, LDAPTOOL_FILEURL_SUCCESS is returned and bvp->bv_len
 * and bvp->bv_val are set (the latter is set to malloc'd memory).
 * Upon failure, a different LDAPTOOL_FILEURL_ error code is returned.
 */
int ldaptool_berval_from_ldif_value( const char *value, int vlen,
	struct berval *bvp, int recognize_url_syntax, int always_try_file,
	int reporterrs );


/*
 * Map an LDAPTOOL_FILEURL_ error code to an LDAP error code (crude).
 */
int ldaptool_fileurlerr2ldaperr( int lderr );


/*
 * Possible return codes for the functions declared in this file:
 */
#define LDAPTOOL_FILEURL_SUCCESS	0
#define LDAPTOOL_FILEURL_NOTAFILEURL	1
#define LDAPTOOL_FILEURL_MISSINGPATH	2
#define LDAPTOOL_FILEURL_NONLOCAL	3
#define LDAPTOOL_FILEURL_NOMEMORY	4
#define LDAPTOOL_FILEURL_FILEIOERROR	5
