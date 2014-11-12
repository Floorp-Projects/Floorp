/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/plugins/PluginWidgetChild.h"
#include "PluginWidgetProxy.h"

namespace mozilla {
namespace plugins {

PluginWidgetChild::PluginWidgetChild() :
  mWidget(nullptr)
{
  MOZ_COUNT_CTOR(PluginWidgetChild);
}

PluginWidgetChild::~PluginWidgetChild()
{
  MOZ_COUNT_DTOR(PluginWidgetChild);
}

void
PluginWidgetChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mWidget) {
    mWidget->ChannelDestroyed();
  }
  mWidget = nullptr;
}

} // namespace plugins
} // namespace mozilla



