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

#ifndef __MOZ_STATUSBAR_H__
#define __MOZ_STATUSBAR_H__

#include <gtk/gtkhbox.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MOZ_STATUSBAR(obj)          GTK_CHECK_CAST (obj, moz_statusbar_get_type (), MozStatusbar)
#define MOZ_STATUSBAR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, moz_statusbar_get_type (), MozStatusbarClass)
#define MOZ_IS_STATUSBAR(obj)       GTK_CHECK_TYPE (obj, moz_statusbar_get_type ())

typedef struct _MozStatusbar      MozStatusbar;
typedef struct _MozStatusbarClass MozStatusbarClass;
typedef struct _MozStatusbarMsg MozStatusbarMsg;

struct _MozStatusbar
{
  GtkHBox parent_widget;

  GtkWidget *sec_frame;
  GtkWidget *sec_pixmap;

  GtkWidget *progress;

  GtkWidget *label;
};

struct _MozStatusbarClass
{
  GtkHBoxClass parent_class;
};

guint      moz_statusbar_get_type     	(void);
GtkWidget* moz_statusbar_new          	();

void	   moz_statusbar_set_security_state (MozStatusbar *statusbar,
					     gint sec_state);

void	   moz_statusbar_set_label	(MozStatusbar *statusbar,
					 const gchar *text);
void	   moz_statusbar_set_percentage	(MozStatusbar *statusbar,
					 guint percentage);

#ifdef __cplusplus
} 
#endif /* __cplusplus */
#endif /* __MOZ_STATUSBAR_H__ */
