// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class assists you in dealing with a specific situation when managing
// ownership between a C++ object and a GTK widget.  It is common to have a
// C++ object which encapsulates a GtkWidget, and that widget is exposed from
// the object for use outside of the class.  In this situation, you commonly
// want the GtkWidget's lifetime to match its C++ object's lifetime.  Using an
// OwnedWigetGtk will take ownership over the initial reference of the
// GtkWidget, so that it is "owned" by the C++ object.  Example usage:
//
// class FooViewGtk() {
//  public:
//   FooViewGtk() { }
//   ~FooViewGtk() { widget_.Destroy(); }
//   void Init() { vbox_.Own(gtk_vbox_new()); }
//   GtkWidget* widget() { return vbox_.get() };  // Host my widget!
//  private:
//   OwnedWidgetGtk vbox_;
// };
//
// This design will ensure that the widget stays alive from the call to Own()
// until the call to Destroy().
//
// - Details of the problem and OwnedWidgetGtk's solution:
// In order to make passing ownership more convenient for newly created
// widgets, GTK has a concept of a "floating" reference.  All GtkObjects (and
// thus GtkWidgets) inherit from GInitiallyUnowned.  When they are created, the
// object starts with a reference count of 1, but has its floating flag set.
// When it is put into a container for the first time, that container will
// "sink" the floating reference, and the count will still be 1.  Now the
// container owns the widget, and if we remove the widget from the container,
// the widget is destroyed.  This style of ownership often causes problems when
// you have an object encapsulating the widget.  If we just use a raw
// GtkObject* with no specific ownership management, we push the widget's
// ownership onto the user of the class.  Now the C++ object can't depend on
// the widget being valid, since it doesn't manage its lifetime.  If the widget
// was removed from a container, removing its only reference, it would be
// destroyed (from the C++ object's perspective) unexpectantly destroyed.  The
// solution is fairly simple, make sure that the C++ object owns the widget,
// and thus it is also responsible for destroying it.  This boils down to:
//   GtkWidget* widget = gtk_widget_new();
//   g_object_ref_sink(widget);  // Claim the initial floating reference.
//   ...
//   gtk_destroy_widget(widget);  // Ask all code to destroy their references.
//   g_object_unref(widget);  // Destroy the initial reference we had claimed.

#ifndef BASE_OWNED_WIDGET_GTK_H_
#define BASE_OWNED_WIDGET_GTK_H_

#include "base/basictypes.h"

typedef struct _GtkWidget GtkWidget;

class OwnedWidgetGtk {
 public:
  // Create an instance that isn't managing any ownership.
  OwnedWidgetGtk() : widget_(NULL) { }
  // Create an instance that owns |widget|.
  explicit OwnedWidgetGtk(GtkWidget* widget) : widget_(NULL) { Own(widget); }

  ~OwnedWidgetGtk();

  // Return the currently owned widget, or NULL if no widget is owned.
  GtkWidget* get() const { return widget_; }

  // Takes ownership of a widget, by taking the initial floating reference of
  // the GtkWidget.  It is expected that Own() is called right after the widget
  // has been created, and before any other references to the widget might have
  // been added.  It is valid to never call Own(), in which case Destroy() will
  // do nothing.  If Own() has been called, you must explicitly call Destroy().
  void Own(GtkWidget* widget);

  // You must call Destroy() after you have called Own().  Calling Destroy()
  // will call gtk_widget_destroy(), and drop our reference to the widget.
  // After a call to Destroy(), you may call Own() again.  NOTE: It is expected
  // that after gtk_widget_destroy we will be holding the only reference left
  // on the object.  We assert this in debug mode to help catch any leaks.
  void Destroy();

 private:
  GtkWidget* widget_;

  DISALLOW_COPY_AND_ASSIGN(OwnedWidgetGtk);
};

#endif  // BASE_OWNED_WIDGET_GTK_H_
