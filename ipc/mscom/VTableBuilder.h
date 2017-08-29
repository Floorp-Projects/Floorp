/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_VTableBuilder_h
#define mozilla_mscom_VTableBuilder_h

#include <stdint.h>
#include <unknwn.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * This function constructs an interface with |aVtblSize| entries, where the
 * IUnknown methods are valid and redirect to |aUnk|. Other than the IUnknown
 * methods, the resulting interface is not intended to actually be called; the
 * remaining vtable entries are null pointers to enforce that.
 *
 * @param aUnk The IUnknown to which the null vtable will redirect its IUnknown
 *             methods.
 * @param aVtblSize The total size of the vtable, including the IUnknown
 *                  entries (thus this parameter must be >= 3).
 * @return The resulting IUnknown, or nullptr on error.
 */
IUnknown* BuildNullVTable(IUnknown* aUnk, uint32_t aVtblSize);

void DeleteNullVTable(IUnknown* aUnk);

#if defined(__cplusplus)
}
#endif

#endif //  mozilla_mscom_VTableBuilder_h
