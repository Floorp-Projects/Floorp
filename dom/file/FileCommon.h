/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_filecommon_h__
#define mozilla_dom_file_filecommon_h__

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
#include "nsDebug.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

#define BEGIN_FILE_NAMESPACE \
  namespace mozilla { namespace dom { namespace file {
#define END_FILE_NAMESPACE \
  } /* namespace file */ } /* namespace dom */ } /* namespace mozilla */
#define USING_FILE_NAMESPACE \
  using namespace mozilla::dom::file;

#endif // mozilla_dom_file_filecommon_h__
