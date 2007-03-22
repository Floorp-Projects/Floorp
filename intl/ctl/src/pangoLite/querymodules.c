/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * querymodules.c:
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
 * The Original Code is the Pango Library (www.pango.org).
 *
 * The Initial Developer of the Original Code is
 * Red Hat Software.
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#include "config.h"

#include <glib.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#include <gmodule.h>
#include "pango-break.h"
#include "pango-context.h"
#include "pango-utils.h"
#include "pango-engine.h"

#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>

#ifdef G_OS_WIN32
#define SOEXT ".dll"
#ifndef PATH_MAX
#include <stdlib.h>
#define PATH_MAX _MAX_PATH
#endif /* PATH_MAX */
#include <direct.h>		/* for getcwd() with MSVC */
#include <io.h>			/* for getcwd() with mingw */
#define getcwd _getcwd
#else
#define SOEXT ".so"
#endif

void 
query_module (const char *dir, const char *name)
{
  void (*list) (PangoliteEngineInfo **engines, gint *n_engines);
  PangoliteEngine *(*load) (const gchar *id);
  void (*unload) (PangoliteEngine *engine);

  GModule *module;
  gchar *path;

  if (name[0] == G_DIR_SEPARATOR)
    path = g_strdup (name);
  else
    path = g_strconcat (dir, G_DIR_SEPARATOR_S, name, NULL);
  
  module = g_module_open (path, 0);

  if (!module)
    fprintf(stderr, "Cannot load module %s: %s\n", path, g_module_error());
	  
  if (module &&
      g_module_symbol (module, "script_engine_list", (gpointer)&list) &&
      g_module_symbol (module, "script_engine_load", (gpointer)&load) &&
      g_module_symbol (module, "script_engine_unload", (gpointer)&unload))
    {
      gint i,j;
      PangoliteEngineInfo *engines;
      gint n_engines;

      (*list) (&engines, &n_engines);

      for (i=0; i<n_engines; i++)
	{
	  const gchar *quote;
	  gchar *quoted_path;

	  if (strchr (path, ' ') != NULL)
	    {
	      quote = "\"";
	      quoted_path = g_strescape (path, NULL);
	    }
	  else
	    {
	      quote = "";
	      quoted_path = g_strdup (path);
	    }
	  
	  g_print ("%s%s%s %s %s %s ", quote, quoted_path, quote,
		   engines[i].id, engines[i].engine_type, engines[i].render_type);
	  g_free (quoted_path);

	  for (j=0; j < engines[i].n_ranges; j++)
	    {
	      if (j != 0)
		g_print (" ");
	      g_print ("%d-%d:%s",
		       engines[i].ranges[j].start,
		       engines[i].ranges[j].end,
		       engines[i].ranges[j].langs);
	    }
	  g_print ("\n");
	  }
    }
  else
    {
      fprintf (stderr, "%s does not export Pangolite module API\n", path);
    }

  g_free (path);
  if (module)
    g_module_close (module);
}		       

int main (int argc, char **argv)
{
  char cwd[PATH_MAX];
  int i;
  char *path;

  printf ("# Pangolite Modules file\n"
	  "# Automatically generated file, do not edit\n"
	  "#\n");

  if (argc == 1)		/* No arguments given */
    {
      char **dirs;
      int i;
      
      path = pangolite_config_key_get ("Pangolite/ModulesPath");
      if (!path)
	path = g_strconcat (pangolite_get_lib_subdirectory (),
			    G_DIR_SEPARATOR_S "modules",
			    NULL);

      printf ("# ModulesPath = %s\n#\n", path);

      dirs = pangolite_split_file_list (path);

      for (i=0; dirs[i]; i++)
	{
	  DIR *dir = opendir (dirs[i]);
	  if (dir)
	    {
	      struct dirent *dent;

	      while ((dent = readdir (dir)))
		{
		  int len = strlen (dent->d_name);
		  if (len > 3 && strcmp (dent->d_name + len - strlen (SOEXT), SOEXT) == 0)
		    query_module (dirs[i], dent->d_name);
		}
	      
	      closedir (dir);
	    }
	}
    }
  else
    {
      getcwd (cwd, PATH_MAX);
      
      for (i=1; i<argc; i++)
	query_module (cwd, argv[i]);
    }
  
  return 0;
}
