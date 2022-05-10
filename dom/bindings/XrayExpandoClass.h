/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file declares a macro for defining Xray expando classes and declares the
 * default Xray expando class.  The actual definition of that default class
 * lives elsewhere.
 */
#ifndef mozilla_dom_XrayExpandoClass_h
#define mozilla_dom_XrayExpandoClass_h

/*
 * maybeStatic_ Should be either `static` or nothing (because some Xray expando
 * classes are not static).
 *
 * name_ should be the name of the variable.
 *
 * extraSlots_ should be how many extra slots to give the class, in addition to
 * the ones Xray expandos want.
 */
#define DEFINE_XRAY_EXPANDO_CLASS(maybeStatic_, name_, extraSlots_)           \
  maybeStatic_ const JSClass name_ = {                                        \
      "XrayExpandoObject",                                                    \
      JSCLASS_HAS_RESERVED_SLOTS(xpc::JSSLOT_EXPANDO_COUNT + (extraSlots_)) | \
          JSCLASS_FOREGROUND_FINALIZE,                                        \
      &xpc::XrayExpandoObjectClassOps}

namespace mozilla::dom {

extern const JSClass DefaultXrayExpandoObjectClass;

}  // namespace mozilla::dom

#endif /* mozilla_dom_XrayExpandoClass_h */
