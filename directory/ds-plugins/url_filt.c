/*  -*- Mode: C; eval: (c-set-style "GNU") -*-
 ******************************************************************************
 * $Id: url_filt.c,v 1.1 2000/01/12 06:15:45 leif%netscape.com Exp $
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
#define PLUGIN_VERS	"1.0"

#define ERR_NOMATCH	"An entry does not match a server LDAP URL rule.\n"
#define ERR_MATCH	"An entry already matches a server LDAP URL rule.\n"


/******************************************************************************
 *  Typedefs and structures. Note that some of the members of this structure
 *  are for performance reason, e.g. the "len" fields.
 */
typedef struct _UrlFilter
{
  char *string;		/* Original LDAP URL string, not used.		*/
  BOOL match;		/* Should we pass if URL matches (yes/no)?	*/
  LDAPURLDesc *url;	/* The components of the URL, parsed.		*/
  char *dn;		/* The DN component.				*/
  int dnLen;		/* Length of the DN component.			*/
  char *search;		/* The LDAP filter/search component.		*/
  int searchLen;	/* Length of the filter component.		*/
  int numDNSubs;	/* The number of $$'s in the Base-DN		*/
  int numSearchSubs;	/* The number of $$'s in the search filter.	*/
  PluginAttrs *attrs;	/* Attributes to apply this filter to.		*/
  struct _UrlFilter *next;
} UrlFilter;


/******************************************************************************
 *  Globals, "private" to this module.
 */
PRIVATE int urlNumFilters = 0;		/* Number of filters we have.	*/
PRIVATE UrlFilter *urlFilters = NULL;	/* Pointer to first filter.	*/
PRIVATE Slapi_PluginDesc urlDesc = { PLUGIN_NAME, THE_AUTHOR, PLUGIN_VERS,
				     "LDAP URL filter plugin" };
PRIVATE char *urlAttrs[2] = { "dn", NULLCP };


/******************************************************************************
 *  Count the number of $$'s in a string, if any.
 */
PRIVATE int
urlCountSubs(char *str)
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
urlDoSubs(char *str, int len, char *val, int valLen)
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
PRIVATE BOOL
urlCreateFilter(UrlFilter *filter, char *attributes, char *string)
{
  int res;

  if (!filter || filter->attrs || !attributes || !string)
    return FALSE;

  if (!(filter->string = slapi_ch_strdup(string)))
    return FALSE;

  if (!(filter->attrs = parseAttributes(attributes)))
    {
      slapi_ch_free((void **)&(filter->string));

      return FALSE;
    }

  /* Parse the URL, and make sure it's of proper syntax.		*/
  if ((res = ldap_url_parse(filter->string, &(filter->url))))
    {
      /* Produce a proper Error Message... */
      freeAttributes(filter->attrs);

      return FALSE;
    }

  /* Let's fill in some more stuff about the filter.			*/
  filter->dn = filter->url->lud_dn;
  filter->search = filter->url->lud_filter;

  filter->dnLen = strlen(filter->dn);
  filter->searchLen = strlen(filter->search);
  filter->numDNSubs = urlCountSubs(filter->dn);
  filter->numSearchSubs = urlCountSubs(filter->search);

  return TRUE;
}


/******************************************************************************
 *  Perform the URL LDAP test, and set the return code properly if we do have
 *  have a violation. Return TRUE if we did run trigger a rule.
 */
PRIVATE INLINE BOOL
urlTestFilter(Slapi_PBlock *pb, UrlFilter *filter, char *type, char *value,
	      int len)
{
  Slapi_PBlock *resPB;
  char *tmpDN, *tmpSearch;
  Slapi_Entry **entries;
  int res, retval;

  /* Let's first do the appropriate substitutions, on DN and Filters.	*/
  if (filter->numDNSubs)
    {
      if (!(tmpDN = urlDoSubs(filter->dn, filter->dnLen +
			      filter->numDNSubs * len, value, len)))
	return FALSE;
    }
  else
    tmpDN = filter->dn;
  if (filter->numSearchSubs)
    {
      if (!(tmpSearch = urlDoSubs(filter->search, filter->searchLen +
				  filter->numSearchSubs * len, value, len)))
	{
	  slapi_ch_free((void **)&tmpDN);

	  return FALSE;
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

  retval = FALSE;
  if ((!res && !filter->match) || (res && filter->match))
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME,
		      "Violation: %s on '%s: %s'\n",
		      filter->string, type, value);
      sendConstraintErr(pb, filter->match ? ERR_NOMATCH : ERR_MATCH);

      retval = TRUE;
    }

  if (filter->numDNSubs)
    slapi_ch_free((void **)&tmpDN);
  if (filter->numSearchSubs)
    slapi_ch_free((void **)&tmpSearch);

  return retval;
}

PRIVATE INLINE BOOL
urlLoopBValues(Slapi_PBlock *pb, UrlFilter *filter, BerVal **bvals, char *type)
{
  while (*bvals)
    {
      if (urlTestFilter(pb, filter, type, (*bvals)->bv_val, (*bvals)->bv_len))
	return TRUE;

      bvals++;
    }

  return FALSE;
}


/******************************************************************************
 *  Handle ADD operations.
 */
PUBLIC int
urlAddFilter(Slapi_PBlock *pb)
{
  UrlFilter *filter;
  PluginAttrs *attrs;
  Slapi_Entry *entry;
  Slapi_Attr *att;
  BerVal **bvals;

  if (!getAddEntry(pb, &entry, PLUGIN_NAME))
    return (-1);

  filter = urlFilters;
  while (filter)
    {
      attrs = filter->attrs;
      while (attrs)
	{
	  if (!slapi_entry_attr_find(entry, attrs->type, &att))
	    {
	      slapi_attr_get_values(att, &bvals);
	      if (urlLoopBValues(pb, filter, bvals, attrs->type))
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
urlModFilter(Slapi_PBlock *pb)
{
  UrlFilter *filter;
  LDAPMod **mods, *mod;

  if (!getModifyMods(pb, &mods, PLUGIN_NAME))
    return (-1);

  while ((mod = *mods))
    {
      if (!(mod->mod_op & LDAP_MOD_DELETE))
	{
	  filter = urlFilters;
	  while (filter)
	    {
	      if (listHasAttribute(filter->attrs, mod->mod_type) &&
		  urlLoopBValues(pb, filter, mod->mod_bvalues, mod->mod_type))
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
urlPlugInit(Slapi_PBlock *pb)
{
  char **argv;
  int argc;
  UrlFilter *new, *last;

  if (!getSlapiArgs(pb, &argc, &argv, 3, PLUGIN_NAME))
    return (-1);

  /* Allocate memory for the new Filter rule.				*/
  if (!(new = (UrlFilter *)slapi_ch_malloc(sizeof(UrlFilter))))
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_MALLOC);

      return (-1);
    }

  /* Create the new filter, using the filter options in argv.		*/
  new->next = (UrlFilter *)NULL;
  new->attrs = (PluginAttrs *)NULL;
  new->match = (*(argv[1]) == '0' ? FALSE : TRUE);
  if (!urlCreateFilter(new, argv[0], argv[2]) || !new || !new->attrs)
    {
      slapi_ch_free((void **)&new);
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_SYNTAX);

      return (-1);
    }

  /* Find the last filter in the chain of filters, and link in the new. */
  if ((last = urlFilters))
    {
      while (last && last->next)
	last = last->next;

      last->next = new;
    }
  else
    urlFilters = new;
  urlNumFilters++;

  /* Ok, everything looks grovy, so let's register the filter, but
     only if we haven't done so already...				*/
  if (urlNumFilters > 1)
    return 0;

  if (slapi_pblock_set(pb, SLAPI_PLUGIN_VERSION, SLAPI_PLUGIN_VERSION_01) ||
      slapi_pblock_set(pb, SLAPI_PLUGIN_DESCRIPTION, &urlDesc) ||
      slapi_pblock_set(pb, SLAPI_PLUGIN_PRE_MODIFY_FN, (void *)&urlModFilter) ||
      slapi_pblock_set(pb, SLAPI_PLUGIN_PRE_ADD_FN, (void *)&urlAddFilter))
    {
      slapi_log_error(LOG_FACILITY, PLUGIN_NAME, ERR_REG);

      return (-1);
    }

  return 0;
}
