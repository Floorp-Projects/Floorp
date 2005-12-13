/*  -*- Mode: C; eval: (c-set-style "GNU") -*-
 ******************************************************************************
 * $Id: rex_filt.c,v 1.5 2005/12/13 13:25:32 gerv%gerv.net Exp $
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
 *   Apply regular expressions to MOD/ADD operations, if the rule is a match
 *   deny/allow the modification. You can use this plugin any number of times
 *   from the slapd.conf file, it will extend the filter rules accordingly.
 *
 * USAGE:
 *    plugin preoperation rex_filt.so rexPlugInit attr1,2 0/1 regexp1
 *    plugin preoperation rex_filt.so rexPlugInit attr3,4 0/1 regexp2
 *
 * TODO:
 *    * Support the /.../i  syntax, for case insensitive regexps.
 *
 *****************************************************************************/

#include <stdio.h>
#include <regex.h>
#include "lulu.h"

/******************************************************************************
 *  Defines, for this particular plugin only.
 */
#define PLUGIN_NAME	"rex_filter"
#define PLUGIN_VERS	"1.1"

#define ERR_NOMATCH	"An attribute does not match a server regex rule.\n"
#define ERR_MATCH	"An attribute matches a server regex rule.\n"


/******************************************************************************
 *  Typedefs and structures. Note that some of the members of this structure
 *  are for performance reason, e.g. the "len" fields.
 */
typedef struct _Rex_Filter
{
  char *string;
  char *attributes;
  regex_t *regex;
  int match;
  Plugin_Attrs *attrs;
  struct _Rex_Filter *next;
} Rex_Filter;


/******************************************************************************
 *  Globals, "private" to this module.
 */
static int rex_num_filters = 0;
static Rex_Filter *rex_filter_list = NULL;
static Slapi_PluginDesc rex_descript = { PLUGIN_NAME,
					 "Leif Hedstrom",
					 PLUGIN_VERS,
					 "Regex filter plugin" };



static int
create_filter(Rex_Filter *filter, char *attributes, char *string)
{
  if (!filter || filter->attrs || !attributes || !string)
    return 0;

  if (!(filter->string = slapi_ch_strdup(string)))
    return 0;

  if (!(filter->attributes = slapi_ch_strdup(attributes)))
    {
      slapi_ch_free((void **)&(filter->attributes));
      return 0;
    }

  if (!(filter->attrs = parse_attributes(filter->attributes)))
    {
      slapi_ch_free((void **)&(filter->string));
      slapi_ch_free((void **)&(filter->attributes));
      return 0;
    }

  if (regcomp(filter->regex, filter->string, REG_EXTENDED|REG_NOSUB))
    {
      slapi_ch_free((void **)&(filter->attributes));
      slapi_ch_free((void **)&(filter->string));
      free_attributes(filter->attrs);
      return 0;
    }

  return 1;
}

static INLINE int
loop_ber_values(Slapi_PBlock *pb, Rex_Filter *filter, BerVal **bvals,
		char *type)
{
  int res;

  while (*bvals)
    {
      res = regexec(filter->regex, (*bvals)->bv_val, (size_t) 0, NULL, 0);
      if ((!res && !filter->match) || (res && filter->match))
	{
	  slapi_log_error(LOG_FACILITY, PLUGIN_NAME,
			  "Violation: %s /%s/ on '%s: %s'\n",
			  filter->match ? "require" : "refuse",
			  filter->string, type, (*bvals)->bv_val);
	  slapi_send_ldap_result(pb, LDAP_CONSTRAINT_VIOLATION, NULL,
				 filter->match ? ERR_NOMATCH : ERR_MATCH, 0,
				 (BerVal **)NULL);
	  return 1;
	}
      bvals++;
    }

  return 0;
}

int
eval_add_filter(Slapi_PBlock *pb)
{
  Rex_Filter *filter;
  Plugin_Attrs *attrs;
  Slapi_Entry *entry;
  Slapi_Attr *att;
  BerVal **bvals;

  if (!entry)
    return 0;

  if (slapi_pblock_get(pb, SLAPI_ADD_ENTRY, &entry) || !entry)
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_NOENTRY);
      slapi_send_ldap_result(pb, LDAP_NO_MEMORY, NULL, ERR_NOENTRY, 0,
			     (BerVal **)NULL);
      return (-1);
    }

  filter = rex_filter_list;
  while (filter)
    {
      attrs = filter->attrs;
      while (attrs)
	{
	  if (!slapi_entry_attr_find(entry, attrs->type, &att))
	    {
	      slapi_attr_get_values(att, &bvals);
	      if (loop_ber_values(pb, filter, bvals, attrs->type))
		return (-1);
	    }
	  attrs = attrs->next;
	}
      filter = filter->next;
    }

  return 0;
}

int
eval_mod_filter(Slapi_PBlock *pb)
{
  Rex_Filter *filter;
  LDAPMod **mods, *mod;

  if (!mods)
    return 0;

  if (slapi_pblock_get(pb, SLAPI_MODIFY_MODS, &mods) || !mods)
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_NOMODS);
      slapi_send_ldap_result(pb, LDAP_NO_MEMORY, NULL, ERR_NOMODS, 0,
			     (BerVal **)NULL);
      return (-1);
    }

  while ((mod = *mods))
    {
      if (!(mod->mod_op & LDAP_MOD_DELETE))
	{
	  filter = rex_filter_list;
	  while (filter)
	    {
	      if (list_has_attribute(filter->attrs, mod->mod_type) &&
		  loop_ber_values(pb, filter, mod->mod_bvalues,
				  mod->mod_type))
		return (-1);
	      filter = filter->next;
	    }
	}
      mods++;
    }

  return 0;
}

int
rex_filter_init(Slapi_PBlock *pb)
{
  char **argv;
  int argc;
  Rex_Filter *new, *last;

  if (slapi_pblock_get(pb, SLAPI_PLUGIN_ARGC, &argc) ||
      slapi_pblock_get(pb, SLAPI_PLUGIN_ARGV, &argv) || (argc < 3) || !argv)
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME,
		      "Can't locate plugin arguments, please panic!\n");
      return (-1);
    }

  if (!(new = (Rex_Filter *)slapi_ch_malloc(sizeof(Rex_Filter))))
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_MALLOC);
      return (-1);
    }

  if (!(new->regex = (regex_t *)slapi_ch_malloc(sizeof(regex_t))))
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_MALLOC);
      return (-1);
    }

  new->next = (Rex_Filter *)NULL;
  new->attrs = (Plugin_Attrs *)NULL;
  new->match = (*(argv[1]) == '0' ? 0 : 1);
  if (!create_filter(new, argv[0], argv[2]) || !new->attrs)
    {
      slapi_ch_free((void **)&(new->regex));
      slapi_ch_free((void **)&new);
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME,
		      "Can't make filter out of plugin arguments.\n");
      return (-1);
    }

  if ((last = rex_filter_list))
    {
      while (last && last->next)
	last = last->next;
      last->next = new;
    }
  else
    rex_filter_list = new;

  if (rex_num_filters++)
    return 0;

  if (slapi_pblock_set(pb, SLAPI_PLUGIN_VERSION, SLAPI_PLUGIN_VERSION_01) ||
      slapi_pblock_set(pb, SLAPI_PLUGIN_DESCRIPTION, (void *)&rex_descript) ||
      slapi_pblock_set(pb, SLAPI_PLUGIN_PRE_MODIFY_FN,
		       (void *)&eval_mod_filter)
      || slapi_pblock_set(pb, SLAPI_PLUGIN_PRE_ADD_FN,
			  (void *)&eval_add_filter))
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME,
		      "Error registering plugin functions.\n");
      return (-1);
    }

  slapi_log_error(LOG_FACILITY, PLUGIN_NAME,
		  "plugin loaded\n");

  return 0;
}
