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
#include "nsIDragService.h"
#include "nsCycleCollectionParticipant.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/File.h"

class nsINode;
class nsITransferable;
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

class DataTransfer final : public nsISupports,
                           public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DATATRANSFER_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DataTransfer)

  friend class mozilla::EventStateManager;

  /// An enum which represents which "Drag Data Store Mode" the DataTransfer is
  /// in according to the spec.
  enum class Mode : uint8_t {
    ReadWrite,
    ReadOnly,
    Protected,
  };

protected:

  // hide the default constructor
  DataTransfer();

  // this constructor is used only by the Clone method to copy the fields as
  // needed to a new data transfer.
  // NOTE: Do not call this method directly.
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
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  /**
   * The actual effect that will be used, and should always be one of the
   * possible values of effectAllowed.
   *
   * For dragstart, drag and dragleave events, the dropEffect is initialized
   * to none. Any value assigned to the dropEffect will be set, but the value
   * isn't used for anything.
   *
   * For the dragenter and dragover events, the dropEffect will be initialized
   * based on what action the user is requesting. How this is determined is
   * platform specific, but typically the user can press modifier keys to
   * adjust which action is desired. Within an event handler for the dragenter
   * and dragover events, the dropEffect should be modified if the action the
   * user is requesting is not the one that is desired.
   *
   * For the drop and dragend events, the dropEffect will be initialized to
   * the action that was desired, which will be the value that the dropEffect
   * had after the last dragenter or dragover event.
   *
   * Possible values:
   *  copy - a copy of the source item is made at the new location
   *  move - an item is moved to a new location
   *  link - a link is established to the source at the new location
   *  none - the item may not be dropped
   *
   * Assigning any other value has no effect and retains the old value.
   */
  void GetDropEffect(nsAString& aDropEffect)
  {
    aDropEffect.AssignASCII(sEffects[mDropEffect]);
  }
  void SetDropEffect(const nsAString& aDropEffect);

  /*
   * Specifies the effects that are allowed for this drag. You may set this in
   * the dragstart event to set the desired effects for the source, and within
   * the dragenter and dragover events to set the desired effects for the
   * target. The value is not used for other events.
   *
   * Possible values:
   *  copy - a copy of the source item is made at the new location
   *  move - an item is moved to a new location
   *  link - a link is established to the source at the new location
   *  copyLink, copyMove, linkMove, all - combinations of the above
   *  none - the item may not be dropped
   *  uninitialized - the default value when the effect has not been set,
   *                  equivalent to all.
   *
   * Assigning any other value has no effect and retains the old value.
   */
  void GetEffectAllowed(nsAString& aEffectAllowed)
  {
    if (mEffectAllowed == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED) {
      aEffectAllowed.AssignLiteral("uninitialized");
    } else {
      aEffectAllowed.AssignASCII(sEffects[mEffectAllowed]);
    }
  }
  void SetEffectAllowed(const nsAString& aEffectAllowed);

  /**
   * Set the image to be used for dragging if a custom one is desired. Most of
   * the time, this would not be set, as a default image is created from the
   * node that was dragged.
   *
   * If the node is an HTML img element, an HTML canvas element or a XUL image
   * element, the image data is used. Otherwise, image should be a visible
   * node and the drag image will be created from this. If image is null, any
   * custom drag image is cleared and the default is used instead.
   *
   * The coordinates specify the offset into the image where the mouse cursor
   * should be. To center the image for instance, use values that are half the
   * width and height.
   *
   * @param image a node to use
   * @param x the horizontal offset
   * @param y the vertical offset
   */
  void SetDragImage(Element& aElement, int32_t aX, int32_t aY);
  void UpdateDragImage(Element& aElement, int32_t aX, int32_t aY);

  void GetTypes(nsTArray<nsString>& aTypes, CallerType aCallerType) const;

  void GetData(const nsAString& aFormat, nsAString& aData,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aRv);

  void SetData(const nsAString& aFormat, const nsAString& aData,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aRv);

  void ClearData(const mozilla::dom::Optional<nsAString>& aFormat,
                 nsIPrincipal& aSubjectPrincipal,
                 mozilla::ErrorResult& aRv);

  /**
   * Holds a list of all the local files available on this data transfer.
   * A dataTransfer containing no files will return an empty list, and an
   * invalid index access on the resulting file list will return null.
   */
  already_AddRefed<FileList>
  GetFiles(nsIPrincipal& aSubjectPrincipal);

  already_AddRefed<Promise>
  GetFilesAndDirectories(nsIPrincipal& aSubjectPrincipal,
                         mozilla::ErrorResult& aRv);

  already_AddRefed<Promise>
  GetFiles(bool aRecursiveFlag,
           nsIPrincipal& aSubjectPrincipal,
           ErrorResult& aRv);


  void AddElement(Element& aElement, mozilla::ErrorResult& aRv);

  uint32_t MozItemCount() const;

  void GetMozCursor(nsAString& aCursor)
  {
    if (mCursorState) {
      aCursor.AssignLiteral("default");
    } else {
      aCursor.AssignLiteral("auto");
    }
  }
  void SetMozCursor(const nsAString& aCursor);

  already_AddRefed<DOMStringList> MozTypesAt(uint32_t aIndex,
                                             CallerType aCallerType,
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

  /*
   * Integer version of dropEffect, set to one of the constants in nsIDragService.
   */
  uint32_t DropEffectInt() const
  {
    return mDropEffect;
  }
  void SetDropEffectInt(uint32_t aDropEffectInt)
  {
    MOZ_RELEASE_ASSERT(aDropEffectInt < ArrayLength(sEffects),
                       "Bogus drop effect value");
    mDropEffect = aDropEffectInt;
  }

  /*
   * Integer version of effectAllowed, set to one or a combination of the
   * constants in nsIDragService.
   */
  uint32_t EffectAllowedInt() const
  {
    return mEffectAllowed;
  }

  void GetMozTriggeringPrincipalURISpec(nsAString& aPrincipalURISpec);

  mozilla::dom::Element* GetDragTarget() const
  {
    return mDragTarget;
  }

  nsresult GetDataAtNoSecurityCheck(const nsAString& aFormat, uint32_t aIndex,
                                    nsIVariant** aData);

  DataTransferItemList* Items() const {
    return mItems;
  }

  // Returns the current "Drag Data Store Mode" of the DataTransfer. This
  // determines what modifications may be performed on the DataTransfer, and
  // what data may be read from it.
  Mode GetMode() const {
    return mMode;
  }
  void SetMode(Mode aMode);

  // Helper method. Is true if the DataTransfer's mode is ReadOnly or Protected,
  // which means that the DataTransfer cannot be modified.
  bool IsReadOnly() const {
    return mMode != Mode::ReadWrite;
  }
  // Helper method. Is true if the DataTransfer's mode is Protected, which means
  // that DataTransfer type information may be read, but data may not be.
  bool IsProtected() const {
    return mMode == Mode::Protected;
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
  already_AddRefed<nsIArray> GetTransferables(nsINode* aDragTarget);

  already_AddRefed<nsIArray>
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

  // Disconnects the DataTransfer from the Drag Data Store. If the
  // dom.dataTransfer.disconnect pref is enabled, this will clear the
  // DataTransfer and set it to the `Protected` state, otherwise this method is
  // a no-op.
  void Disconnect();

  // clears all of the data
  void ClearAll();

  // Similar to SetData except also specifies the principal to store.
  // aData may be null when called from CacheExternalDragFormats or
  // CacheExternalClipboardFormats.
  nsresult SetDataWithPrincipal(const nsAString& aFormat,
                                nsIVariant* aData,
                                uint32_t aIndex,
                                nsIPrincipal* aPrincipal,
                                bool aHidden=false);

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

  // This method makes a copy of the DataTransfer object, with a few properties
  // changed, and the mode updated to reflect the correct mode for the given
  // event. This method is used during the drag operation to generate the
  // DataTransfer objects for each event after `dragstart`. Event objects will
  // lazily clone the DataTransfer stored in the DragSession (which is a clone
  // of the DataTransfer used in the `dragstart` event) when requested.
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

  // Testing method used to emulate internal DataTransfer management.
  // NOTE: Please don't use this. See the comments in the webidl for more.
  already_AddRefed<DataTransfer> MozCloneForEvent(const nsAString& aEvent,
                                                  ErrorResult& aRv);

protected:

  // caches text and uri-list data formats that exist in the drag service or
  // clipboard for retrieval later.
  nsresult CacheExternalData(const char* aFormat, uint32_t aIndex,
                             nsIPrincipal* aPrincipal, bool aHidden);

  // caches the formats that exist in the drag service that were added by an
  // external drag
  void CacheExternalDragFormats();

  // caches the formats that exist in the clipboard
  void CacheExternalClipboardFormats(bool aPlainTextOnly);

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

  // The current "Drag Data Store Mode" which the DataTransfer is in.
  Mode mMode;

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
