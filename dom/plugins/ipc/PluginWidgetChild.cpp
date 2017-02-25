/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/plugins/PluginWidgetChild.h"

#include "mozilla/dom/TabChild.h"
#include "mozilla/plugins/PluginWidgetParent.h"
#include "PluginWidgetProxy.h"

#include "mozilla/Unused.h"
#include "mozilla/DebugOnly.h"
#include "nsDebug.h"

#include "mozilla/plugins/PluginInstanceParent.h"

#define PWLOG(...)
//#define PWLOG(...) printf_stderr(__VA_ARGS__)

namespace mozilla {
namespace plugins {

PluginWidgetChild::PluginWidgetChild() :
  mWidget(nullptr)
{
  PWLOG("PluginWidgetChild::PluginWidgetChild()\n");
  MOZ_COUNT_CTOR(PluginWidgetChild);
}

PluginWidgetChild::~PluginWidgetChild()
{
  PWLOG("PluginWidgetChild::~PluginWidgetChild()\n");
  MOZ_COUNT_DTOR(PluginWidgetChild);
}

// Called by the proxy widget when it is destroyed by layout. Only gets
// called once.
void
PluginWidgetChild::ProxyShutdown()
{
  PWLOG("PluginWidgetChild::ProxyShutdown()\n");
  if (mWidget) {
    mWidget = nullptr;
    auto tab = static_cast<mozilla::dom::TabChild*>(Manager());
    if (!tab->IsDestroyed()) {
      Unused << Send__delete__(this);
    }
  }
}

void
PluginWidgetChild::KillWidget()
{
  PWLOG("PluginWidgetChild::KillWidget()\n");
  if (mWidget) {
    mWidget->ChannelDestroyed();
  }
  mWidget = nullptr;
}

void
PluginWidgetChild::ActorDestroy(ActorDestroyReason aWhy)
{
  PWLOG("PluginWidgetChild::ActorDestroy(%d)\n", aWhy);
  KillWidget();
}

} // namespace plugins
} // namespace mozilla
