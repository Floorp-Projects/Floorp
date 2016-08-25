/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginWidgetParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/ContentParent.h"
#include "nsComponentManagerUtils.h"
#include "nsWidgetsCID.h"

#include "mozilla/Unused.h"
#include "mozilla/DebugOnly.h"
#include "nsDebug.h"

#if defined(MOZ_WIDGET_GTK)
#include "nsPluginNativeWindowGtk.h"
#endif

using namespace mozilla;
using namespace mozilla::widget;

#define PWLOG(...)
//#define PWLOG(...) printf_stderr(__VA_ARGS__)

#if defined(XP_WIN)
namespace mozilla {
namespace dom {
// For nsWindow
const wchar_t* kPluginWidgetContentParentProperty =
  L"kPluginWidgetParentProperty";
} }
#endif

namespace mozilla {
namespace plugins {

static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);

// This macro returns true to prevent an abort in the child process when
// ipc message delivery fails.
#define ENSURE_CHANNEL {                                      \
  if (!mWidget) {                                             \
    NS_WARNING("called on an invalid remote widget.");        \
    return true;                                              \
  }                                                           \
}

PluginWidgetParent::PluginWidgetParent()
{
  PWLOG("PluginWidgetParent::PluginWidgetParent()\n");
  MOZ_COUNT_CTOR(PluginWidgetParent);
}

PluginWidgetParent::~PluginWidgetParent()
{
  PWLOG("PluginWidgetParent::~PluginWidgetParent()\n");
  MOZ_COUNT_DTOR(PluginWidgetParent);
  // A destroy call can actually get skipped if a widget is associated
  // with the last out-of-process page, make sure and cleanup any left
  // over widgets if we have them.
  KillWidget();
}

mozilla::dom::TabParent*
PluginWidgetParent::GetTabParent()
{
  return static_cast<mozilla::dom::TabParent*>(Manager());
}

void
PluginWidgetParent::SetParent(nsIWidget* aParent)
{
  // This will trigger sync send messages to the plugin process window
  // procedure and a cascade of events to that window related to focus
  // and activation.
  if (mWidget && aParent) {
    mWidget->SetParent(aParent);
  }
}

// When plugins run in chrome, nsPluginNativeWindow(Plat) implements platform
// specific functionality that wraps plugin widgets. With e10s we currently
// bypass this code on Window, and reuse a bit of it on Linux. Content still
// makes use of some of the utility functions as well.

bool
PluginWidgetParent::RecvCreate(nsresult* aResult)
{
  PWLOG("PluginWidgetParent::RecvCreate()\n");

  mWidget = do_CreateInstance(kWidgetCID, aResult);
  NS_ASSERTION(NS_SUCCEEDED(*aResult), "widget create failure");

#if defined(MOZ_WIDGET_GTK)
  // We need this currently just for GTK in setting up a socket widget
  // we can send over to content -> plugin.
  PLUG_NewPluginNativeWindow((nsPluginNativeWindow**)&mWrapper);
  if (!mWrapper) {
    KillWidget();
    return false;
  }
  // Give a copy of this to the widget, which handles some update
  // work for us.
  mWidget->SetNativeData(NS_NATIVE_PLUGIN_OBJECT_PTR, (uintptr_t)mWrapper.get());
#endif

  // This returns the top level window widget
  nsCOMPtr<nsIWidget> parentWidget = GetTabParent()->GetWidget();
  // If this fails, bail.
  if (!parentWidget) {
    *aResult = NS_ERROR_NOT_AVAILABLE;
    KillWidget();
    return true;
  }

  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_plugin_ipc_chrome;
  initData.mUnicode = false;
  initData.clipChildren = true;
  initData.clipSiblings = true;
  *aResult = mWidget->Create(parentWidget.get(), nullptr,
                             LayoutDeviceIntRect(0, 0, 0, 0), &initData);
  if (NS_FAILED(*aResult)) {
    KillWidget();
    // This should never fail, abort.
    return false;
  }

  mWidget->EnableDragDrop(true);

#if defined(MOZ_WIDGET_GTK)
  // For setup, initially GTK code expects 'window' to hold the parent.
  mWrapper->window = mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
  DebugOnly<nsresult> drv = mWrapper->CreateXEmbedWindow(false);
  NS_ASSERTION(NS_SUCCEEDED(drv), "widget call failure");
  mWrapper->SetAllocation();
  PWLOG("Plugin XID=%p\n", (void*)mWrapper->window);
#elif defined(XP_WIN)
  DebugOnly<DWORD> winres =
    ::SetPropW((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW),
               mozilla::dom::kPluginWidgetContentParentProperty,
               GetTabParent()->Manager()->AsContentParent());
  NS_ASSERTION(winres, "SetPropW call failure");

  uint64_t scrollCaptureId = mWidget->CreateScrollCaptureContainer();
  uintptr_t pluginId =
    reinterpret_cast<uintptr_t>(mWidget->GetNativeData(NS_NATIVE_PLUGIN_ID));
  Unused << SendSetScrollCaptureId(scrollCaptureId, pluginId);
#endif

  // This is a special call we make to nsBaseWidget to register this
  // window as a remote plugin window which is expected to receive
  // visibility updates from the compositor, which ships this data
  // over with corresponding layer updates.
  mWidget->RegisterPluginWindowForRemoteUpdates();

  return true;
}

void
PluginWidgetParent::KillWidget()
{
  PWLOG("PluginWidgetParent::KillWidget() widget=%p\n", (void*)mWidget.get());
  if (mWidget) {
    mWidget->UnregisterPluginWindowForRemoteUpdates();
    mWidget->Destroy();
#if defined(MOZ_WIDGET_GTK)
    mWidget->SetNativeData(NS_NATIVE_PLUGIN_OBJECT_PTR, (uintptr_t)0);
    mWrapper = nullptr;
#elif defined(XP_WIN)
    ::RemovePropW((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW),
                  mozilla::dom::kPluginWidgetContentParentProperty);
#endif
    mWidget = nullptr;
  }
}

void
PluginWidgetParent::ActorDestroy(ActorDestroyReason aWhy)
{
  PWLOG("PluginWidgetParent::ActorDestroy(%d)\n", aWhy);
  KillWidget();
}

// Called by TabParent's Destroy() in response to an early tear down (Early
// in that this is happening before layout in the child has had a chance
// to destroy the child widget.) when the tab is closing.
void
PluginWidgetParent::ParentDestroy()
{
  PWLOG("PluginWidgetParent::ParentDestroy()\n");
}

bool
PluginWidgetParent::RecvSetFocus(const bool& aRaise)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvSetFocus(%d)\n", aRaise);
  mWidget->SetFocus(aRaise);
  return true;
}

bool
PluginWidgetParent::RecvGetNativePluginPort(uintptr_t* value)
{
  ENSURE_CHANNEL;
#if defined(MOZ_WIDGET_GTK)
  *value = (uintptr_t)mWrapper->window;
  NS_ASSERTION(*value, "no xid??");
#else
  *value = (uintptr_t)mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
  NS_ASSERTION(*value, "no native port??");
#endif
  PWLOG("PluginWidgetParent::RecvGetNativeData() %p\n", (void*)*value);
  return true;
}

bool
PluginWidgetParent::RecvSetNativeChildWindow(const uintptr_t& aChildWindow)
{
#if defined(XP_WIN)
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvSetNativeChildWindow(%p)\n",
        (void*)aChildWindow);
  mWidget->SetNativeData(NS_NATIVE_CHILD_WINDOW, aChildWindow);
  return true;
#else
  NS_NOTREACHED("PluginWidgetParent::RecvSetNativeChildWindow not implemented!");
  return false;
#endif
}

} // namespace plugins
} // namespace mozilla
