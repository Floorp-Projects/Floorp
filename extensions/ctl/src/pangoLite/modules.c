/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * modules.c:
 *
 * The contents of this file are subject to the Mozilla Public	
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Pango Library (www.pango.org)
 * 
 * The Initial Developer of the Original Code is Red Hat Software
 * Portions created by Red Hat are Copyright (C) 1999 Red Hat Software.
 * 
 * Contributor(s): 
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Lessor General Public License Version 2 (the 
 * "LGPL"), in which case the provisions of the LGPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the LGPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the LGPL. If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * LGPL.
*/
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <gmodule.h>

#include "pango-modules.h"
#include "pango-utils.h"

typedef struct _PangoMapInfo    PangoMapInfo;
typedef struct _PangoEnginePair PangoEnginePair;
typedef struct _PangoSubmap     PangoSubmap;

struct _PangoSubmap
{
  gboolean is_leaf;
  union {
    PangoMapEntry entry;
    PangoMapEntry *leaves;
  } d;
};

struct _PangoMap
{
  gint        n_submaps;
  PangoSubmap submaps[256];
};

struct _PangoMapInfo
{
  const gchar *lang;
  guint       engine_type_id;
  guint       render_type_id;
  PangoMap    *map;
};

struct _PangoEnginePair
{
  PangoEngineInfo info;
  gboolean        included;
  void            *load_info;
  PangoEngine     *engine;
};

static GList *maps = NULL;

static GSList *builtin_engines = NULL;
static GSList *registered_engines = NULL;
static GSList *dlloaded_engines = NULL;

static void build_map(PangoMapInfo *info);
static void init_modules(void);

/**
 * pango_find_map:
 * @lang: the language tag for which to find the map (in the form
 *        en or en_US)
 * @engine_type_id: the render type for the map to find
 * @render_type_id: the engine type for the map to find
 * 
 * Locate a #PangoMap for a particular engine type and render
 * type. The resulting map can be used to determine the engine
 * for each character.
 * 
 * Return value: 
 **/
PangoMap*
pango_find_map(const char *lang, guint engine_type_id, guint render_type_id)
{
  GList        *tmp_list = maps;
  PangoMapInfo *map_info = NULL;
  gboolean     found_earlier = FALSE;

  while (tmp_list) {
    map_info = tmp_list->data;
    if (map_info->engine_type_id == engine_type_id &&
        map_info->render_type_id == render_type_id) {
      if (strcmp(map_info->lang, lang) == 0)
        break;
      else
        found_earlier = TRUE;
    }
    tmp_list = tmp_list->next;
  }
  
  if (!tmp_list) {
    map_info = g_new(PangoMapInfo, 1);
    map_info->lang = g_strdup(lang);
    map_info->engine_type_id = engine_type_id;
    map_info->render_type_id = render_type_id;
    
    build_map(map_info);    
    maps = g_list_prepend(maps, map_info);
  }
  else if (found_earlier) {
    /* Move the found map to the beginning of the list
     * for speed next time around if we had to do
     * any failing strcmps.
     */
    maps = g_list_remove_link(maps, tmp_list);
    maps = g_list_prepend(maps, tmp_list->data);
    g_list_free_1(tmp_list);
  }  
  return map_info->map;
}

static PangoEngine *
pango_engine_pair_get_engine(PangoEnginePair *pair)
{
  if (!pair->engine) {
    if (pair->included) {
      PangoIncludedModule *included_module = pair->load_info;
      
      pair->engine = included_module->load(pair->info.id);
    }
    else {
      GModule *module;
      char    *module_name = pair->load_info;
      PangoEngine *(*load)(const gchar *id);
  	  
      module = g_module_open(module_name, 0);
      if (!module) {
	      fprintf(stderr, "Cannot load module %s: %s\n",
                module_name, g_module_error());
	      return NULL;
	    }
      
      g_module_symbol(module, "script_engine_load", (gpointer)&load);
      if (!load) {
	      fprintf(stderr, "cannot retrieve script_engine_load from %s: %s\n",
                module_name, g_module_error());
	      g_module_close(module);
	      return NULL;
	    }
      
      pair->engine = (*load)(pair->info.id);
    }
  }
  return pair->engine;
}

static void
handle_included_module(PangoIncludedModule *module, GSList **engine_list)
{
  PangoEngineInfo *engine_info;
  int             n_engines, i;

  module->list(&engine_info, &n_engines);
  
  for (i = 0; i < n_engines; i++) {
    PangoEnginePair *pair = g_new(PangoEnginePair, 1);
    
    pair->info = engine_info[i];
    pair->included = TRUE;
    pair->load_info = module;
    pair->engine = NULL;
    
    *engine_list = g_slist_prepend(*engine_list, pair);
  }
}

static gboolean /* Returns true if succeeded, false if failed */
process_module_file(FILE *module_file)
{
  GString  *line_buf = g_string_new(NULL);
  GString  *tmp_buf = g_string_new(NULL);
  gboolean have_error = FALSE;

  while (pango_read_line(module_file, line_buf)) {
    PangoEnginePair *pair = g_new(PangoEnginePair, 1);
    PangoEngineRange *range;
    GList *ranges = NULL;
    GList *tmp_list;
    
    const char *p, *q;
    int        i, start, end;
    
    pair->included = FALSE;
    
    p = line_buf->str;
    
    if (!pango_skip_space(&p)) {
      g_free(pair);
      continue;
    }
    
    i = 0;
    while (1) {
	  if (!pango_scan_string(&p, tmp_buf)) {
      have_error = TRUE;
      goto error;
    }
    
	  switch (i) {
    case 0:
      pair->load_info = g_strdup(tmp_buf->str);
      break;
    case 1:
      pair->info.id = g_strdup(tmp_buf->str);
      break;
    case 2:
      pair->info.engine_type = g_strdup(tmp_buf->str);
      break;
    case 3:
      pair->info.render_type = g_strdup(tmp_buf->str);
      break;
    default:
      range = g_new(PangoEngineRange, 1);
      if (sscanf(tmp_buf->str, "%d-%d:", &start, &end) != 2) {
        fprintf(stderr, "Error reading modules file");
        have_error = TRUE;
        goto error;
      }
      q = strchr(tmp_buf->str, ':');
      if (!q) {
        fprintf(stderr, "Error reading modules file");
        have_error = TRUE;
        goto error;
      }
      q++;
      range->start = start;
      range->end = end;
      range->langs = g_strdup(q);
      
      ranges = g_list_prepend(ranges, range);
    }
    
	  if (!pango_skip_space(&p))
	    break;	  
	  i++;
    }
    
    if (i<3) {
      fprintf(stderr, "Error reading modules file");
      have_error = TRUE;
      goto error;
    }
    
    ranges = g_list_reverse(ranges);
    pair->info.n_ranges = g_list_length(ranges);
    pair->info.ranges = g_new(PangoEngineRange, pair->info.n_ranges);
    
    tmp_list = ranges;
    for (i=0; i<pair->info.n_ranges; i++) {
      pair->info.ranges[i] = *(PangoEngineRange *)tmp_list->data;
      tmp_list = tmp_list->next;
    }
    
    pair->engine = NULL;    
    dlloaded_engines = g_slist_prepend(dlloaded_engines, pair);
    
  error:
    g_list_foreach(ranges, (GFunc)g_free, NULL);
    g_list_free(ranges);
    
    if (have_error) {
      g_free(pair);
      break;
    }
  }
  
  g_string_free(line_buf, TRUE);
  g_string_free(tmp_buf, TRUE);  
  return !have_error;
}

static void
read_modules(void)
{
  FILE *module_file;  
  char *file_str = pango_config_key_get("Pango/ModuleFiles");
  char **files;
  int  n;

  if (!file_str)
    file_str = g_strconcat(pango_get_sysconf_subdirectory(),
                           G_DIR_SEPARATOR_S "pango.modules", NULL);

  files = pango_split_file_list(file_str);

  n = 0;
  while (files[n])
    n++;
  
  while (n-- > 0) {
    module_file = fopen(files[n], "r");
    if (!module_file)
      g_warning("Error opening module file '%s': %s\n", files[n], g_strerror(errno));
    else {
      process_module_file(module_file);
      fclose(module_file);
    }
  }
  
  g_strfreev(files);
  g_free(file_str);  
  dlloaded_engines = g_slist_reverse(dlloaded_engines);
}

static void
set_entry(PangoMapEntry *entry, gboolean is_exact, PangoEngineInfo *info)
{
  if ((is_exact && !entry->is_exact) || !entry->info) {
    entry->is_exact = is_exact;
    entry->info = info;
  }
}

static void
init_modules(void)
{
  static gboolean init = FALSE;
  
  if (init)
    return;
  else
    init = TRUE;
  
  read_modules();
}

static void
map_add_engine(PangoMapInfo *info, PangoEnginePair *pair)
{
  int      i, j, submap;
  PangoMap *map = info->map;
 
  for (i=0; i<pair->info.n_ranges; i++) {
    gchar    **langs;
    gboolean is_exact = FALSE;
    
    if (pair->info.ranges[i].langs) {
      langs = g_strsplit(pair->info.ranges[i].langs, ";", -1);
      for (j=0; langs[j]; j++)
        if (strcmp(langs[j], "*") == 0 || strcmp(langs[j], info->lang) == 0) {
          is_exact = TRUE;
          break;
	      }
      g_strfreev(langs);
    }
    
    for (submap = pair->info.ranges[i].start / 256;
         submap <= pair->info.ranges[i].end / 256; submap ++) {
      gunichar start;
      gunichar end;
      
      if (submap == pair->info.ranges[i].start / 256)
        start = pair->info.ranges[i].start % 256;
      else
        start = 0;
      
      if (submap == pair->info.ranges[i].end / 256)
        end = pair->info.ranges[i].end % 256;
      else
        end = 255;
      
      if (map->submaps[submap].is_leaf && start == 0 && end == 255) {
        set_entry(&map->submaps[submap].d.entry, is_exact, &pair->info);
	    }
      else {
	      if (map->submaps[submap].is_leaf) {
          map->submaps[submap].is_leaf = FALSE;
          map->submaps[submap].d.leaves = g_new(PangoMapEntry, 256);
          for (j=0; j<256; j++) {
            map->submaps[submap].d.leaves[j].info = NULL;
            map->submaps[submap].d.leaves[j].is_exact = FALSE;
          }
        }
	      
	      for (j=start; j<=end; j++)
          set_entry(&map->submaps[submap].d.leaves[j], is_exact, &pair->info);
	    }
    }
  }
}

static void
map_add_engine_list(PangoMapInfo *info,
                    GSList       *engines,
                    const char   *engine_type,
                    const char   *render_type)  
{
  GSList *tmp_list = engines;
  
  while (tmp_list) {
    PangoEnginePair *pair = tmp_list->data;
    tmp_list = tmp_list->next;
    
    if (strcmp(pair->info.engine_type, engine_type) == 0 &&
        strcmp(pair->info.render_type, render_type) == 0) {
      map_add_engine(info, pair);
    }
  }
}

static void
build_map(PangoMapInfo *info)
{
  int      i;
  PangoMap *map;

  const char *engine_type = g_quark_to_string(info->engine_type_id);
  const char *render_type = g_quark_to_string(info->render_type_id);
  
  init_modules();

  info->map = map = g_new(PangoMap, 1);
  map->n_submaps = 0;
  for (i=0; i<256; i++) {
    map->submaps[i].is_leaf = TRUE;
    map->submaps[i].d.entry.info = NULL;
    map->submaps[i].d.entry.is_exact = FALSE;
  }
  
  map_add_engine_list(info, dlloaded_engines, engine_type, render_type);  
  map_add_engine_list(info, registered_engines, engine_type, render_type);  
  map_add_engine_list(info, builtin_engines, engine_type, render_type);  
}

/**
 * pango_map_get_entry:
 * @map: a #PangoMap
 * @wc:  an ISO-10646 codepoint
 * 
 * Returns the entry in the map for a given codepoint. The entry
 * contains information about engine that should be used for
 * the codepoint and also whether the engine matches the language
 * tag for the map was created exactly or just approximately.
 * 
 * Return value: the #PangoMapEntry for the codepoint. This value
 *   is owned by the #PangoMap and should not be freed.
 **/
PangoMapEntry *
pango_map_get_entry(PangoMap *map, guint32 wc)
{
  PangoSubmap *submap = &map->submaps[wc / 256];
  return submap->is_leaf ? &submap->d.entry : &submap->d.leaves[wc % 256];
}

/**
 * pango_map_get_engine:
 * @map: a #PangoMap
 * @wc:  an ISO-10646 codepoint
 * 
 * Returns the engine listed in the map for a given codepoint. 
 * 
 * Return value: the engine, if one is listed for the codepoint,
 *    or %NULL. The lookup may cause the engine to be loaded;
 *    once an engine is loaded
 **/
PangoEngine *
pango_map_get_engine(PangoMap *map, guint32 wc)
{
  PangoSubmap *submap = &map->submaps[wc / 256];
  PangoMapEntry *entry = submap->is_leaf ? &submap->d.entry : 
    &submap->d.leaves[wc % 256];
  
  if (entry->info)
    return pango_engine_pair_get_engine((PangoEnginePair *)entry->info);
  else
    return NULL;
}

/**
 * pango_module_register:
 * @module: a #PangoIncludedModule
 * 
 * Registers a statically linked module with Pango. The
 * #PangoIncludedModule structure that is passed in contains the
 * functions that would otherwise be loaded from a dynamically loaded
 * module.
 **/
void
pango_module_register(PangoIncludedModule *module)
{
  GSList *tmp_list = NULL;
  
  handle_included_module(module, &tmp_list);  
  registered_engines = g_slist_concat(registered_engines, 
                                      g_slist_reverse(tmp_list));
}
