/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteDragStartData_h
#define mozilla_dom_RemoteDragStartData_h

#include "nsCOMPtr.h"
#include "nsRect.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"

class nsICookieJarSettings;

namespace mozilla::dom {

class IPCDataTransferItem;
class BrowserParent;

/**
 * This class is used to hold information about a drag
 * when a drag begins in a content process.
 */
class RemoteDragStartData {
 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteDragStartData)

  RemoteDragStartData(BrowserParent* aBrowserParent,
                      nsTArray<IPCTransferableData>&& aTransferableData,
                      const LayoutDeviceIntRect& aRect,
                      nsIPrincipal* aPrincipal, nsIContentSecurityPolicy* aCsp,
                      nsICookieJarSettings* aCookieJarSettings,
                      WindowContext* aSourceWindowContext,
                      WindowContext* aSourceTopWindowContext);

  void SetVisualization(
      already_AddRefed<gfx::DataSourceSurface> aVisualization) {
    mVisualization = aVisualization;
  }

  // Get the drag image and rectangle, clearing it from this
  // RemoteDragStartData in the process.
  already_AddRefed<mozilla::gfx::SourceSurface> TakeVisualization(
      LayoutDeviceIntRect* aRect) {
    *aRect = mRect;
    return mVisualization.forget();
  }

  void AddInitialDnDDataTo(DataTransfer* aDataTransfer,
                           nsIPrincipal** aPrincipal,
                           nsIContentSecurityPolicy** aCsp,
                           nsICookieJarSettings** aCookieJarSettings);

  WindowContext* GetSourceWindowContext() { return mSourceWindowContext; }
  WindowContext* GetSourceTopWindowContext() { return mSourceTopWindowContext; }

 private:
  virtual ~RemoteDragStartData();

  RefPtr<BrowserParent> mBrowserParent;
  nsTArray<IPCTransferableData> mTransferableData;
  const LayoutDeviceIntRect mRect;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIContentSecurityPolicy> mCsp;
  nsCOMPtr<nsICookieJarSettings> mCookieJarSettings;
  RefPtr<WindowContext> mSourceWindowContext;
  RefPtr<WindowContext> mSourceTopWindowContext;
  RefPtr<mozilla::gfx::SourceSurface> mVisualization;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_RemoteDragStartData_h
