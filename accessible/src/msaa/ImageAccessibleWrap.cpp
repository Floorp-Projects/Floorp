/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageAccessibleWrap.h"

using namespace mozilla;
using namespace mozilla::a11y;

NS_IMPL_ISUPPORTS_INHERITED0(ImageAccessibleWrap,
                             ImageAccessible)

IMPL_IUNKNOWN_INHERITED1(ImageAccessibleWrap,
                         AccessibleWrap,
                         ia2AccessibleImage)

