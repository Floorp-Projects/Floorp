/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of the Original Code is Alexander. Portions
 * created by Alexander Larsson are Copyright (C) 1999
 * Alexander Larsson. All Rights Reserved. 
 */


/*
 * gtkmozilla.cpp
 */

#include <gtk/gtk.h>
#include "gtkmozilla.h"

static void gtk_mozilla_realize(GtkWidget *widget);
static void gtk_mozilla_finalize (GtkObject *object);

typedef gboolean (*GtkSignal_BOOL__POINTER_INT) (GtkObject * object,
                                                 gpointer arg1,
                                                 gint arg2,
                                                 gpointer user_data);

extern "C" void 
gtk_mozilla_marshal_BOOL__POINTER_INT (GtkObject * object,
                                       GtkSignalFunc func,
                                       gpointer func_data,
                                       GtkArg * args)
{
  GtkSignal_BOOL__POINTER_INT rfunc;
  gboolean *return_val;
  return_val = GTK_RETLOC_BOOL (args[2]);
  rfunc = (GtkSignal_BOOL__POINTER_INT) func;
  *return_val = (*rfunc) (object,
                          GTK_VALUE_POINTER (args[0]),
                          GTK_VALUE_INT (args[1]),
                          func_data);
}

static GtkLayoutClass *parent_class = 0;

static void
gtk_mozilla_class_init (GtkMozillaClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;

  parent_class = (GtkLayoutClass *)gtk_type_class (GTK_TYPE_LAYOUT);

  object_class->finalize = gtk_mozilla_finalize;
  widget_class->realize = gtk_mozilla_realize;

}

static void
gtk_mozilla_realize (GtkWidget *widget)
{
  //printf("gtk_mozilla_realize()\n");
  GtkMozilla *moz = GTK_MOZILLA(widget);

  if (GTK_WIDGET_CLASS (parent_class)->realize)
    (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
}

static void
gtk_mozilla_init (GtkMozilla *moz)
{
  //printf("gtk_mozilla_init()\n");
  
  gtk_layout_set_hadjustment (GTK_LAYOUT (moz), 0);
  gtk_layout_set_vadjustment (GTK_LAYOUT (moz), 0);

  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(moz), GTK_CAN_FOCUS);
}

GtkType
gtk_mozilla_get_type (void)
{
  static GtkType mozilla_type = 0;

  if (!mozilla_type) {
    static const GtkTypeInfo mozilla_info = {
      "GtkMozilla",
      sizeof (GtkMozilla),
      sizeof (GtkMozillaClass),
      (GtkClassInitFunc) gtk_mozilla_class_init,
      (GtkObjectInitFunc) gtk_mozilla_init,
      /* reserved_1 */ NULL,
      /* reserved_2 */ NULL,
      (GtkClassInitFunc) NULL
    };
    mozilla_type = gtk_type_unique (GTK_TYPE_LAYOUT, &mozilla_info);
  }

  return mozilla_type;
}

GtkWidget*
gtk_mozilla_new (void)
{
  GtkMozilla *moz;

  moz = GTK_MOZILLA(gtk_type_new(GTK_TYPE_MOZILLA));
  
  return GTK_WIDGET (moz);
}

static void
gtk_mozilla_finalize (GtkObject *object)
{
  GtkMozilla *moz;

  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_MOZILLA(object));

  moz = GTK_MOZILLA(object);

  GTK_OBJECT_CLASS(parent_class)->finalize (object);
}
