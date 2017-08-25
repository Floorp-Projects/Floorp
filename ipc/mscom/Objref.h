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
 * @param aStream IStream containing a serialized proxy.
 *                There should be nothing else written to the stream past the
 *                current OBJREF.
 * @param aStart  Absolute position of the beginning of the OBJREF.
 * @param aEnd    Absolute position of the end of the OBJREF.
 * @return true if the handler was successfully stripped, otherwise false.
 */
bool
StripHandlerFromOBJREF(NotNull<IStream*> aStream,
                       const uint64_t aStart,
                       const uint64_t aEnd);

/**
 * Given a buffer containing a serialized proxy to an interface, this function
 * returns the length of the serialized data.
 * @param aStream IStream containing a serialized proxy. The stream pointer
 *                must be positioned at the beginning of the OBJREF.
 * @return The size of the serialized proxy, or 0 on error.
 */
uint32_t
GetOBJREFSize(NotNull<IStream*> aStream);

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Objref_h

