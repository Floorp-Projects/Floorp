/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkStatusbar Copyright (C) 1998 Shawn T. Amundson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtk/gtk.h>
#include "moz-statusbar.h"
#include <gnome.h>

static void moz_statusbar_class_init               (MozStatusbarClass *class);
static void moz_statusbar_init                     (MozStatusbar      *statusbar);
static void moz_statusbar_destroy                  (GtkObject         *object);
static void moz_statusbar_finalize                 (GtkObject         *object);
static void moz_statusbar_update		   (MozStatusbar      *statusbar,
						    guint	       context_id,
						    const gchar       *text);
     
static GtkContainerClass *parent_class;

guint      
moz_statusbar_get_type ()
{
  static guint statusbar_type = 0;

  if (!statusbar_type)
    {
      GtkTypeInfo statusbar_info =
      {
        "MozStatusbar",
        sizeof (MozStatusbar),
        sizeof (MozStatusbarClass),
        (GtkClassInitFunc) moz_statusbar_class_init,
        (GtkObjectInitFunc) moz_statusbar_init,
        (GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      statusbar_type = gtk_type_unique (gtk_hbox_get_type (), &statusbar_info);
    }

  return statusbar_type;
}

static void
moz_statusbar_class_init (MozStatusbarClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = gtk_type_class (gtk_hbox_get_type ());

  object_class->destroy = moz_statusbar_destroy;
  object_class->finalize = moz_statusbar_finalize;
}

static void
moz_statusbar_init (MozStatusbar *statusbar)
{
  GtkBox *box;

  box = GTK_BOX (statusbar);

  box->spacing = 4;
  box->homogeneous = FALSE;

  statusbar->sec_pixmap = gnome_pixmap_new_from_file("images/Dash_Unsecure.xpm");
  gtk_box_pack_start (box, statusbar->sec_pixmap, FALSE, FALSE, 0);
  gtk_widget_show (statusbar->sec_pixmap);

  statusbar->progress = gtk_progress_bar_new();
  gtk_box_pack_start (box, statusbar->progress, FALSE, FALSE, 0);
  gtk_widget_show (statusbar->progress);

  statusbar->label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (statusbar->label), 0.0, 0.0);
  gtk_box_pack_start (box, statusbar->label, TRUE, TRUE, 0);
  gtk_widget_show (statusbar->label);
}

GtkWidget* 
moz_statusbar_new ()
{
  return gtk_type_new (moz_statusbar_get_type ());
}

void
moz_statusbar_set_label(MozStatusbar *statusbar,
			const gchar *text)
{
  if (text == NULL) text = "";

  gtk_label_set(GTK_LABEL(statusbar->label), text);
}

void
moz_statusbar_set_percentage(MozStatusbar *statusbar,
			     guint percentage)
{
  gtk_progress_bar_update(GTK_PROGRESS_BAR(statusbar->progress), ((gfloat)percentage)/100.0);
}

static void
moz_statusbar_destroy (GtkObject *object)
{
  MozStatusbar *statusbar;
  MozStatusbarClass *class;
  GSList *list;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_STATUSBAR (object));

  statusbar = MOZ_STATUSBAR (object);
  class = MOZ_STATUSBAR_CLASS (GTK_OBJECT (statusbar)->klass);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
moz_statusbar_finalize (GtkObject *object)
{
  MozStatusbar *statusbar;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_STATUSBAR (object));

  statusbar = MOZ_STATUSBAR (object);

  GTK_OBJECT_CLASS (parent_class)->finalize (object);
}
