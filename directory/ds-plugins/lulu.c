/*  -*- Mode: C; eval: (c-set-style "GNU") -*-
 ******************************************************************************
 * $Id: lulu.c,v 1.2 2000/01/12 06:26:59 leif%netscape.com Exp $
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
 *   Lots of Useful Little Utilities
 *
 *****************************************************************************/
#include <stdio.h>
#include "lulu.h"


/******************************************************************************
 *  Free the list of attributes, in case something went south...
 */
PUBLIC BOOL
freeAttributes(PluginAttrs *attrs)
{
  PluginAttrs *cur;

  if (!attrs)
    return FALSE;

  if (attrs->type)
    slapi_ch_free((void **)&(attrs->type));

  while (attrs)
    {
      cur = attrs;
      attrs = cur->next;
      slapi_ch_free((void **)&cur);
    }

  return TRUE;
}


/******************************************************************************
 *  Create a list of attributes, from the string argument. This is a comma
 *  separated list of attributes, without white spaces! For instance:
 *
 *    parseAttributes("cn,uid,mail");
 */
PUBLIC PluginAttrs *
parseAttributes(char *str)
{
  char *list, *tmp;
  PluginAttrs *cur, *ret;

  /* Allocate space for the attributes and the regex string		*/
  if (!str || !(list = slapi_ch_strdup(str)))
    return (PluginAttrs *)NULL;

  tmp = list;
  while (*tmp)
    {
      *tmp = tolower((int)*tmp);
      tmp++;
    }

  /* Parse through attributes, one by one. Note that this doesn't handle
     weird input like "," or "phone,".					*/
  if (!(ret = (PluginAttrs *)slapi_ch_malloc(sizeof(PluginAttrs))))
    return (PluginAttrs *)NULL;
  cur = ret;

  tmp = strtok(list, ATTR_SEPARATOR);
  while (tmp)
    {
      if (!cur)
	{
	  /* This shouldn't happen, but if it does let's be clean.	*/
	  freeAttributes(ret);

	  return (PluginAttrs *)NULL;
	}

      cur->type = tmp;
      cur->first = *tmp;
      cur->len = strlen(tmp);

      if ((tmp = strtok(NULLCP, ATTR_SEPARATOR)))
	cur->next = (PluginAttrs *)slapi_ch_malloc(sizeof(PluginAttrs));
      else
	cur->next = (PluginAttrs *)NULL;

      cur = cur->next;
    }

  return ret;
}


/******************************************************************************
 *  Check if a particular attribute type is in a Plugin Attribute list. This
 *  can be used to decide if a plugin/filter should be applied for instance.
 */
PUBLIC INLINE BOOL
listHasAttribute(PluginAttrs *attrs, char *type)
{
  int len;

  if (!attrs || !type)
    return FALSE;

  len = strlen(type);
  while (attrs)
    {
      if ((attrs->first == *type) &&
	  (attrs->len == len) &&
	  (!strcmp(attrs->type, type)))
	return TRUE;

      attrs = attrs->next;
    }
  
  return FALSE;
}


/******************************************************************************
 *  Send a constraint violation error back to the client, with a more
 *  descriptive error message.
 */
PUBLIC BOOL
sendConstraintErr(Slapi_PBlock *pb, char *str)
{
  slapi_send_ldap_result(pb, LDAP_CONSTRAINT_VIOLATION, NULLCP, str, 0,
			 (BerVal **)NULL);

  return TRUE;
}


/******************************************************************************
 *  Send a constraint violation error back to the client, with a more
 *  descriptive error message.
 */
PUBLIC BOOL
sendOperationsErr(Slapi_PBlock *pb, char *str)
{
  slapi_send_ldap_result(pb, LDAP_OPERATIONS_ERROR, NULLCP, str, 0,
			 (BerVal **)NULL);

  return TRUE;
}


/******************************************************************************
 *  Get the ADD entry, and if it fails we send an error, and return FALSE. As
 *  a side effect the "entry" argument (a handle) is set to the structure.
 */
PUBLIC INLINE BOOL
getAddEntry(Slapi_PBlock *pb, Slapi_Entry **entry, char *name)
{
  if (!entry)
    return FALSE;

  if (slapi_pblock_get(pb, SLAPI_ADD_ENTRY, entry) || !*entry)
    {
      slapi_log_error(LOG_FACILITY, name, ERR_NOENTRY);
      slapi_send_ldap_result(pb, LDAP_NO_MEMORY, NULLCP, ERR_NOENTRY, 0,
			     (BerVal **)NULL);
      
      return FALSE;
    }

  return TRUE;
}


/******************************************************************************
 *  Get the MODS for the current operation, and if it fails we send and error,
 *  and return FALSE. As a side effect, we set the "mods" argument (a handle)
 *  to the modifications structure.
 */
PUBLIC INLINE BOOL
getModifyMods(Slapi_PBlock *pb, LDAPMod ***mods, char *name)
{
  if (!mods)
    return FALSE;

  if (slapi_pblock_get(pb, SLAPI_MODIFY_MODS, mods) || !*mods)
    {
      slapi_log_error(LOG_FACILITY, name, ERR_NOMODS);
      slapi_send_ldap_result(pb, LDAP_NO_MEMORY, NULLCP, ERR_NOMODS, 0,
			     (BerVal **)NULL);

      return FALSE;
    }

  return TRUE;
}


/******************************************************************************
 *  Get the "command line arguments", and return TRUE if successful.
 */
PUBLIC BOOL
getSlapiArgs(Slapi_PBlock *pb, int *argc, char ***argv, int exp, char *name)
{
  if (slapi_pblock_get(pb, SLAPI_PLUGIN_ARGC, argc) ||
      slapi_pblock_get(pb, SLAPI_PLUGIN_ARGV, argv) ||
      (*argc < exp) || !*argv)
    {
      slapi_log_error(LOG_FACILITY, name, ERR_ARGS);

      return FALSE;
    }

  return TRUE;
}


/******************************************************************************
 *  Set the DN and authtype return values, making sure the pblock_set() did
 *  actually work.
 */
PUBLIC int
setSimpleAuthInfo(Slapi_PBlock *pb, char *dn, char *auth, char *name)
{
  char *setDN;

  if (!dn)
    return LDAP_OPERATIONS_ERROR;

  setDN = slapi_ch_strdup(dn);
  if (!setDN ||
      slapi_pblock_set(pb, SLAPI_CONN_DN, setDN) ||
      slapi_pblock_set(pb, SLAPI_CONN_AUTHTYPE, auth))
    {
      slapi_log_error(LOG_FACILITY, name, ERR_NOSET);

      return LDAP_OPERATIONS_ERROR;
    }

  return LDAP_SUCCESS;
}
