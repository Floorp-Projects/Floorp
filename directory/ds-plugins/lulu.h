/*  -*- Mode: C; eval: (c-set-style "GNU") -*-
 ******************************************************************************
 * $Id: lulu.h,v 1.2 2000/01/12 06:27:00 leif%netscape.com Exp $
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
 * The Original Code is Netscape DS Plugins. The Initial Developer of the
 * Original Code is Leif Hedstrom and Netscape Communications Corp.
 * Portions created by Netscape are Copyright (C) Netscape Communications
 * Corp. All Rights Reserved.
 *
 * Contributor(s):
 *
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
#define THE_AUTHOR	"IS-Architorture"


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
typedef int BOOL;
#define TRUE (1)
#define FALSE (0)

typedef struct _PluginAttrs
{
  char *type;		/* Name/type of the attribute, all lower case.	*/
  char first;		/* First character in the attribute name.	*/
  int len;		/* Length of the name/string.			*/
  struct _PluginAttrs *next;
} PluginAttrs;

typedef struct berval BerVal;


/******************************************************************************
 *  Public functions.
 */
PUBLIC PluginAttrs *parseAttributes(char *);
PUBLIC BOOL listHasAttribute(PluginAttrs *, char *);
PUBLIC BOOL freeAttributes(PluginAttrs *);
PUBLIC BOOL sendConstraintErr(Slapi_PBlock *, char *);
PUBLIC BOOL sendOperationsErr(Slapi_PBlock *, char *);
PUBLIC BOOL getAddEntry(Slapi_PBlock *, Slapi_Entry **, char *);
PUBLIC BOOL getModifyMods(Slapi_PBlock *, LDAPMod ***, char *);
PUBLIC BOOL getSlapiArgs(Slapi_PBlock *, int *, char ***, int, char *);
PUBLIC int setSimpleAuthInfo(Slapi_PBlock *, char *, char *, char *);
