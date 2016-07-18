/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginWidgetChild_h
#define mozilla_plugins_PluginWidgetChild_h

#include "mozilla/plugins/PPluginWidgetChild.h"

namespace mozilla {
namespace widget {
class PluginWidgetProxy;
} // namespace widget
namespace plugins {

class PluginWidgetChild : public PPluginWidgetChild
{
public:
  PluginWidgetChild();
  virtual ~PluginWidgetChild();

  bool RecvSetScrollCaptureId(const uint64_t& aScrollCaptureId,
                              const uintptr_t& aPluginInstanceId) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  void SetWidget(mozilla::widget::PluginWidgetProxy* aWidget) {
    mWidget = aWidget;
  }
  void ProxyShutdown();

private:
  void KillWidget();

  mozilla::widget::PluginWidgetProxy* mWidget;
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_PluginWidgetChild_h

