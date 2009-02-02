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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
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

#include "gtkmozembed.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <stdio.h>
#include <stdlib.h>

#include "nsStringAPI.h"
#include "gtkmozembed_glue.cpp"

int (*old_handler) (Display *, XErrorEvent *);

int error_handler (Display *d, XErrorEvent *e)
{
  if ((e->error_code == BadWindow) || (e->error_code == BadDrawable))
    {
      XID resourceid = e->resourceid;
      GdkWindow *window = gdk_window_lookup (resourceid);

      if (window)
        {
          if (!g_dataset_get_data (window, "bonobo-error"))
            {
              g_dataset_set_data_full (window, "bonobo-error",
                                       g_new (gint, 1),
                                       (GDestroyNotify)g_free);

              g_warning ("Error accessing window %ld", resourceid);
            }
        }
      return 0;
    }
  else
    {
      return (*old_handler)(d, e);
    }
}

void
load_page(gpointer data);

GtkWidget *embed = 0;
GtkWidget *entry = 0;

int
main(int argc, char **argv)
{
  guint32 xid;

  GtkWidget *window;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *button;

  gtk_init(&argc, &argv);

  static const GREVersionRange greVersion = {
    "1.9a", PR_TRUE,
    "2", PR_TRUE
  };

  char xpcomPath[PATH_MAX];

  nsresult rv = GRE_GetGREPathWithProperties(&greVersion, 1, nsnull, 0,
                                             xpcomPath, sizeof(xpcomPath));
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Couldn't find a compatible GRE.\n");
    return 1;
  }

  rv = XPCOMGlueStartup(xpcomPath);
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Couldn't start XPCOM.");
    return 1;
  }

  rv = GTKEmbedGlueStartup();
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Couldn't find GTKMozEmbed symbols.");
    return 1;
  }

  char *lastSlash = strrchr(xpcomPath, '/');
  if (lastSlash)
    *lastSlash = '\0';

  gtk_moz_embed_set_path(xpcomPath);

  old_handler = XSetErrorHandler (error_handler);

  if (argc < 2) {
    fprintf(stderr, "Usage: TestGtkEmbedChild WINDOW_ID\n");
    exit(1);
  }

  xid = strtol (argv[1], (char **)NULL, 0);

  if (xid == 0) {
    fprintf(stderr, "Invalid window id '%s'\n", argv[1]);
    exit(1);
  }

  window = gtk_plug_new(xid);

  g_signal_connect(GTK_OBJECT(window), "destroy",
                   G_CALLBACK(gtk_main_quit), NULL);

  gtk_container_set_border_width(GTK_CONTAINER(window), 0);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);
  gtk_widget_show(vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  button = gtk_button_new_with_label("Load");
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(load_page), NULL);

  embed = gtk_moz_embed_new();
  gtk_box_pack_start(GTK_BOX(vbox), embed, TRUE, TRUE, 0);
  gtk_widget_set_size_request(embed, 200, 200);
  gtk_widget_show(embed);

  gtk_widget_show(window);
  
  gtk_main();

  fprintf(stderr, "exiting.\n");

  return 0;
}

void
load_page(gpointer data)
{
  gchar *text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
  g_print("load url %s\n", text);
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(embed), text);
  g_free(text);
}
