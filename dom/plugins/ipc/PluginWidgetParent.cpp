/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginWidgetParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/ContentParent.h"
#include "nsComponentManagerUtils.h"
#include "nsWidgetsCID.h"

#include "mozilla/Unused.h"
#include "mozilla/DebugOnly.h"
#include "nsDebug.h"

using namespace mozilla;
using namespace mozilla::widget;

#define PWLOG(...)
//#define PWLOG(...) printf_stderr(__VA_ARGS__)

namespace mozilla {
namespace dom {
// For nsWindow
const wchar_t* kPluginWidgetContentParentProperty =
    L"kPluginWidgetParentProperty";
}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace plugins {

// This macro returns IPC_OK() to prevent an abort in the child process when
// ipc message delivery fails.
#define ENSURE_CHANNEL                                   \
  {                                                      \
    if (!mWidget) {                                      \
      NS_WARNING("called on an invalid remote widget."); \
      return IPC_OK();                                   \
    }                                                    \
  }

PluginWidgetParent::PluginWidgetParent() {
  PWLOG("PluginWidgetParent::PluginWidgetParent()\n");
  MOZ_COUNT_CTOR(PluginWidgetParent);
}

PluginWidgetParent::~PluginWidgetParent() {
  PWLOG("PluginWidgetParent::~PluginWidgetParent()\n");
  MOZ_COUNT_DTOR(PluginWidgetParent);
  // A destroy call can actually get skipped if a widget is associated
  // with the last out-of-process page, make sure and cleanup any left
  // over widgets if we have them.
  KillWidget();
}

mozilla::dom::BrowserParent* PluginWidgetParent::GetBrowserParent() {
  return static_cast<mozilla::dom::BrowserParent*>(Manager());
}

void PluginWidgetParent::SetParent(nsIWidget* aParent) {
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

mozilla::ipc::IPCResult PluginWidgetParent::RecvCreate(
    nsresult* aResult, uint64_t* aScrollCaptureId,
    uintptr_t* aPluginInstanceId) {
  PWLOG("PluginWidgetParent::RecvCreate()\n");

  *aScrollCaptureId = 0;
  *aPluginInstanceId = 0;

  mWidget = nsIWidget::CreateChildWindow();
  *aResult = mWidget ? NS_OK : NS_ERROR_FAILURE;

  // This returns the top level window widget
  nsCOMPtr<nsIWidget> parentWidget = GetBrowserParent()->GetWidget();
  // If this fails, bail.
  if (!parentWidget) {
    *aResult = NS_ERROR_NOT_AVAILABLE;
    KillWidget();
    return IPC_OK();
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
    return IPC_FAIL_NO_REASON(this);
  }

  mWidget->EnableDragDrop(true);

  // This is a special call we make to nsBaseWidget to register this
  // window as a remote plugin window which is expected to receive
  // visibility updates from the compositor, which ships this data
  // over with corresponding layer updates.
  mWidget->RegisterPluginWindowForRemoteUpdates();

  return IPC_OK();
}

void PluginWidgetParent::KillWidget() {
  PWLOG("PluginWidgetParent::KillWidget() widget=%p\n", (void*)mWidget.get());
  if (mWidget) {
    mWidget->UnregisterPluginWindowForRemoteUpdates();
    mWidget->Destroy();
    ::RemovePropW((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW),
                  mozilla::dom::kPluginWidgetContentParentProperty);
    mWidget = nullptr;
  }
}

void PluginWidgetParent::ActorDestroy(ActorDestroyReason aWhy) {
  PWLOG("PluginWidgetParent::ActorDestroy(%d)\n", aWhy);
  KillWidget();
}

// Called by BrowserParent's Destroy() in response to an early tear down (Early
// in that this is happening before layout in the child has had a chance
// to destroy the child widget.) when the tab is closing.
void PluginWidgetParent::ParentDestroy() {
  PWLOG("PluginWidgetParent::ParentDestroy()\n");
}

mozilla::ipc::IPCResult PluginWidgetParent::RecvSetFocus(const bool& aRaise) {
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvSetFocus(%d)\n", aRaise);
  mWidget->SetFocus(aRaise);
  return IPC_OK();
}

mozilla::ipc::IPCResult PluginWidgetParent::RecvGetNativePluginPort(
    uintptr_t* value) {
  ENSURE_CHANNEL;
  *value = (uintptr_t)mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
  NS_ASSERTION(*value, "no native port??");
  PWLOG("PluginWidgetParent::RecvGetNativeData() %p\n", (void*)*value);
  return IPC_OK();
}

mozilla::ipc::IPCResult PluginWidgetParent::RecvSetNativeChildWindow(
    const uintptr_t& aChildWindow) {
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvSetNativeChildWindow(%p)\n",
        (void*)aChildWindow);
  mWidget->SetNativeData(NS_NATIVE_CHILD_WINDOW, aChildWindow);
  return IPC_OK();
}

}  // namespace plugins
}  // namespace mozilla
