/*  -*- Mode: C; eval: (c-set-style "GNU") -*-
 ******************************************************************************
 * $Id: lulu.h,v 1.4 2004/04/25 21:07:10 gerv%gerv.net Exp $
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Netscape DS Plugins. 
 *
 * The Initial Developer of the Original Code is Leif Hedstrom and Netscape  
 * Communications Corp. 
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 
/*****************************************************************************
 * DESCRIPTION:
 *    Lots of Useful Little Utilities, defines and stuff.
 *
 *****************************************************************************/
 
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>

#include <slapi-plugin.h>


/******************************************************************************
 *  Configurations, you might want to look at these...
 */
#define LOG_FACILITY	SLAPI_LOG_PLUGIN


/******************************************************************************
 *  Error strings.
 */
#define ERR_ARGS	"Can't locate plugin arguments, Panic!\n"
#define ERR_NOFILTERS	"There are not filters defined, disabling plugin.\n"
#define ERR_REG		"Error registering plugin function.\n"
#define ERR_MALLOC	"Can't allocate memory, which is a Bad Thing(tm).\n"
#define ERR_SYNTAX	"Can't make filter out of plugin arguments.\n"
#define ERR_NOENTRY	"Could not get entry.\n"
#define ERR_NOMODS	"Could not get the modifications.\n"
#define ERR_NOSET	"Failed to set DN and auth method.\n"


/******************************************************************************
 *  Misc. defines etc., don't touch anything below this.
 */
#define PRIVATE static
#define PUBLIC
#define STATIC static

#define NULLCP (char *)NULL
#define ATTR_SEPARATOR	","


/******************************************************************************
 *  Try to get "inlined" defined, if possible. Currently only supports GCC.
 */
#ifdef __GNUC__
#  define INLINE inline
#else   /* not __GNUC__ */
#  define INLINE
#endif  /* __GNUC__     */


/******************************************************************************
 *  Typedefs and structures.
 */
typedef struct _Plugin_Attrs
{
  char *type;		/* Name/type of the attribute, all lower case.	*/
  char first;		/* First character in the attribute name.	*/
  int len;		/* Length of the name/string.			*/
  struct _Plugin_Attrs *next;
} Plugin_Attrs;

typedef struct berval BerVal;


/******************************************************************************
 *  Public functions.
 */
PUBLIC Plugin_Attrs *parse_attributes(char *);
PUBLIC int list_has_attribute(Plugin_Attrs *, char *);
PUBLIC int free_attributes(Plugin_Attrs *);
PUBLIC int send_constraint_err(Slapi_PBlock *, char *);
PUBLIC int send_operations_err(Slapi_PBlock *, char *);
PUBLIC int get_add_entry(Slapi_PBlock *, Slapi_Entry **, char *);
PUBLIC int get_modify_mods(Slapi_PBlock *, LDAPMod ***, char *);
PUBLIC int get_slapi_args(Slapi_PBlock *, int *, char ***, int, char *);
PUBLIC int set_simple_auth_info(Slapi_PBlock *, char *, char *, char *);
