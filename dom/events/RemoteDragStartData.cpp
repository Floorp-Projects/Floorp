/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentAreaDragDrop.h"
#include "RemoteDragStartData.h"
#include "nsVariant.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/DOMTypes.h"
#include "ProtocolUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

RemoteDragStartData::~RemoteDragStartData() {}

RemoteDragStartData::RemoteDragStartData(
    BrowserParent* aBrowserParent, nsTArray<IPCDataTransfer>&& aDataTransfer,
    const LayoutDeviceIntRect& aRect, nsIPrincipal* aPrincipal)
    : mBrowserParent(aBrowserParent),
      mDataTransfer(aDataTransfer),
      mRect(aRect),
      mPrincipal(aPrincipal) {}

void RemoteDragStartData::AddInitialDnDDataTo(DataTransfer* aDataTransfer,
                                              nsIPrincipal** aPrincipal) {
  NS_IF_ADDREF(*aPrincipal = mPrincipal);

  for (uint32_t i = 0; i < mDataTransfer.Length(); ++i) {
    nsTArray<IPCDataTransferItem>& itemArray = mDataTransfer[i].items();
    for (auto& item : itemArray) {
      RefPtr<nsVariantCC> variant = new nsVariantCC();
      // Special case kFilePromiseMime so that we get the right
      // nsIFlavorDataProvider for it.
      if (item.flavor().EqualsLiteral(kFilePromiseMime)) {
        RefPtr<nsISupports> flavorDataProvider =
            new nsContentAreaDragDropDataProvider();
        variant->SetAsISupports(flavorDataProvider);
      } else if (item.data().type() == IPCDataTransferData::TnsString) {
        variant->SetAsAString(item.data().get_nsString());
      } else if (item.data().type() == IPCDataTransferData::TIPCBlob) {
        RefPtr<BlobImpl> impl =
            IPCBlobUtils::Deserialize(item.data().get_IPCBlob());
        variant->SetAsISupports(impl);
      } else if (item.data().type() == IPCDataTransferData::TShmem) {
        if (nsContentUtils::IsFlavorImage(item.flavor())) {
          // An image! Get the imgIContainer for it and set it in the variant.
          nsCOMPtr<imgIContainer> imageContainer;
          nsresult rv = nsContentUtils::DataTransferItemToImage(
              item, getter_AddRefs(imageContainer));
          if (NS_FAILED(rv)) {
            continue;
          }
          variant->SetAsISupports(imageContainer);
        } else {
          Shmem data = item.data().get_Shmem();
          variant->SetAsACString(
              nsDependentCSubstring(data.get<char>(), data.Size<char>()));
        }

        mozilla::Unused << mBrowserParent->DeallocShmem(
            item.data().get_Shmem());
      }

      // We set aHidden to false, as we don't need to worry about hiding data
      // from content in the parent process where there is no content.
      aDataTransfer->SetDataWithPrincipalFromOtherProcess(
          NS_ConvertUTF8toUTF16(item.flavor()), variant, i, mPrincipal,
          /* aHidden = */ false);
    }
  }

  // Clear things that are no longer needed.
  mDataTransfer.Clear();
  mPrincipal = nullptr;
}

}  // namespace dom
}  // namespace mozilla
