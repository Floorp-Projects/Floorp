/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationLog_h
#define mozilla_dom_PresentationLog_h

/*
 * MOZ_LOG=Presentation:5
 * For detail, see PresentationService.cpp
 */
namespace mozilla {
namespace dom {
extern mozilla::LazyLogModule gPresentationLog;
}
}

#undef PRES_ERROR
#define PRES_ERROR(...) MOZ_LOG(mozilla::dom::gPresentationLog, mozilla::LogLevel::Error, (__VA_ARGS__))

#undef PRES_DEBUG
#define PRES_DEBUG(...) MOZ_LOG(mozilla::dom::gPresentationLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

#endif // mozilla_dom_PresentationLog_h
