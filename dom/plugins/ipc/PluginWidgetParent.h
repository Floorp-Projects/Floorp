/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginWidgetParent_h
#define mozilla_plugins_PluginWidgetParent_h

#include "mozilla/plugins/PPluginWidgetParent.h"
#include "nsIWidget.h"
#include "nsCOMPtr.h"

#if defined(MOZ_WIDGET_GTK)
class nsPluginNativeWindowGtk;
#endif

namespace mozilla {

namespace dom {
class TabParent;
}

namespace plugins {

class PluginWidgetParent : public PPluginWidgetParent
{
public:
  /**
   * Windows helper for firing off an update window request to a plugin.
   *
   * aWidget - the eWindowType_plugin_ipc_chrome widget associated with
   *           this plugin window.
   */
  static void SendAsyncUpdate(nsIWidget* aWidget);

  PluginWidgetParent();
  virtual ~PluginWidgetParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool RecvCreate(nsresult* aResult) MOZ_OVERRIDE;
  virtual bool RecvDestroy() MOZ_OVERRIDE;
  virtual bool RecvSetFocus(const bool& aRaise) MOZ_OVERRIDE;
  virtual bool RecvGetNativePluginPort(uintptr_t* value) MOZ_OVERRIDE;

  // Helper for compositor checks on the channel
  bool ActorDestroyed() { return !mWidget; }

  // Called by PBrowser when it receives a Destroy() call from the child.
  void ParentDestroy();

  // Sets mWidget's parent
  void SetParent(nsIWidget* aParent);

private:
  // The tab our connection is associated with.
  mozilla::dom::TabParent* GetTabParent();

public:
  // Identifies the side of the connection that initiates shutdown.
  enum ShutdownType {
    TAB_CLOSURE = 1,
    CONTENT     = 2
  };

private:
  void Shutdown(ShutdownType aType);

  // The chrome side native widget.
  nsCOMPtr<nsIWidget> mWidget;
#if defined(MOZ_WIDGET_GTK)
  nsAutoPtr<nsPluginNativeWindowGtk> mWrapper;
#endif
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_PluginWidgetParent_h

