/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DataTransfer_h
#define mozilla_dom_DataTransfer_h

#include "nsString.h"
#include "nsTArray.h"
#include "nsIVariant.h"
#include "nsIPrincipal.h"
#include "nsIDOMDataTransfer.h"
#include "nsIDOMElement.h"
#include "nsIDragService.h"
#include "nsCycleCollectionParticipant.h"

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/File.h"

class nsINode;
class nsITransferable;
class nsISupportsArray;
class nsILoadContext;

namespace mozilla {

class EventStateManager;

namespace dom {

class DataTransferItem;
class DataTransferItemList;
class DOMStringList;
class Element;
class FileList;
class Promise;
template<typename T> class Optional;

#define NS_DATATRANSFER_IID \
{ 0x6c5f90d1, 0xa886, 0x42c8, \
  { 0x85, 0x06, 0x10, 0xbe, 0x5c, 0x0d, 0xc6, 0x77 } }

class DataTransfer final : public nsIDOMDataTransfer,
                           public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DATATRANSFER_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMDATATRANSFER

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DataTransfer)

  friend class mozilla::EventStateManager;

  static DataTransfer* Cast(nsIDOMDataTransfer* aArg)
  {
    return static_cast<DataTransfer*>(aArg);
  }

protected:

  // hide the default constructor
  DataTransfer();

  // this constructor is used only by the Clone method to copy the fields as
  // needed to a new data transfer.
  DataTransfer(nsISupports* aParent,
               EventMessage aEventMessage,
               const uint32_t aEffectAllowed,
               bool aCursorState,
               bool aIsExternal,
               bool aUserCancelled,
               bool aIsCrossDomainSubFrameDrop,
               int32_t aClipboardType,
               DataTransferItemList* aItems,
               Element* aDragImage,
               uint32_t aDragImageX,
               uint32_t aDragImageY);

  ~DataTransfer();

  static const char sEffects[8][9];

public:
  // Constructor for DataTransfer.
  //
  // aIsExternal must only be true when used to create a dataTransfer for a
  // paste or a drag that was started without using a data transfer. The
  // latter will occur when an external drag occurs, that is, a drag where the
  // source is another application, or a drag is started by calling the drag
  // service directly. For clipboard operations, aClipboardType indicates
  // which clipboard to use, from nsIClipboard, or -1 for non-clipboard
  // operations, or if access to the system clipboard should not be allowed.
  DataTransfer(nsISupports* aParent, EventMessage aEventMessage,
               bool aIsExternal, int32_t aClipboardType);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  void SetParentObject(nsISupports* aNewParent)
  {
    MOZ_ASSERT(aNewParent);
    // Setting the parent after we've been wrapped is pointless, so
    // make sure we aren't wrapped yet.
    MOZ_ASSERT(!GetWrapperPreserveColor());
    mParent = aNewParent;
  }

  static already_AddRefed<DataTransfer>
  Constructor(const GlobalObject& aGlobal, const nsAString& aEventType,
              bool aIsExternal, ErrorResult& aRv);

  void GetDropEffect(nsString& aDropEffect)
  {
    aDropEffect.AssignASCII(sEffects[mDropEffect]);
  }

  void GetEffectAllowed(nsString& aEffectAllowed)
  {
    if (mEffectAllowed == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED) {
      aEffectAllowed.AssignLiteral("uninitialized");
    } else {
      aEffectAllowed.AssignASCII(sEffects[mEffectAllowed]);
    }
  }

  void SetDragImage(Element& aElement, int32_t aX, int32_t aY,
                    ErrorResult& aRv);

  void GetTypes(nsTArray<nsString>& aTypes) const;

  void GetData(const nsAString& aFormat, nsAString& aData,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aRv);

  void SetData(const nsAString& aFormat, const nsAString& aData,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aRv);

  void ClearData(const mozilla::dom::Optional<nsAString>& aFormat,
                 nsIPrincipal& aSubjectPrincipal,
                 mozilla::ErrorResult& aRv);

  already_AddRefed<FileList>
  GetFiles(nsIPrincipal& aSubjectPrincipal,
           mozilla::ErrorResult& aRv);

  already_AddRefed<Promise>
  GetFilesAndDirectories(nsIPrincipal& aSubjectPrincipal,
                         mozilla::ErrorResult& aRv);

  already_AddRefed<Promise>
  GetFiles(bool aRecursiveFlag,
           nsIPrincipal& aSubjectPrincipal,
           ErrorResult& aRv);


  void AddElement(Element& aElement, mozilla::ErrorResult& aRv);

  uint32_t MozItemCount() const;

  void GetMozCursor(nsString& aCursor)
  {
    if (mCursorState) {
      aCursor.AssignLiteral("default");
    } else {
      aCursor.AssignLiteral("auto");
    }
  }

  already_AddRefed<DOMStringList> MozTypesAt(uint32_t aIndex,
                                             mozilla::ErrorResult& aRv) const;

  void MozClearDataAt(const nsAString& aFormat, uint32_t aIndex,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aRv);

  void MozSetDataAt(JSContext* aCx, const nsAString& aFormat,
                    JS::Handle<JS::Value> aData, uint32_t aIndex,
                    nsIPrincipal& aSubjectPrincipal,
                    mozilla::ErrorResult& aRv);

  void MozGetDataAt(JSContext* aCx, const nsAString& aFormat,
                    uint32_t aIndex, JS::MutableHandle<JS::Value> aRetval,
                    nsIPrincipal& aSubjectPrincipal,
                    mozilla::ErrorResult& aRv);

  bool MozUserCancelled() const
  {
    return mUserCancelled;
  }

  already_AddRefed<nsINode> GetMozSourceNode();

  mozilla::dom::Element* GetDragTarget() const
  {
    return mDragTarget;
  }

  nsresult GetDataAtNoSecurityCheck(const nsAString& aFormat, uint32_t aIndex,
                                    nsIVariant** aData);

  DataTransferItemList* Items() const {
    return mItems;
  }

  // a readonly dataTransfer cannot have new data added or existing data
  // removed. Only the dropEffect and effectAllowed may be modified.
  bool IsReadOnly() const {
    return mReadOnly;
  }
  void SetReadOnly() {
    mReadOnly = true;
  }

  int32_t ClipboardType() const {
    return mClipboardType;
  }
  EventMessage GetEventMessage() const {
    return mEventMessage;
  }
  bool IsCrossDomainSubFrameDrop() const {
    return mIsCrossDomainSubFrameDrop;
  }

  // converts the data into an array of nsITransferable objects to be used for
  // drag and drop or clipboard operations.
  already_AddRefed<nsISupportsArray> GetTransferables(nsIDOMNode* aDragTarget);

  already_AddRefed<nsISupportsArray>
  GetTransferables(nsILoadContext* aLoadContext);

  // converts the data for a single item at aIndex into an nsITransferable
  // object.
  already_AddRefed<nsITransferable>
  GetTransferable(uint32_t aIndex, nsILoadContext* aLoadContext);

  // converts the data in the variant to an nsISupportString if possible or
  // an nsISupports or null otherwise.
  bool ConvertFromVariant(nsIVariant* aVariant,
                          nsISupports** aSupports,
                          uint32_t* aLength) const;

  // clears all of the data
  void ClearAll();

  // Similar to SetData except also specifies the principal to store.
  // aData may be null when called from CacheExternalDragFormats or
  // CacheExternalClipboardFormats.
  nsresult SetDataWithPrincipal(const nsAString& aFormat,
                                nsIVariant* aData,
                                uint32_t aIndex,
                                nsIPrincipal* aPrincipal);

  // Variation of SetDataWithPrincipal with handles extracting
  // kCustomTypesMime data into separate types.
  void SetDataWithPrincipalFromOtherProcess(const nsAString& aFormat,
                                            nsIVariant* aData,
                                            uint32_t aIndex,
                                            nsIPrincipal* aPrincipal,
                                            bool aHidden);

  // returns a weak reference to the drag image
  Element* GetDragImage(int32_t* aX, int32_t* aY) const
  {
    *aX = mDragImageX;
    *aY = mDragImageY;
    return mDragImage;
  }

  nsresult Clone(nsISupports* aParent, EventMessage aEventMessage,
                 bool aUserCancelled, bool aIsCrossDomainSubFrameDrop,
                 DataTransfer** aResult);

  // converts some formats used for compatibility in aInFormat into aOutFormat.
  // Text and text/unicode become text/plain, and URL becomes text/uri-list
  void GetRealFormat(const nsAString& aInFormat, nsAString& aOutFormat) const;

  static bool PrincipalMaySetData(const nsAString& aFormat,
                                  nsIVariant* aData,
                                  nsIPrincipal* aPrincipal);

  // Notify the DataTransfer that the list returned from GetTypes may have
  // changed.  This can happen due to items we care about for purposes of
  // GetTypes being added or removed or changing item kinds.
  void TypesListMayHaveChanged();

protected:

  // caches text and uri-list data formats that exist in the drag service or
  // clipboard for retrieval later.
  nsresult CacheExternalData(const char* aFormat, uint32_t aIndex,
                             nsIPrincipal* aPrincipal, bool aHidden);

  // caches the formats that exist in the drag service that were added by an
  // external drag
  void CacheExternalDragFormats();

  // caches the formats that exist in the clipboard
  void CacheExternalClipboardFormats();

  FileList* GetFilesInternal(ErrorResult& aRv, nsIPrincipal* aSubjectPrincipal);
  nsresult GetDataAtInternal(const nsAString& aFormat, uint32_t aIndex,
                             nsIPrincipal* aSubjectPrincipal,
                             nsIVariant** aData);

  nsresult SetDataAtInternal(const nsAString& aFormat, nsIVariant* aData,
                             uint32_t aIndex, nsIPrincipal* aSubjectPrincipal);

  friend class ContentParent;

  void FillAllExternalData();

  void FillInExternalCustomTypes(uint32_t aIndex, nsIPrincipal* aPrincipal);

  void FillInExternalCustomTypes(nsIVariant* aData, uint32_t aIndex,
                                 nsIPrincipal* aPrincipal);

  void MozClearDataAtHelper(const nsAString& aFormat, uint32_t aIndex,
                            nsIPrincipal& aSubjectPrincipal,
                            mozilla::ErrorResult& aRv);

  nsCOMPtr<nsISupports> mParent;

  // the drop effect and effect allowed
  uint32_t mDropEffect;
  uint32_t mEffectAllowed;

  // the event message this data transfer is for. This will correspond to an
  // event->mMessage value.
  EventMessage mEventMessage;

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

  // Indicates which clipboard type to use for clipboard operations. Ignored for
  // drag and drop.
  int32_t mClipboardType;

  // The items contained with the DataTransfer
  RefPtr<DataTransferItemList> mItems;

  // the target of the drag. The drag and dragend events will fire at this.
  nsCOMPtr<mozilla::dom::Element> mDragTarget;

  // the custom drag image and coordinates within the image. If mDragImage is
  // null, the default image is created from the drag target.
  nsCOMPtr<mozilla::dom::Element> mDragImage;
  uint32_t mDragImageX;
  uint32_t mDragImageY;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DataTransfer, NS_DATATRANSFER_IID)

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_DataTransfer_h */
