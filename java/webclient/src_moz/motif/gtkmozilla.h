/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Alexander
 * Larsson. Portions created by Alexander Larsson are
 * Copyright (C) 1999 Alexander Larsson. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
 * gtkmozilla.h
 */

#ifndef GTKMOZILLA_H
#define GTKMOZILLA_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtklayout.h>

#define GTK_TYPE_MOZILLA              (gtk_mozilla_get_type ())
#define GTK_MOZILLA(obj)              GTK_CHECK_CAST ((obj), GTK_TYPE_MOZILLA, GtkMozilla)
#define GTK_MOZILLA_CLASS(klass)      GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_MOZILLA, GtkMozillaClass)
#define GTK_IS_MOZILLA(obj)           GTK_CHECK_TYPE ((obj), GTK_TYPE_MOZILLA)
#define GTK_IS_MOZILLA_CLASS(klass)   GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MOZILLA)


typedef struct _GtkMozilla       GtkMozilla;
typedef struct _GtkMozillaClass  GtkMozillaClass;
  
struct _GtkMozilla
{
  GtkLayout layout;
};

struct _GtkMozillaClass
{
  GtkLayoutClass parent_class;
};

extern GtkType    gtk_mozilla_get_type(void);
extern GtkWidget* gtk_mozilla_new(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* GTKMOZILLA_H */
