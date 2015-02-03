/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/plugins/PluginWidgetChild.h"
#include "PluginWidgetProxy.h"
#include "mozilla/DebugOnly.h"
#include "nsDebug.h"

#if defined(XP_WIN)
#include "mozilla/plugins/PluginInstanceParent.h"
using mozilla::plugins::PluginInstanceParent;
#endif

#define PWLOG(...)
//#define PWLOG(...) printf_stderr(__VA_ARGS__)

namespace mozilla {
namespace plugins {

PluginWidgetChild::PluginWidgetChild() :
  mWidget(nullptr)
{
  MOZ_COUNT_CTOR(PluginWidgetChild);
}

PluginWidgetChild::~PluginWidgetChild()
{
  PWLOG("PluginWidgetChild::~PluginWidgetChild()\n");
  MOZ_COUNT_DTOR(PluginWidgetChild);
}

/*
 * Tear down scenarios
 * layout (plugin content unloading):
 *  - PluginWidgetProxy::Destroy(), calls PluginWidgetChild->SendDestroy(), (proxy disabled)
 *  - PluginWidgetParent::RecvDestroy(), sends async ParentShutdown()
 *  - PluginWidgetChild::RecvParentShutdown(), calls Send__delete__()
 *  - PluginWidgetParent::ActorDestroy() called in response to __delete__.
 * PBrowser teardown (tab closing):
 *  - TabParent::Destroy()
 *  - PluginWidgetParent::ParentDestroy(), sends async ParentShutdown()
 *  - PluginWidgetChild::RecvParentShutdown(), (proxy disabled)
 *  - PluginWidgetParent::ActorDestroy()
 *  - PluginWidgetParent::~PluginWidgetParent() in response to PBrowserParent::DeallocSubtree()
 *  - PluginWidgetChild::ActorDestroy() from PPluginWidgetChild::DestroySubtree
 *  - ~PluginWidgetChild() in response to PBrowserChild::DeallocSubtree()
 **/

void
PluginWidgetChild::ShutdownProxy()
{
  if (mWidget) {
    mWidget->ChannelDestroyed();
  }
  mWidget = nullptr;
}

void
PluginWidgetChild::ActorDestroy(ActorDestroyReason aWhy)
{
  PWLOG("PluginWidgetChild::ActorDestroy(%d)\n", aWhy);
  ShutdownProxy(); // backup
}

bool
PluginWidgetChild::RecvParentShutdown(const bool& aParentInitiated)
{
  PWLOG("PluginWidgetChild::RecvParentShutdown(%d)\n", aParentInitiated);
  ShutdownProxy();
  if (!aParentInitiated) {
    Send__delete__(this);
  }
  return true;
}

bool
PluginWidgetChild::RecvUpdateWindow(const uintptr_t& aChildId)
{
#if defined(XP_WIN)
  NS_ASSERTION(aChildId, "Expected child hwnd value for remote plugin instance.");
  PluginInstanceParent* parentInstance =
    PluginInstanceParent::LookupPluginInstanceByID(aChildId);
  NS_ASSERTION(parentInstance, "Expected matching plugin instance");
  if (parentInstance) {
    // sync! update call to the plugin instance that forces the
    // plugin to paint its child window.
    parentInstance->CallUpdateWindow();
  }
  return true;
#else
  NS_NOTREACHED("PluginWidgetChild::RecvUpdateWindow calls unexpected on this platform.");
  return false;
#endif
}

} // namespace plugins
} // namespace mozilla
