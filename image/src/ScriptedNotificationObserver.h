/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScriptedNotificationObserver_h
#define ScriptedNotificationObserver_h

#include "imgINotificationObserver.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

class imgIScriptedNotificationObserver;

namespace mozilla {
namespace image {

class ScriptedNotificationObserver : public imgINotificationObserver
{
public:
  ScriptedNotificationObserver(imgIScriptedNotificationObserver* aInner);
  virtual ~ScriptedNotificationObserver() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER
  NS_DECL_CYCLE_COLLECTION_CLASS(ScriptedNotificationObserver)

private:
  nsCOMPtr<imgIScriptedNotificationObserver> mInner;
};

}}

#endif
