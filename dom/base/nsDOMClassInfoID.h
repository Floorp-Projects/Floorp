/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file defines enum values for all of the DOM objects which have
 * an entry in nsDOMClassInfo.
 */

#ifndef nsDOMClassInfoID_h__
#define nsDOMClassInfoID_h__

#include "nsIXPCScriptable.h"

enum nsDOMClassInfoID
{
  // This one better be the last one in this list
  eDOMClassInfoIDCount
};

/**
 * nsIClassInfo helper macros
 */

#ifdef MOZILLA_INTERNAL_API

class nsIClassInfo;
class nsXPCClassInfo;


#endif // MOZILLA_INTERNAL_API

#endif // nsDOMClassInfoID_h__
