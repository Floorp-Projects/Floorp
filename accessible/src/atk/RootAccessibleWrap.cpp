/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#include "nsMai.h"

using namespace mozilla::a11y;

NativeRootAccessibleWrap::NativeRootAccessibleWrap(AtkObject* aAccessible):
  RootAccessible(nsnull, nsnull, nsnull)
{
  // XXX: mark the object as defunct to ensure no single internal method is
  // running on it.
  mFlags |= eIsDefunct;

  g_object_ref(aAccessible);
  mAtkObject = aAccessible;
}

NativeRootAccessibleWrap::~NativeRootAccessibleWrap()
{
  g_object_unref(mAtkObject);
  mAtkObject = nsnull;
}
