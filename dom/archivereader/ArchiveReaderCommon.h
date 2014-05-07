/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_archivereader_archivereader_h
#define mozilla_dom_archivereader_archivereader_h

#include "mozilla/DOMEventTargetHelper.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsString.h"
#include "nsTArray.h"

#define BEGIN_ARCHIVEREADER_NAMESPACE \
  namespace mozilla { namespace dom { namespace archivereader {
#define END_ARCHIVEREADER_NAMESPACE \
  } /* namespace archivereader */ } /* namespace dom */ } /* namespace mozilla */
#define USING_ARCHIVEREADER_NAMESPACE \
  using namespace mozilla::dom::archivereader;

#endif // mozilla_dom_archivereader_archivereadercommon_h
