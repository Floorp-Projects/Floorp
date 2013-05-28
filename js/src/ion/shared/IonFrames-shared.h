/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_ionframes_shared_h__
#define jsion_ionframes_shared_h__

#define ION_FRAME_DOMGETTER ((IonCode *)0x1)
#define ION_FRAME_DOMSETTER ((IonCode *)0x2)
#define ION_FRAME_DOMMETHOD ((IonCode *)0x3)
#define ION_FRAME_OOL_NATIVE_GETTER ((IonCode *)0x4)
#define ION_FRAME_OOL_PROPERTY_OP   ((IonCode *)0x5)
#define ION_FRAME_OOL_PROXY_GET     ((IonCode *)0x6)

#endif
