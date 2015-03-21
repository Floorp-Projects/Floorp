/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RootAccessibleWrap_h__
#define mozilla_a11y_RootAccessibleWrap_h__

#include "BaseAccessibles.h"
#include "RootAccessible.h"

namespace mozilla {
namespace a11y {

typedef RootAccessible RootAccessibleWrap;

/* GtkWindowAccessible is the accessible class for gtk+ native window.
 * The instance of GtkWindowAccessible is a child of MaiAppRoot instance.
 * It is added into root when the toplevel window is created, and removed
 * from root when the toplevel window is destroyed.
 */
class GtkWindowAccessible final : public DummyAccessible
{
public:
  explicit GtkWindowAccessible(AtkObject* aAccessible);
  virtual ~GtkWindowAccessible();
};

} // namespace a11y
} // namespace mozilla

#endif   /* mozilla_a11y_Root_Accessible_Wrap_h__ */

