/*  -*- Mode: C; eval: (c-set-style "GNU") -*-
 ******************************************************************************
 * $Id: url_filt.c,v 1.4 2005/12/13 13:25:32 gerv%gerv.net Exp $
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
 *   Do an LDAP search before doing a MOD/ADD operations, if there is a match,
 *   deny/allow the modification. You can use this plugin any number of times
 *   from the slapd.conf file, it will extend the filter rules accordingly.
 *
 * USAGE:
 *    plugin preoperation url_filt.so urlPlugInit attr1,2 0/1 filter1
 *    plugin preoperation url_filt.so urlPlugInit attr3,4 0/1 filter2
 *
 *    The filter specification is an LDAP URL, where a substring of $$ is
 *    replaced with the LDAP operations attribute value. Examples:
 * 
 *      mail 0 ldap:///dc=foo,dc=com??sub?(|(mail=$$)(mailAlternateAddess=$$))
 *      uid 0  ldap:///dc=foo,dc=com??sub?(uid=$$)
 *      uniqueMember,owner 1 ldap:///$$??base?(objectclass=*)
 *
 *****************************************************************************/

#include <stdio.h>
#include "lulu.h"


/******************************************************************************
 *  Defines, for this particular plugin only.
 */
#define PLUGIN_NAME	"url_filt"
#define PLUGIN_VERS	"1.1"

#define ERR_NOMATCH	"An entry does not match a server LDAP URL rule.\n"
#define ERR_MATCH	"An entry already matches a server LDAP URL rule.\n"


/******************************************************************************
 *  Typedefs and structures. Note that some of the members of this structure
 *  are for performance reason, e.g. the "len" fields.
 */
typedef struct _URL_Filter
{
  char *string;		/* Original LDAP URL string, not used.		*/
  int match;		/* Should we pass if URL matches (yes/no)?	*/
  LDAPURLDesc *url;	/* The components of the URL, parsed.		*/
  char *dn;		/* The DN component.				*/
  int dn_len;		/* Length of the DN component.			*/
  char *search;		/* The LDAP filter/search component.		*/
  int search_len;	/* Length of the filter component.		*/
  int num_dn_subs;	/* The number of $$'s in the Base-DN		*/
  int num_search_subs;	/* The number of $$'s in the search filter.	*/
  Plugin_Attrs *attrs;	/* Attributes to apply this filter to.		*/
  struct _URL_Filter *next;
} URL_Filter;


/******************************************************************************
 *  Globals, "private" to this module.
 */
PRIVATE int url_num_filters = 0;		/* Number of filters.	*/
PRIVATE URL_Filter *url_filter_list = NULL;	/* List of filters.	*/
PRIVATE Slapi_PluginDesc urlDesc = { PLUGIN_NAME,
				     "Leif Hedstrom",
				     PLUGIN_VERS,
				     "LDAP URL filter plugin" };
PRIVATE char *urlAttrs[2] = { "dn", NULLCP };


/******************************************************************************
 *  Count the number of $$'s in a string, if any.
 */
PRIVATE int
count_subs(char *str)
{
  int count = 0;

  while(*str)
    if ((*(str++) == '$') && (*(str++) == '$'))
      count ++;

  return count;
}


/******************************************************************************
 *  Allocate a string for a new string, and do the necessary string subs, with
 *  the supplied berval value. This looks kludgy, any better/smarter ways
 *  of doing this (remember, the $$ can occur more than once)...
 */
PRIVATE INLINE char *
do_subs(char *str, int len, char *val, int valLen)
{
  char *sub, *tmp;

  if (!(sub = (char *)slapi_ch_malloc(len)))
    return NULLCP;

  tmp = sub;
  while (*str)
    {
      if (*str == '$')
	{
	  if (*(++str) == '$')
	    {
	      memcpy(tmp, val, valLen);
	      tmp += valLen;
	      str++;
	    }
	  else
	    *(tmp++) = '$';
	}
      else
	*(tmp++) = *(str++);
    }
  *tmp = '\0';

  return sub;
}


/******************************************************************************
 *  Setup a filter structure, used when parsing the plugin arguments from the
 *  registration routien.
 */
PRIVATE int
create_filter(URL_Filter *filter, char *attributes, char *string)
{
  int res;

  if (!filter || filter->attrs || !attributes || !string)
    return 0;

  if (!(filter->string = slapi_ch_strdup(string)))
    return 0;

  if (!(filter->attrs = parse_attributes(attributes)))
    {
      slapi_ch_free((void **)&(filter->string));

      return 0;
    }

  /* Parse the URL, and make sure it's of proper syntax.		*/
  if ((res = ldap_url_parse(filter->string, &(filter->url))))
    {
      /* Produce a proper Error Message... */
      free_attributes(filter->attrs);

      return 0;
    }

  /* Let's fill in some more stuff about the filter.			*/
  filter->dn = filter->url->lud_dn;
  filter->search = filter->url->lud_filter;

  filter->dn_len = strlen(filter->dn);
  filter->search_len = strlen(filter->search);
  filter->num_dn_subs = count_subs(filter->dn);
  filter->num_search_subs = count_subs(filter->search);

  return 1;
}


/******************************************************************************
 *  Perform the URL LDAP test, and set the return code properly if we do have
 *  have a violation. Return 1 if we did run trigger a rule.
 */
PRIVATE INLINE int
test_filter(Slapi_PBlock *pb, URL_Filter *filter, char *type, char *value,
	      int len)
{
  Slapi_PBlock *resPB;
  char *tmpDN, *tmpSearch;
  Slapi_Entry **entries;
  int res, retval;

  /* Let's first do the appropriate substitutions, on DN and Filters.	*/
  if (filter->num_dn_subs)
    {
      if (!(tmpDN = do_subs(filter->dn, filter->dn_len +
			      filter->num_dn_subs * len, value, len)))
	return 0;
    }
  else
    tmpDN = filter->dn;
  if (filter->num_search_subs)
    {
      if (!(tmpSearch = do_subs(filter->search, filter->search_len +
				  filter->num_search_subs * len, value, len)))
	{
	  slapi_ch_free((void **)&tmpDN);

	  return 0;
	}
    }
  else
    tmpSearch = filter->search;

  /* Now we'll do the actual lookup in the internal DB. We only care if
     the search finds something or not, the actual data is irrelevant.	*/
  resPB = slapi_search_internal(tmpDN, filter->url->lud_scope, tmpSearch,
				(LDAPControl **)NULL, urlAttrs, 1);
  if (resPB)
    {
      if (slapi_pblock_get(resPB, SLAPI_PLUGIN_INTOP_RESULT, &res))
	res = LDAP_OPERATIONS_ERROR;
      else if (res == LDAP_SUCCESS)
	{
	  if (slapi_pblock_get(resPB, SLAPI_PLUGIN_INTOP_SEARCH_ENTRIES,
			       &entries))
	    res = LDAP_OPERATIONS_ERROR;
	  else if (entries && *entries)
	    res = LDAP_SUCCESS;
	  else
	    res = LDAP_NO_SUCH_OBJECT;
	}

      slapi_free_search_results_internal(resPB);
      slapi_ch_free((void **)&resPB);
    }
  else
    res = LDAP_OPERATIONS_ERROR;

  retval = 0;
  if ((!res && !filter->match) || (res && filter->match))
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME,
		      "Violation: %s on '%s: %s'\n",
		      filter->string, type, value);
      send_constraint_err(pb, filter->match ? ERR_NOMATCH : ERR_MATCH);

      retval = 1;
    }

  if (filter->num_dn_subs)
    slapi_ch_free((void **)&tmpDN);
  if (filter->num_search_subs)
    slapi_ch_free((void **)&tmpSearch);

  return retval;
}

PRIVATE INLINE int
loop_ber_values(Slapi_PBlock *pb, URL_Filter *filter, BerVal **bvals, char *type)
{
  while (*bvals)
    {
      if (test_filter(pb, filter, type, (*bvals)->bv_val, (*bvals)->bv_len))
	return 1;

      bvals++;
    }

  return 0;
}


/******************************************************************************
 *  Handle ADD operations.
 */
PUBLIC int
eval_add_filter(Slapi_PBlock *pb)
{
  URL_Filter *filter;
  Plugin_Attrs *attrs;
  Slapi_Entry *entry;
  Slapi_Attr *att;
  BerVal **bvals;

  if (!get_add_entry(pb, &entry, PLUGIN_NAME))
    return (-1);

  filter = url_filter_list;
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


/******************************************************************************
 *  Handle MOD operations.
 */
PUBLIC int
eval_mod_filter(Slapi_PBlock *pb)
{
  URL_Filter *filter;
  LDAPMod **mods, *mod;

  if (!get_modify_mods(pb, &mods, PLUGIN_NAME))
    return (-1);

  while ((mod = *mods))
    {
      if (!(mod->mod_op & LDAP_MOD_DELETE))
	{
	  filter = url_filter_list;
	  while (filter)
	    {
	      if (list_has_attribute(filter->attrs, mod->mod_type) &&
		  loop_ber_values(pb, filter, mod->mod_bvalues, mod->mod_type))
		return (-1);

	      filter = filter->next;
	    }
	}
      mods++;
    }

  return 0;
}


/******************************************************************************
 *  Register the pre-op functions.
 */
PUBLIC int
url_filter_init(Slapi_PBlock *pb)
{
  char **argv;
  int argc;
  URL_Filter *new, *last;

  if (!get_slapi_args(pb, &argc, &argv, 3, PLUGIN_NAME))
    return (-1);

  /* Allocate memory for the new Filter rule.				*/
  if (!(new = (URL_Filter *)slapi_ch_malloc(sizeof(URL_Filter))))
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_MALLOC);

      return (-1);
    }

  /* Create the new filter, using the filter options in argv.		*/
  new->next = (URL_Filter *)NULL;
  new->attrs = (Plugin_Attrs *)NULL;
  new->match = (*(argv[1]) == '0' ? 0 : 1);
  if (!create_filter(new, argv[0], argv[2]) || !new || !new->attrs)
    {
      slapi_ch_free((void **)&new);
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_SYNTAX);

      return (-1);
    }

  /* Find the last filter in the chain of filters, and link in the new. */
  if ((last = url_filter_list))
    {
      while (last && last->next)
	last = last->next;

      last->next = new;
    }
  else
    url_filter_list = new;
  url_num_filters++;

  /* Ok, everything looks grovy, so let's register the filter, but
     only if we haven't done so already...				*/
  if (url_num_filters > 1)
    return 0;

  if (slapi_pblock_set(pb, SLAPI_PLUGIN_VERSION, SLAPI_PLUGIN_VERSION_01) ||
      slapi_pblock_set(pb, SLAPI_PLUGIN_DESCRIPTION, &urlDesc) ||
      slapi_pblock_set(pb, SLAPI_PLUGIN_PRE_MODIFY_FN, (void *)&eval_mod_filter) ||
      slapi_pblock_set(pb, SLAPI_PLUGIN_PRE_ADD_FN, (void *)&eval_add_filter))
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_REG);

      return (-1);
    }

  return 0;
}
