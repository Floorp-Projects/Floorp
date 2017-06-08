/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Objref_h
#define mozilla_mscom_Objref_h

#include "mozilla/NotNull.h"

struct IStream;

namespace mozilla {
namespace mscom {

/**
 * Given a buffer containing a serialized proxy to an interface with a handler,
 * this function strips out the handler and converts it to a standard one.
 * @param aStream IStream whose pointer is positioned at the beginning of the
 *                OBJREF to be stripped. There should be nothing else written
 *                to the stream past the current OBJREF.
 * @return true if the handler was successfully stripped, otherwise false.
 */
bool
StripHandlerFromOBJREF(NotNull<IStream*> aStream);

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Objref_h

