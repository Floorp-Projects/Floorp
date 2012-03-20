/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Neil Deakin <enndeakin@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Util.h"

#include "nsDOMDataTransfer.h"

#include "prlog.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIVariant.h"
#include "nsISupportsPrimitives.h"
#include "nsDOMClassInfoID.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMLists.h"
#include "nsGUIEvent.h"
#include "nsDOMError.h"
#include "nsIDragService.h"
#include "nsIScriptableRegion.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsCRT.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIWebNavigation.h"
#include "nsIDocShellTreeItem.h"

using namespace mozilla;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMDataTransfer)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMDataTransfer)
  if (tmp->mFiles) {
    tmp->mFiles->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFiles)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDragTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDragImage)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMDataTransfer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFiles)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDragTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDragImage)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMDataTransfer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMDataTransfer)

DOMCI_DATA(DataTransfer, nsDOMDataTransfer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMDataTransfer)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDataTransfer)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDataTransfer)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DataTransfer)
NS_INTERFACE_MAP_END

// the size of the array
const char nsDOMDataTransfer::sEffects[8][9] = {
  "none", "copy", "move", "copyMove", "link", "copyLink", "linkMove", "all"
};

nsDOMDataTransfer::nsDOMDataTransfer()
  : mEventType(NS_DRAGDROP_START),
    mDropEffect(nsIDragService::DRAGDROP_ACTION_NONE),
    mEffectAllowed(nsIDragService::DRAGDROP_ACTION_UNINITIALIZED),
    mCursorState(false),
    mReadOnly(false),
    mIsExternal(false),
    mUserCancelled(false),
    mIsCrossDomainSubFrameDrop(false),
    mDragImageX(0),
    mDragImageY(0)
{
}

nsDOMDataTransfer::nsDOMDataTransfer(PRUint32 aEventType)
  : mEventType(aEventType),
    mDropEffect(nsIDragService::DRAGDROP_ACTION_NONE),
    mEffectAllowed(nsIDragService::DRAGDROP_ACTION_UNINITIALIZED),
    mCursorState(false),
    mReadOnly(true),
    mIsExternal(true),
    mUserCancelled(false),
    mIsCrossDomainSubFrameDrop(false),
    mDragImageX(0),
    mDragImageY(0)
{
  CacheExternalFormats();
}

nsDOMDataTransfer::nsDOMDataTransfer(PRUint32 aEventType,
                                     const PRUint32 aEffectAllowed,
                                     bool aCursorState,
                                     bool aIsExternal,
                                     bool aUserCancelled,
                                     bool aIsCrossDomainSubFrameDrop,
                                     nsTArray<nsTArray<TransferItem> >& aItems,
                                     nsIDOMElement* aDragImage,
                                     PRUint32 aDragImageX,
                                     PRUint32 aDragImageY)
  : mEventType(aEventType),
    mDropEffect(nsIDragService::DRAGDROP_ACTION_NONE),
    mEffectAllowed(aEffectAllowed),
    mCursorState(aCursorState),
    mReadOnly(true),
    mIsExternal(aIsExternal),
    mUserCancelled(aUserCancelled),
    mIsCrossDomainSubFrameDrop(aIsCrossDomainSubFrameDrop),
    mItems(aItems),
    mDragImage(aDragImage),
    mDragImageX(aDragImageX),
    mDragImageY(aDragImageY)
{
  // The items are copied from aItems into mItems. There is no need to copy
  // the actual data in the items as the data transfer will be read only. The
  // draggesture and dragstart events are the only times when items are
  // modifiable, but those events should have been using the first constructor
  // above.
  NS_ASSERTION(aEventType != NS_DRAGDROP_GESTURE &&
               aEventType != NS_DRAGDROP_START,
               "invalid event type for nsDOMDataTransfer constructor");
}

NS_IMETHODIMP
nsDOMDataTransfer::GetDropEffect(nsAString& aDropEffect)
{
  aDropEffect.AssignASCII(sEffects[mDropEffect]);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::SetDropEffect(const nsAString& aDropEffect)
{
  // the drop effect can only be 'none', 'copy', 'move' or 'link'.
  for (PRUint32 e = 0; e <= nsIDragService::DRAGDROP_ACTION_LINK; e++) {
    if (aDropEffect.EqualsASCII(sEffects[e])) {
      // don't allow copyMove
      if (e != (nsIDragService::DRAGDROP_ACTION_COPY |
                nsIDragService::DRAGDROP_ACTION_MOVE))
        mDropEffect = e;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetEffectAllowed(nsAString& aEffectAllowed)
{
  if (mEffectAllowed == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED)
    aEffectAllowed.AssignLiteral("uninitialized");
  else
    aEffectAllowed.AssignASCII(sEffects[mEffectAllowed]);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::SetEffectAllowed(const nsAString& aEffectAllowed)
{
  if (aEffectAllowed.EqualsLiteral("uninitialized")) {
    mEffectAllowed = nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;
    return NS_OK;
  }

  PR_STATIC_ASSERT(nsIDragService::DRAGDROP_ACTION_NONE == 0);
  PR_STATIC_ASSERT(nsIDragService::DRAGDROP_ACTION_COPY == 1);
  PR_STATIC_ASSERT(nsIDragService::DRAGDROP_ACTION_MOVE == 2);
  PR_STATIC_ASSERT(nsIDragService::DRAGDROP_ACTION_LINK == 4);

  for (PRUint32 e = 0; e < ArrayLength(sEffects); e++) {
    if (aEffectAllowed.EqualsASCII(sEffects[e])) {
      mEffectAllowed = e;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetDropEffectInt(PRUint32* aDropEffect)
{
  *aDropEffect = mDropEffect;
  return  NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::SetDropEffectInt(PRUint32 aDropEffect)
{
  mDropEffect = aDropEffect;
  return  NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetEffectAllowedInt(PRUint32* aEffectAllowed)
{
  *aEffectAllowed = mEffectAllowed;
  return  NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::SetEffectAllowedInt(PRUint32 aEffectAllowed)
{
  mEffectAllowed = aEffectAllowed;
  return  NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetMozUserCancelled(bool* aUserCancelled)
{
  *aUserCancelled = mUserCancelled;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetFiles(nsIDOMFileList** aFileList)
{
  *aFileList = nsnull;

  if (mEventType != NS_DRAGDROP_DROP && mEventType != NS_DRAGDROP_DRAGDROP)
    return NS_OK;

  if (!mFiles) {
    mFiles = new nsDOMFileList(static_cast<nsIDOMDataTransfer*>(this));
    NS_ENSURE_TRUE(mFiles, NS_ERROR_OUT_OF_MEMORY);

    PRUint32 count = mItems.Length();

    for (PRUint32 i = 0; i < count; i++) {
      nsCOMPtr<nsIVariant> variant;
      nsresult rv = MozGetDataAt(NS_ConvertUTF8toUTF16(kFileMime), i, getter_AddRefs(variant));
      NS_ENSURE_SUCCESS(rv, rv);

      if (!variant)
        continue;

      nsCOMPtr<nsISupports> supports;
      rv = variant->GetAsISupports(getter_AddRefs(supports));

      if (NS_FAILED(rv))
        continue;

      nsCOMPtr<nsIFile> file = do_QueryInterface(supports);

      if (!file)
        continue;

      nsRefPtr<nsDOMFileFile> domFile = new nsDOMFileFile(file);

      if (!mFiles->Append(domFile))
        return NS_ERROR_FAILURE;
    }
  }

  *aFileList = mFiles;
  NS_ADDREF(*aFileList);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetTypes(nsIDOMDOMStringList** aTypes)
{
  *aTypes = nsnull;

  nsRefPtr<nsDOMStringList> types = new nsDOMStringList();
  NS_ENSURE_TRUE(types, NS_ERROR_OUT_OF_MEMORY);

  if (mItems.Length()) {
    nsTArray<TransferItem>& item = mItems[0];
    for (PRUint32 i = 0; i < item.Length(); i++)
      types->Add(item[i].mFormat);

    bool filePresent, filePromisePresent;
    types->Contains(NS_LITERAL_STRING(kFileMime), &filePresent);
    types->Contains(NS_LITERAL_STRING("application/x-moz-file-promise"), &filePromisePresent);
    if (filePresent || filePromisePresent)
      types->Add(NS_LITERAL_STRING("Files"));
  }

  *aTypes = types;
  NS_ADDREF(*aTypes);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetData(const nsAString& aFormat, nsAString& aData)
{
  // return an empty string if data for the format was not found
  aData.Truncate();

  nsCOMPtr<nsIVariant> data;
  nsresult rv = MozGetDataAt(aFormat, 0, getter_AddRefs(data));
  if (rv == NS_ERROR_DOM_INDEX_SIZE_ERR)
    return NS_OK;

  NS_ENSURE_SUCCESS(rv, rv);

  if (data) {
    nsAutoString stringdata;
    data->GetAsAString(stringdata);

    // for the URL type, parse out the first URI from the list. The URIs are
    // separated by newlines
    nsAutoString lowercaseFormat;
    nsContentUtils::ASCIIToLower(aFormat, lowercaseFormat);
    
    if (lowercaseFormat.EqualsLiteral("url")) {
      PRInt32 lastidx = 0, idx;
      PRInt32 length = stringdata.Length();
      while (lastidx < length) {
        idx = stringdata.FindChar('\n', lastidx);
        // lines beginning with # are comments
        if (stringdata[lastidx] == '#') {
          if (idx == -1)
            break;
        }
        else {
          if (idx == -1)
            aData.Assign(Substring(stringdata, lastidx));
          else
            aData.Assign(Substring(stringdata, lastidx, idx - lastidx));
          aData = nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(aData, true);
          return NS_OK;
        }
        lastidx = idx + 1;
      }
    }
    else {
      aData = stringdata;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::SetData(const nsAString& aFormat, const nsAString& aData)
{
  nsCOMPtr<nsIWritableVariant> variant = do_CreateInstance(NS_VARIANT_CONTRACTID);
  NS_ENSURE_TRUE(variant, NS_ERROR_OUT_OF_MEMORY);

  variant->SetAsAString(aData);

  return MozSetDataAt(aFormat, variant, 0);
}

NS_IMETHODIMP
nsDOMDataTransfer::ClearData(const nsAString& aFormat)
{
  nsresult rv = MozClearDataAt(aFormat, 0);
  return (rv == NS_ERROR_DOM_INDEX_SIZE_ERR) ? NS_OK : rv;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetMozItemCount(PRUint32* aCount)
{
  *aCount = mItems.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetMozCursor(nsAString& aCursorState)
{
  if (mCursorState) {
    aCursorState.AssignLiteral("default");
  } else {
    aCursorState.AssignLiteral("auto");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::SetMozCursor(const nsAString& aCursorState)
{
  // Lock the cursor to an arrow during the drag.
  mCursorState = aCursorState.EqualsLiteral("default");

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::GetMozSourceNode(nsIDOMNode** aSourceNode)
{
  *aSourceNode = nsnull;

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (!dragSession)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> sourceNode;
  dragSession->GetSourceNode(getter_AddRefs(sourceNode));
  if (sourceNode && !nsContentUtils::CanCallerAccess(sourceNode))
    return NS_OK;

  sourceNode.swap(*aSourceNode);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::MozTypesAt(PRUint32 aIndex, nsIDOMDOMStringList** aTypes)
{
  *aTypes = nsnull;

  nsRefPtr<nsDOMStringList> types = new nsDOMStringList();
  NS_ENSURE_TRUE(types, NS_ERROR_OUT_OF_MEMORY);

  if (aIndex < mItems.Length()) {
    // note that you can retrieve the types regardless of their principal
    nsTArray<TransferItem>& item = mItems[aIndex];
    for (PRUint32 i = 0; i < item.Length(); i++)
      types->Add(item[i].mFormat);
  }

  *aTypes = types;
  NS_ADDREF(*aTypes);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::MozGetDataAt(const nsAString& aFormat,
                                PRUint32 aIndex,
                                nsIVariant** aData)
{
  *aData = nsnull;

  if (aFormat.IsEmpty())
    return NS_OK;

  if (aIndex >= mItems.Length())
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  nsAutoString format;
  GetRealFormat(aFormat, format);

  nsTArray<TransferItem>& item = mItems[aIndex];

  // Check if the caller is allowed to access the drag data. Callers with
  // UniversalXPConnect privileges can always read the data. During the
  // drop event, allow retrieving the data except in the case where the
  // source of the drag is in a child frame of the caller. In that case,
  // we only allow access to data of the same principal. During other events,
  // only allow access to the data with the same principal.
  nsIPrincipal* principal = nsnull;
  if (mIsCrossDomainSubFrameDrop ||
      (mEventType != NS_DRAGDROP_DROP && mEventType != NS_DRAGDROP_DRAGDROP &&
       !nsContentUtils::CallerHasUniversalXPConnect())) {
    nsresult rv = NS_OK;
    principal = GetCurrentPrincipal(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRUint32 count = item.Length();
  for (PRUint32 i = 0; i < count; i++) {
    TransferItem& formatitem = item[i];
    if (formatitem.mFormat.Equals(format)) {
      bool subsumes;
      if (formatitem.mPrincipal && principal &&
          (NS_FAILED(principal->Subsumes(formatitem.mPrincipal, &subsumes)) || !subsumes))
        return NS_ERROR_DOM_SECURITY_ERR;

      if (!formatitem.mData) {
        FillInExternalDragData(formatitem, aIndex);
      } else {
        nsCOMPtr<nsISupports> data;
        formatitem.mData->GetAsISupports(getter_AddRefs(data));
        // Make sure the code that is calling us is same-origin with the data.
        nsCOMPtr<nsIDOMEventTarget> pt = do_QueryInterface(data);
        if (pt) {
          nsresult rv = NS_OK;
          nsIScriptContext* c = pt->GetContextForEventHandlers(&rv);
          NS_ENSURE_TRUE(c && NS_SUCCEEDED(rv), NS_ERROR_DOM_SECURITY_ERR);
          nsIScriptObjectPrincipal* sp = c->GetObjectPrincipal();
          NS_ENSURE_TRUE(sp, NS_ERROR_DOM_SECURITY_ERR);
          nsIPrincipal* dataPrincipal = sp->GetPrincipal();
          NS_ENSURE_TRUE(dataPrincipal, NS_ERROR_DOM_SECURITY_ERR);
          NS_ENSURE_TRUE(principal || (principal = GetCurrentPrincipal(&rv)),
                         NS_ERROR_DOM_SECURITY_ERR);
          NS_ENSURE_SUCCESS(rv, rv);
          bool equals = false;
          NS_ENSURE_TRUE(NS_SUCCEEDED(principal->Equals(dataPrincipal, &equals)) && equals,
                         NS_ERROR_DOM_SECURITY_ERR);
        }
      }
      *aData = formatitem.mData;
      NS_IF_ADDREF(*aData);
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::MozSetDataAt(const nsAString& aFormat,
                                nsIVariant* aData,
                                PRUint32 aIndex)
{
  NS_ENSURE_TRUE(aData, NS_ERROR_NULL_POINTER);

  if (aFormat.IsEmpty())
    return NS_OK;

  if (mReadOnly)
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;

  // Specifying an index less than the current length will replace an existing
  // item. Specifying an index equal to the current length will add a new item.
  if (aIndex > mItems.Length())
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  // don't allow non-chrome to add file data
  // XXX perhaps this should also limit any non-string type as well
  if ((aFormat.EqualsLiteral("application/x-moz-file-promise") ||
       aFormat.EqualsLiteral("application/x-moz-file")) &&
       !nsContentUtils::CallerHasUniversalXPConnect()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsresult rv = NS_OK;
  nsIPrincipal* principal = GetCurrentPrincipal(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return SetDataWithPrincipal(aFormat, aData, aIndex, principal);
}

NS_IMETHODIMP
nsDOMDataTransfer::MozClearDataAt(const nsAString& aFormat, PRUint32 aIndex)
{
  if (mReadOnly)
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;

  if (aIndex >= mItems.Length())
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  nsAutoString format;
  GetRealFormat(aFormat, format);

  nsresult rv = NS_OK;
  nsIPrincipal* principal = GetCurrentPrincipal(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // if the format is empty, clear all formats
  bool clearall = format.IsEmpty();

  nsTArray<TransferItem>& item = mItems[aIndex];
  // count backwards so that the count and index don't have to be adjusted
  // after removing an element
  for (PRInt32 i = item.Length() - 1; i >= 0; i--) {
    TransferItem& formatitem = item[i];
    if (clearall || formatitem.mFormat.Equals(format)) {
      // don't allow removing data that has a stronger principal
      bool subsumes;
      if (formatitem.mPrincipal && principal &&
          (NS_FAILED(principal->Subsumes(formatitem.mPrincipal, &subsumes)) || !subsumes))
        return NS_ERROR_DOM_SECURITY_ERR;

      item.RemoveElementAt(i);

      // if a format was specified, break out. Otherwise, loop around until
      // all formats have been removed
      if (!clearall)
        break;
    }
  }

  // if the last format for an item is removed, remove the entire item
  if (!item.Length())
     mItems.RemoveElementAt(aIndex);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::SetDragImage(nsIDOMElement* aImage, PRInt32 aX, PRInt32 aY)
{
  if (mReadOnly)
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;

  if (aImage) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aImage);
    NS_ENSURE_TRUE(content, NS_ERROR_INVALID_ARG);
  }
  mDragImage = aImage;
  mDragImageX = aX;
  mDragImageY = aY;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataTransfer::AddElement(nsIDOMElement* aElement)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  if (aElement) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    NS_ENSURE_TRUE(content, NS_ERROR_INVALID_ARG);
  }

  if (mReadOnly)
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;

  mDragTarget = do_QueryInterface(aElement);

  return NS_OK;
}

nsresult
nsDOMDataTransfer::Clone(PRUint32 aEventType, bool aUserCancelled,
                         bool aIsCrossDomainSubFrameDrop,
                         nsIDOMDataTransfer** aNewDataTransfer)
{
  nsDOMDataTransfer* newDataTransfer =
    new nsDOMDataTransfer(aEventType, mEffectAllowed, mCursorState,
                          mIsExternal, aUserCancelled, aIsCrossDomainSubFrameDrop,
                          mItems, mDragImage, mDragImageX, mDragImageY);
  NS_ENSURE_TRUE(newDataTransfer, NS_ERROR_OUT_OF_MEMORY);

  *aNewDataTransfer = newDataTransfer;
  NS_ADDREF(*aNewDataTransfer);
  return NS_OK;
}

void
nsDOMDataTransfer::GetTransferables(nsISupportsArray** aArray)
{
  *aArray = nsnull;

  nsCOMPtr<nsISupportsArray> transArray =
    do_CreateInstance("@mozilla.org/supports-array;1");
  if (!transArray)
    return;

  bool added = false;
  PRUint32 count = mItems.Length();
  for (PRUint32 i = 0; i < count; i++) {

    nsTArray<TransferItem>& item = mItems[i];
    PRUint32 count = item.Length();
    if (!count)
      continue;

    nsCOMPtr<nsITransferable> transferable =
      do_CreateInstance("@mozilla.org/widget/transferable;1");
    if (!transferable)
      return;

    for (PRUint32 f = 0; f < count; f++) {
      TransferItem& formatitem = item[f];
      if (!formatitem.mData) // skip empty items
        continue;

      PRUint32 length;
      nsCOMPtr<nsISupports> convertedData;
      if (!ConvertFromVariant(formatitem.mData, getter_AddRefs(convertedData), &length))
        continue;

      // the underlying drag code uses text/unicode, so use that instead of text/plain
      const char* format;
      NS_ConvertUTF16toUTF8 utf8format(formatitem.mFormat);
      if (utf8format.EqualsLiteral("text/plain"))
        format = kUnicodeMime;
      else
        format = utf8format.get();

      // if a converter is set for a format, set the converter for the
      // transferable and don't add the item
      nsCOMPtr<nsIFormatConverter> converter = do_QueryInterface(convertedData);
      if (converter) {
        transferable->AddDataFlavor(format);
        transferable->SetConverter(converter);
        continue;
      }

      nsresult rv = transferable->SetTransferData(format, convertedData, length);
      if (NS_FAILED(rv))
        return;

      added = true;
    }

    // only append the transferable if data was successfully added to it
    if (added)
      transArray->AppendElement(transferable);
  }

  NS_ADDREF(*aArray = transArray);
}

bool
nsDOMDataTransfer::ConvertFromVariant(nsIVariant* aVariant,
                                      nsISupports** aSupports,
                                      PRUint32* aLength)
{
  *aSupports = nsnull;
  *aLength = 0;

  PRUint16 type;
  aVariant->GetDataType(&type);
  if (type == nsIDataType::VTYPE_INTERFACE ||
      type == nsIDataType::VTYPE_INTERFACE_IS) {
    nsCOMPtr<nsISupports> data;
    if (NS_FAILED(aVariant->GetAsISupports(getter_AddRefs(data))))
       return false;
 
    nsCOMPtr<nsIFlavorDataProvider> fdp = do_QueryInterface(data);
    if (fdp) {
      // for flavour data providers, use kFlavorHasDataProvider (which has the
      // value 0) as the length.
      NS_ADDREF(*aSupports = fdp);
      *aLength = nsITransferable::kFlavorHasDataProvider;
    }
    else {
      // wrap the item in an nsISupportsInterfacePointer
      nsCOMPtr<nsISupportsInterfacePointer> ptrSupports =
        do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID);
      if (!ptrSupports)
        return false;

      ptrSupports->SetData(data);
      NS_ADDREF(*aSupports = ptrSupports);

      *aLength = sizeof(nsISupportsInterfacePointer *);
    }

    return true;
  }

  PRUnichar* chrs;
  PRUint32 len = 0;
  nsresult rv = aVariant->GetAsWStringWithSize(&len, &chrs);
  if (NS_FAILED(rv))
    return false;

  nsAutoString str;
  str.Adopt(chrs, len);

  nsCOMPtr<nsISupportsString>
    strSupports(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (!strSupports)
    return false;

  strSupports->SetData(str);

  *aSupports = strSupports;
  NS_ADDREF(*aSupports);

  // each character is two bytes
  *aLength = str.Length() << 1;

  return true;
}

void
nsDOMDataTransfer::ClearAll()
{
  mItems.Clear();
}

nsresult
nsDOMDataTransfer::SetDataWithPrincipal(const nsAString& aFormat,
                                        nsIVariant* aData,
                                        PRUint32 aIndex,
                                        nsIPrincipal* aPrincipal)
{
  nsAutoString format;
  GetRealFormat(aFormat, format);

  // check if the item for the format already exists. In that case,
  // just replace it.
  TransferItem* formatitem;
  if (aIndex < mItems.Length()) {
    nsTArray<TransferItem>& item = mItems[aIndex];
    PRUint32 count = item.Length();
    for (PRUint32 i = 0; i < count; i++) {
      TransferItem& itemformat = item[i];
      if (itemformat.mFormat.Equals(format)) {
        // don't allow replacing data that has a stronger principal
        bool subsumes;
        if (itemformat.mPrincipal && aPrincipal &&
            (NS_FAILED(aPrincipal->Subsumes(itemformat.mPrincipal, &subsumes)) || !subsumes))
          return NS_ERROR_DOM_SECURITY_ERR;

        itemformat.mPrincipal = aPrincipal;
        itemformat.mData = aData;
        return NS_OK;
      }
    }

    // add a new format
    formatitem = item.AppendElement();
  }
  else {
    NS_ASSERTION(aIndex == mItems.Length(), "Index out of range");

    // add a new index
    nsTArray<TransferItem>* item = mItems.AppendElement();
    NS_ENSURE_TRUE(item, NS_ERROR_OUT_OF_MEMORY);

    formatitem = item->AppendElement();
  }

  NS_ENSURE_TRUE(formatitem, NS_ERROR_OUT_OF_MEMORY);

  formatitem->mFormat = format;
  formatitem->mPrincipal = aPrincipal;
  formatitem->mData = aData;

  return NS_OK;
}

nsIPrincipal*
nsDOMDataTransfer::GetCurrentPrincipal(nsresult* rv)
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();

  nsCOMPtr<nsIPrincipal> currentPrincipal;
  *rv = ssm->GetSubjectPrincipal(getter_AddRefs(currentPrincipal));
  NS_ENSURE_SUCCESS(*rv, nsnull);

  if (!currentPrincipal)
    ssm->GetSystemPrincipal(getter_AddRefs(currentPrincipal));

  return currentPrincipal.get();
}

void
nsDOMDataTransfer::GetRealFormat(const nsAString& aInFormat, nsAString& aOutFormat)
{
  // treat text/unicode as equivalent to text/plain
  nsAutoString lowercaseFormat;
  nsContentUtils::ASCIIToLower(aInFormat, lowercaseFormat);
  if (lowercaseFormat.EqualsLiteral("text") || lowercaseFormat.EqualsLiteral("text/unicode"))
    aOutFormat.AssignLiteral("text/plain");
  else if (lowercaseFormat.EqualsLiteral("url"))
    aOutFormat.AssignLiteral("text/uri-list");
  else
    aOutFormat.Assign(lowercaseFormat);
}

void
nsDOMDataTransfer::CacheExternalFormats()
{
  // Called during the constructor to cache the formats available from an
  // external drag. The data associated with each format will be set to null.
  // This data will instead only be retrieved in FillInExternalDragData when
  // asked for, as it may be time consuming for the source application to
  // generate it.

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (!dragSession)
    return;

  // make sure that the system principal is used for external drags
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> sysPrincipal;
  ssm->GetSystemPrincipal(getter_AddRefs(sysPrincipal));

  // there isn't a way to get a list of the formats that might be available on
  // all platforms, so just check for the types that can actually be imported
  // XXXndeakin there are some other formats but those are platform specific.
  const char* formats[] = { kFileMime, kHTMLMime, kURLMime, kURLDataMime, kUnicodeMime };

  PRUint32 count;
  dragSession->GetNumDropItems(&count);
  for (PRUint32 c = 0; c < count; c++) {
    for (PRUint32 f = 0; f < ArrayLength(formats); f++) {
      // IsDataFlavorSupported doesn't take an index as an argument and just
      // checks if any of the items support a particular flavor, even though
      // the GetData method does take an index. Here, we just assume that
      // every item being dragged has the same set of flavors.
      bool supported;
      dragSession->IsDataFlavorSupported(formats[f], &supported);
      // if the format is supported, add an item to the array with null as
      // the data. When retrieved, GetRealData will read the data.
      if (supported) {
        if (strcmp(formats[f], kUnicodeMime) == 0) {
          SetDataWithPrincipal(NS_LITERAL_STRING("text/plain"), nsnull, c, sysPrincipal);
        }
        else {
          if (strcmp(formats[f], kURLDataMime) == 0)
            SetDataWithPrincipal(NS_LITERAL_STRING("text/uri-list"), nsnull, c, sysPrincipal);
          SetDataWithPrincipal(NS_ConvertUTF8toUTF16(formats[f]), nsnull, c, sysPrincipal);
        }
      }
    }
  }
}

void
nsDOMDataTransfer::FillInExternalDragData(TransferItem& aItem, PRUint32 aIndex)
{
  NS_PRECONDITION(mIsExternal, "Not an external drag");

  if (!aItem.mData) {
    nsCOMPtr<nsITransferable> trans =
      do_CreateInstance("@mozilla.org/widget/transferable;1");
    if (!trans)
      return;

    NS_ConvertUTF16toUTF8 utf8format(aItem.mFormat);
    const char* format = utf8format.get();
    if (strcmp(format, "text/plain") == 0)
      format = kUnicodeMime;
    else if (strcmp(format, "text/uri-list") == 0)
      format = kURLDataMime;

    nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
    if (!dragSession)
      return;

    trans->AddDataFlavor(format);
    dragSession->GetData(trans, aIndex);

    PRUint32 length = 0;
    nsCOMPtr<nsISupports> data;
    trans->GetTransferData(format, getter_AddRefs(data), &length);
    if (!data)
      return;

    nsCOMPtr<nsIWritableVariant> variant = do_CreateInstance(NS_VARIANT_CONTRACTID);
    if (!variant)
      return;

    nsCOMPtr<nsISupportsString> supportsstr = do_QueryInterface(data);
    if (supportsstr) {
      nsAutoString str;
      supportsstr->GetData(str);
      variant->SetAsAString(str);
    }
    else {
      variant->SetAsISupports(data);
    }
    aItem.mData = variant;
  }
}
