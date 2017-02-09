/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginWidgetChild_h
#define mozilla_plugins_PluginWidgetChild_h

#ifndef XP_WIN
#error "This header should be Windows-only."
#endif

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

