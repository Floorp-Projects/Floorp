/*  -*- Mode: C; eval: (c-set-style "GNU") -*-
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <slapi-plugin.h>
#include <regex.h>

#define LOG_FACILITY	SLAPI_LOG_PLUGIN
#define PLUGIN_NAME	"rex_filter"

#define ERR_NOMATCH	"An attribute does not match a server regex rule.\n"
#define ERR_MATCH	"An attribute matches a server regex rule.\n"

#define ERR_MALLOC	"Can't allocate memory, which is a Bad Thing(tm).\n"
#define ERR_NOMODS	"Could not get the modifications.\n"
#define ERR_NOENTRY	"Could not get entry.\n"

#ifdef __GNUC__
#  define INLINE inline
#else /* not __GNUC__ */
#  define INLINE
#endif /* __GNUC__     */

typedef struct berval BerVal;

typedef struct _Rex_Filter_Attrs
{
  char *type;
  char first;
  int len;
  struct _Rex_Filter_Attrs *next;
} Rex_Filter_Attrs;

typedef struct _Rex_Filter
{
  char *string;
  char *attributes;
  regex_t *regex;
  int match;
  Rex_Filter_Attrs *attrs;
  struct _Rex_Filter *next;
} Rex_Filter;

static int rex_num_filters = 0;
static Rex_Filter *rex_filter_list = NULL;
static Slapi_PluginDesc rex_descript = { PLUGIN_NAME,
					 "Leif Hedstrom",
					 "1.0",
					 "Regex filter plugin" };

static int
free_attributes(Rex_Filter_Attrs *attrs)
{
  Rex_Filter_Attrs *cur;

  if (!attrs)
    return 0;

  while (attrs)
    {
      cur = attrs;
      attrs = cur->next;
      slapi_ch_free((void **)&cur);
    }

  return 1;
}

static Rex_Filter_Attrs *
parse_attributes(char *str)
{
  char *tmp;
  Rex_Filter_Attrs *cur, *ret;

  if (!str)
    return (Rex_Filter_Attrs *)NULL;

  tmp = str;
  while (*tmp)
    {
      *tmp = tolower((int)*tmp);
      tmp++;
    }
  if (!(ret = (Rex_Filter_Attrs *)slapi_ch_malloc(sizeof(Rex_Filter_Attrs))))
    return (Rex_Filter_Attrs *)NULL;

  cur = ret;
  tmp = strtok(str, ",");
  while (tmp)
    {
      if (!cur)
	{
	  free_attributes(ret);
	  return (Rex_Filter_Attrs *)NULL;
	}

      cur->type = tmp;
      cur->first = *tmp;
      cur->len = strlen(tmp);

      if ((tmp = strtok(NULL, ",")))
	cur->next = (Rex_Filter_Attrs *)
	  slapi_ch_malloc(sizeof(Rex_Filter_Attrs));
      else
	cur->next = (Rex_Filter_Attrs *)NULL;

      cur = cur->next;
    }

  return ret;
}

static INLINE int
list_has_attribute(Rex_Filter_Attrs *attrs, char *type)
{
  int len;

  if (!attrs || !type)
    return 0;

  len = strlen(type);
  while (attrs)
    {
      if ((attrs->first == *type) &&
	  (attrs->len == len) && (!strcmp(attrs->type, type)))
	return 1;

      attrs = attrs->next;
    }

  return 0;
}

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
  Rex_Filter_Attrs *attrs;
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
  new->attrs = (Rex_Filter_Attrs *)NULL;
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
