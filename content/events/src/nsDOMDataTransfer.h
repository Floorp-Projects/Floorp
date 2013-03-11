/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDataTransfer_h__
#define nsDOMDataTransfer_h__

#include "nsString.h"
#include "nsTArray.h"
#include "nsIVariant.h"
#include "nsIPrincipal.h"
#include "nsIDOMDataTransfer.h"
#include "nsIDragService.h"
#include "nsIDOMElement.h"
#include "nsCycleCollectionParticipant.h"

#include "nsAutoPtr.h"
#include "nsIFile.h"
#include "nsDOMFile.h"
#include "mozilla/Attributes.h"

class nsITransferable;

/**
 * TransferItem is used to hold data for a particular format. Each piece of
 * data has a principal set from the caller which added it. This allows a
 * caller that wishes to retrieve the data to only be able to access the data
 * it is allowed to, yet still allow a chrome caller to retrieve any of the
 * data.
 */
struct TransferItem {
  nsString mFormat;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIVariant> mData;
};

class nsDOMDataTransfer MOZ_FINAL : public nsIDOMDataTransfer
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMDATATRANSFER

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMDataTransfer, nsIDOMDataTransfer)

  friend class nsEventStateManager;

protected:

  // hide the default constructor
  nsDOMDataTransfer();

  // this constructor is used only by the Clone method to copy the fields as
  // needed to a new data transfer.
  nsDOMDataTransfer(uint32_t aEventType,
                    const uint32_t aEffectAllowed,
                    bool aCursorState,
                    bool aIsExternal,
                    bool aUserCancelled,
                    bool aIsCrossDomainSubFrameDrop,
                    nsTArray<nsTArray<TransferItem> >& aItems,
                    nsIDOMElement* aDragImage,
                    uint32_t aDragImageX,
                    uint32_t aDragImageY);

  ~nsDOMDataTransfer()
  {
    if (mFiles) {
      mFiles->Disconnect();
    }
  }

  static const char sEffects[8][9];

public:

  // Constructor for nsDOMDataTransfer.
  //
  // aEventType is an event constant (such as NS_DRAGDROP_START)
  //
  // aIsExternal must only be true when used to create a dataTransfer for a
  // paste or a drag that was started without using a data transfer. The
  // latter will occur when an external drag occurs, that is, a drag where the
  // source is another application, or a drag is started by calling the drag
  // service directly.
  nsDOMDataTransfer(uint32_t aEventType, bool aIsExternal);

  void GetDragTarget(nsIDOMElement** aDragTarget)
  {
    *aDragTarget = mDragTarget;
    NS_IF_ADDREF(*aDragTarget);
  }

  // a readonly dataTransfer cannot have new data added or existing data removed.
  // Only the dropEffect and effectAllowed may be modified.
  void SetReadOnly() { mReadOnly = true; }

  // converts the data into an array of nsITransferable objects to be used for
  // drag and drop or clipboard operations.
  already_AddRefed<nsISupportsArray> GetTransferables(nsIDOMNode* aDragTarget);

  // converts the data for a single item at aIndex into an nsITransferable object.
  already_AddRefed<nsITransferable> GetTransferable(uint32_t aIndex,
                                                    nsILoadContext* aLoadContext);

  // converts the data in the variant to an nsISupportString if possible or
  // an nsISupports or null otherwise.
  bool ConvertFromVariant(nsIVariant* aVariant,
                            nsISupports** aSupports,
                            uint32_t* aLength);

  // clears all of the data
  void ClearAll();

  // Similar to SetData except also specifies the principal to store.
  // aData may be null when called from CacheExternalDragFormats or
  // CacheExternalClipboardFormats.
  nsresult SetDataWithPrincipal(const nsAString& aFormat,
                                nsIVariant* aData,
                                uint32_t aIndex,
                                nsIPrincipal* aPrincipal);

protected:

  // returns a weak reference to the drag image
  nsIDOMElement* GetDragImage(int32_t* aX, int32_t* aY)
  {
    *aX = mDragImageX;
    *aY = mDragImageY;
    return mDragImage;
  }

  // returns a weak reference to the current principal
  nsIPrincipal* GetCurrentPrincipal(nsresult* rv);

  // converts some formats used for compatibility in aInFormat into aOutFormat.
  // Text and text/unicode become text/plain, and URL becomes text/uri-list
  void GetRealFormat(const nsAString& aInFormat, nsAString& aOutFormat);

  // caches text and uri-list data formats that exist in the drag service or
  // clipboard for retrieval later.
  void CacheExternalData(const char* aFormat, uint32_t aIndex, nsIPrincipal* aPrincipal);

  // caches the formats that exist in the drag service that were added by an
  // external drag
  void CacheExternalDragFormats();

  // caches the formats that exist in the clipboard
  void CacheExternalClipboardFormats();

  // fills in the data field of aItem with the data from the drag service or
  // clipboard for a given index.
  void FillInExternalData(TransferItem& aItem, uint32_t aIndex);

  // the event type this data transfer is for. This will correspond to an
  // event->message value.
  uint32_t mEventType;

  // the drop effect and effect allowed
  uint32_t mDropEffect;
  uint32_t mEffectAllowed;

  // Indicates the behavior of the cursor during drag operations
  bool mCursorState;

  // readonly data transfers may not be modified except the drop effect and
  // effect allowed.
  bool mReadOnly;

  // true for drags started without a data transfer, for example, those from
  // another application.
  bool mIsExternal;

  // true if the user cancelled the drag. Used only for the dragend event.
  bool mUserCancelled;

  // true if this is a cross-domain drop from a subframe where access to the
  // data should be prevented
  bool mIsCrossDomainSubFrameDrop;

  // array of items, each containing an array of format->data pairs
  nsTArray<nsTArray<TransferItem> > mItems;

  // array of files, containing only the files present in the dataTransfer
  nsRefPtr<nsDOMFileList> mFiles;

  // the target of the drag. The drag and dragend events will fire at this.
  nsCOMPtr<nsIDOMElement> mDragTarget;

  // the custom drag image and coordinates within the image. If mDragImage is
  // null, the default image is created from the drag target.
  nsCOMPtr<nsIDOMElement> mDragImage;
  uint32_t mDragImageX;
  uint32_t mDragImageY;
};

#endif // nsDOMDataTransfer_h__

