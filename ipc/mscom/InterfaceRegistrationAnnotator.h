/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_InterfaceRegistrationAnnotator_h
#define mozilla_mscom_InterfaceRegistrationAnnotator_h

#if !defined(MOZ_CRASHREPORTER)
#error "This header requires crash reporting to be enabled"
#endif

namespace mozilla {
namespace mscom {

void AnnotateInterfaceRegistration(REFIID aIid);

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_InterfaceRegistrationAnnotator_h
