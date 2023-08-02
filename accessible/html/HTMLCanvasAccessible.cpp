/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLCanvasAccessible.h"

#include "mozilla/a11y/Role.h"

using namespace mozilla::a11y;

HTMLCanvasAccessible::HTMLCanvasAccessible(nsIContent* aContent,
                                           DocAccessible* aDoc)
    : HyperTextAccessible(aContent, aDoc) {}

role HTMLCanvasAccessible::NativeRole() const { return roles::CANVAS; }
