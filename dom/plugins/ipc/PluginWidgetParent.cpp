/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginWidgetParent.h"
#include "mozilla/dom/TabParent.h"
#include "nsComponentManagerUtils.h"
#include "nsWidgetsCID.h"
#include "nsDebug.h"

using namespace mozilla::widget;

#define PWLOG(...)
// #define PWLOG(...) printf_stderr(__VA_ARGS__)

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
  if (mWidget) {
    mWidget->Destroy();
    mWidget = nullptr;
  }
}

mozilla::dom::TabParent*
PluginWidgetParent::GetTabParent()
{
  return static_cast<mozilla::dom::TabParent*>(Manager());
}

void
PluginWidgetParent::ActorDestroy(ActorDestroyReason aWhy)
{
  PWLOG("PluginWidgetParent::ActorDestroy()\n");
}

// When plugins run in chrome, nsPluginNativeWindow(Plat) implements platform
// specific functionality that wraps plugin widgets. With e10s we currently
// bypass this code since we can't connect up platform specific bits in the
// content process. We may need to instantiate nsPluginNativeWindow here and
// enable some of its logic.

bool
PluginWidgetParent::RecvCreate()
{
  PWLOG("PluginWidgetParent::RecvCreate()\n");

  nsresult rv;

  mWidget = do_CreateInstance(kWidgetCID, &rv);

  // This returns the top level window widget
  nsCOMPtr<nsIWidget> parentWidget = GetTabParent()->GetWidget();

  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_plugin_ipc_chrome;
  initData.mUnicode = false;
  initData.clipChildren = true;
  initData.clipSiblings = true;
  rv = mWidget->Create(parentWidget.get(), nullptr, nsIntRect(0,0,0,0),
                       nullptr, &initData);
  if (NS_FAILED(rv)) {
    mWidget->Destroy();
    mWidget = nullptr;
    return false;
  }

  mWidget->EnableDragDrop(true);
  mWidget->Show(false);
  mWidget->Enable(false);

  // Force the initial position down into content. If we miss an
  // initial position update this insures the widget doesn't overlap
  // chrome.
  RecvMove(0, 0);

  return true;
}

bool
PluginWidgetParent::RecvDestroy()
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvDestroy()\n");
  mWidget->Destroy();
  mWidget = nullptr;
  return true;
}

bool
PluginWidgetParent::RecvShow(const bool& aState)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvShow(%d)\n", aState);
  mWidget->Show(aState);
  return true;
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
PluginWidgetParent::RecvInvalidate(const nsIntRect& aRect)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvInvalidate(%d, %d, %d, %d)\n", aRect.x, aRect.y, aRect.width, aRect.height);
  mWidget->Invalidate(aRect);
  return true;
}

bool
PluginWidgetParent::RecvGetNativePluginPort(uintptr_t* value)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvGetNativeData()\n");
  *value = (uintptr_t)mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
  return true;
}

bool
PluginWidgetParent::RecvResize(const nsIntRect& aRect)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvResize(%d, %d, %d, %d)\n", aRect.x, aRect.y, aRect.width, aRect.height);
  mWidget->Resize(aRect.width, aRect.height, true);
  return true;
}

bool
PluginWidgetParent::RecvMove(const double& aX, const double& aY)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvMove(%f, %f)\n", aX, aY);


  // This returns the top level window
  nsCOMPtr<nsIWidget> widget = GetTabParent()->GetWidget();
  if (!widget) {
    // return true otherwise ipc will abort the content process, crashing
    // all tabs.
    return true;
  }

  // Passed in coords are at the tab origin, adjust to the main window.
  nsIntPoint offset = GetTabParent()->GetChildProcessOffset();
  offset.x = abs(offset.x);
  offset.y = abs(offset.y);
  offset += nsIntPoint(ceil(aX), ceil(aY));
  mWidget->Move(offset.x, offset.y);

  return true;
}

bool
PluginWidgetParent::RecvSetWindowClipRegion(const nsTArray<nsIntRect>& Regions,
                                            const bool& aIntersectWithExisting)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetParent::RecvSetWindowClipRegion()\n");
  mWidget->SetWindowClipRegion(Regions, aIntersectWithExisting);
  return true;
}

} // namespace plugins
} // namespace mozilla
