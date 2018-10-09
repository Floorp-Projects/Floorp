/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionAccessibility.h"
#include "AndroidUiThread.h"
#include "nsThreadUtils.h"
#include "HyperTextAccessible.h"
#include "JavaBuiltins.h"
#include "RootAccessibleWrap.h"
#include "nsAccessibilityService.h"

#ifdef DEBUG
#include <android/log.h>
#define AALOG(args...)                                                         \
  __android_log_print(ANDROID_LOG_INFO, "GeckoAccessibilityNative", ##args)
#else
#define AALOG(args...)                                                         \
  do {                                                                         \
  } while (0)
#endif

template<>
const char nsWindow::NativePtr<mozilla::a11y::SessionAccessibility>::sName[] =
  "SessionAccessibility";

using namespace mozilla::a11y;

class Settings final
  : public mozilla::java::SessionAccessibility::Settings::Natives<Settings>
{
public:
  static void ToggleNativeAccessibility(bool aEnable)
  {
    if (aEnable) {
      GetOrCreateAccService();
    } else {
      MaybeShutdownAccService(nsAccessibilityService::ePlatformAPI);
    }
  }
};

void
SessionAccessibility::SetAttached(bool aAttached,
                                  already_AddRefed<Runnable> aRunnable)
{
  if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
    uiThread->Dispatch(NS_NewRunnableFunction(
      "SessionAccessibility::Attach",
      [aAttached,
       sa = java::SessionAccessibility::NativeProvider::GlobalRef(
           mSessionAccessibility),
       runnable = RefPtr<Runnable>(aRunnable)] {
        sa->SetAttached(aAttached);
        if (runnable) {
          runnable->Run();
        }
      }));
  }
}

void
SessionAccessibility::Init()
{
  java::SessionAccessibility::NativeProvider::Natives<
    SessionAccessibility>::Init();
  Settings::Init();
}

mozilla::jni::Object::LocalRef
SessionAccessibility::GetNodeInfo(int32_t aID)
{
  java::GeckoBundle::GlobalRef ret = nullptr;
  RefPtr<SessionAccessibility> self(this);
  nsAppShell::SyncRunEvent([this, self, aID, &ret] {
    if (RootAccessibleWrap* rootAcc = GetRoot()) {
      AccessibleWrap* acc = rootAcc->FindAccessibleById(aID);
      if (acc) {
        ret = acc->ToBundle();
      } else {
        AALOG("oops, nothing for %d", aID);
      }
    }
  });

  return mozilla::jni::Object::Ref::From(ret);
}

RootAccessibleWrap*
SessionAccessibility::GetRoot()
{
  if (!mWindow) {
    return nullptr;
  }

  return static_cast<RootAccessibleWrap*>(mWindow->GetRootAccessible());
}

void
SessionAccessibility::SetText(int32_t aID, jni::String::Param aText)
{
  if (RootAccessibleWrap* rootAcc = GetRoot()) {
    AccessibleWrap* acc = rootAcc->FindAccessibleById(aID);
    if (!acc) {
      return;
    }

    acc->SetTextContents(aText->ToString());
  }
}
