/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILCOMPOSITORTABLE_H_
#define NS_SMILCOMPOSITORTABLE_H_

#include "nsTHashtable.h"

//----------------------------------------------------------------------
// nsSMILCompositorTable : A hashmap of nsSMILCompositors
//
// This is just a forward-declaration because it is included in
// nsSMILAnimationController which is used in nsDocument. We don't want to
// expose all of nsSMILCompositor or otherwise any changes to it will mean the
// whole world will need to be rebuilt.

class nsSMILCompositor;
typedef nsTHashtable<nsSMILCompositor> nsSMILCompositorTable;

#endif // NS_SMILCOMPOSITORTABLE_H_
