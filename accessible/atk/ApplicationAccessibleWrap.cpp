/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ApplicationAccessibleWrap.h"

#include "nsMai.h"
#include "nsAccessibilityService.h"

#include <gtk/gtk.h>
#include "atk/atkobject.h"

using namespace mozilla;
using namespace mozilla::a11y;

// ApplicationAccessibleWrap

ApplicationAccessibleWrap::ApplicationAccessibleWrap() = default;

ApplicationAccessibleWrap::~ApplicationAccessibleWrap() {
  AccessibleWrap::ShutdownAtkObject();
}

gboolean toplevel_event_watcher(GSignalInvocationHint* ihint,
                                guint n_param_values,
                                const GValue* param_values, gpointer data) {
  static GQuark sQuark_gecko_acc_obj = 0;

  if (!sQuark_gecko_acc_obj) {
    sQuark_gecko_acc_obj = g_quark_from_static_string("GeckoAccObj");
  }

  if (nsAccessibilityService::IsShutdown()) return TRUE;

  GObject* object =
      reinterpret_cast<GObject*>(g_value_get_object(param_values));
  if (!GTK_IS_WINDOW(object)) return TRUE;

  AtkObject* child = gtk_widget_get_accessible(GTK_WIDGET(object));
  AtkRole role = atk_object_get_role(child);

  // GTK native dialog
  if (!IS_MAI_OBJECT(child) &&
      (role == ATK_ROLE_DIALOG || role == ATK_ROLE_FILE_CHOOSER ||
       role == ATK_ROLE_COLOR_CHOOSER || role == ATK_ROLE_FONT_CHOOSER)) {
    if (data == reinterpret_cast<gpointer>(nsIAccessibleEvent::EVENT_SHOW)) {
      // Attach the dialog accessible to app accessible tree
      LocalAccessible* windowAcc =
          GetAccService()->AddNativeRootAccessible(child);
      g_object_set_qdata(G_OBJECT(child), sQuark_gecko_acc_obj,
                         reinterpret_cast<gpointer>(windowAcc));

    } else {
      // Deattach the dialog accessible
      LocalAccessible* windowAcc = reinterpret_cast<LocalAccessible*>(
          g_object_get_qdata(G_OBJECT(child), sQuark_gecko_acc_obj));
      if (windowAcc) {
        GetAccService()->RemoveNativeRootAccessible(windowAcc);
        g_object_set_qdata(G_OBJECT(child), sQuark_gecko_acc_obj, nullptr);
      }
    }
  }

  return TRUE;
}

ENameValueFlag ApplicationAccessibleWrap::Name(nsString& aName) const {
  // ATK doesn't provide a way to obtain an application name (for example,
  // Firefox or Thunderbird) like IA2 does. Thus let's return an application
  // name as accessible name that was used to get a branding name (for example,
  // Minefield aka nightly Firefox or Daily aka nightly Thunderbird).
  AppName(aName);
  return eNameOK;
}

void ApplicationAccessibleWrap::GetNativeInterface(void** aOutAccessible) {
  *aOutAccessible = nullptr;

  if (!mAtkObject) {
    mAtkObject = reinterpret_cast<AtkObject*>(
        g_object_new(MAI_TYPE_ATK_OBJECT, nullptr));
    if (!mAtkObject) return;

    atk_object_initialize(mAtkObject, static_cast<Accessible*>(this));
    mAtkObject->role = ATK_ROLE_INVALID;
    mAtkObject->layer = ATK_LAYER_INVALID;
  }

  *aOutAccessible = mAtkObject;
}

struct AtkRootAccessibleAddedEvent {
  AtkObject* app_accessible;
  AtkObject* root_accessible;
  uint32_t index;
};

gboolean fireRootAccessibleAddedCB(gpointer data) {
  AtkRootAccessibleAddedEvent* eventData = (AtkRootAccessibleAddedEvent*)data;
  g_signal_emit_by_name(eventData->app_accessible, "children_changed::add",
                        eventData->index, eventData->root_accessible, nullptr);
  g_object_unref(eventData->app_accessible);
  g_object_unref(eventData->root_accessible);
  free(data);

  return FALSE;
}

bool ApplicationAccessibleWrap::InsertChildAt(uint32_t aIdx,
                                              LocalAccessible* aChild) {
  if (!ApplicationAccessible::InsertChildAt(aIdx, aChild)) return false;

  AtkObject* atkAccessible = AccessibleWrap::GetAtkObject(aChild);
  atk_object_set_parent(atkAccessible, mAtkObject);

  uint32_t count = mChildren.Length();

  // Emit children_changed::add in a timeout
  // to make sure aRootAccWrap is fully initialized.
  AtkRootAccessibleAddedEvent* eventData =
      (AtkRootAccessibleAddedEvent*)malloc(sizeof(AtkRootAccessibleAddedEvent));
  if (eventData) {
    eventData->app_accessible = mAtkObject;
    eventData->root_accessible = atkAccessible;
    eventData->index = count - 1;
    g_object_ref(mAtkObject);
    g_object_ref(atkAccessible);
    g_timeout_add(0, fireRootAccessibleAddedCB, eventData);
  }

  return true;
}

bool ApplicationAccessibleWrap::RemoveChild(LocalAccessible* aChild) {
  int32_t index = aChild->IndexInParent();

  AtkObject* atkAccessible = AccessibleWrap::GetAtkObject(aChild);
  atk_object_set_parent(atkAccessible, nullptr);
  g_signal_emit_by_name(mAtkObject, "children_changed::remove", index,
                        atkAccessible, nullptr);

  return ApplicationAccessible::RemoveChild(aChild);
}
