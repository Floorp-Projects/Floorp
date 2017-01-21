/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFileControlFrame.h"

#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/HTMLButtonElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/Preferences.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileList.h"
#include "nsIDOMDragEvent.h"
#include "nsIDOMFileList.h"
#include "nsContentList.h"
#include "nsIDOMMutationEvent.h"
#include "nsTextNode.h"

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame*
NS_NewFileControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFileControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsFileControlFrame)

nsFileControlFrame::nsFileControlFrame(nsStyleContext* aContext)
  : nsBlockFrame(aContext)
{
  AddStateBits(NS_BLOCK_FLOAT_MGR);
}


void
nsFileControlFrame::Init(nsIContent*       aContent,
                         nsContainerFrame* aParent,
                         nsIFrame*         aPrevInFlow)
{
  nsBlockFrame::Init(aContent, aParent, aPrevInFlow);

  mMouseListener = new DnDListener(this);
}

void
nsFileControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  ENSURE_TRUE(mContent);

  // Remove the events.
  if (mContent) {
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("drop"),
                                        mMouseListener, false);
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("dragover"),
                                        mMouseListener, false);
  }

  nsContentUtils::DestroyAnonymousContent(&mTextContent);
  nsContentUtils::DestroyAnonymousContent(&mBrowseFilesOrDirs);

  mMouseListener->ForgetFrame();
  nsBlockFrame::DestroyFrom(aDestructRoot);
}

static already_AddRefed<Element>
MakeAnonButton(nsIDocument* aDoc, const char* labelKey,
               HTMLInputElement* aInputElement,
               const nsAString& aAccessKey)
{
  RefPtr<Element> button = aDoc->CreateHTMLElement(nsGkAtoms::button);
  // NOTE: SetIsNativeAnonymousRoot() has to be called before setting any
  // attribute.
  button->SetIsNativeAnonymousRoot();
  button->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                  NS_LITERAL_STRING("button"), false);

  // Set the file picking button text depending on the current locale.
  nsXPIDLString buttonTxt;
  nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                     labelKey, buttonTxt);

  // Set the browse button text. It's a bit of a pain to do because we want to
  // make sure we are not notifying.
  RefPtr<nsTextNode> textContent =
    new nsTextNode(button->NodeInfo()->NodeInfoManager());

  textContent->SetText(buttonTxt, false);

  nsresult rv = button->AppendChildTo(textContent, false);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  // Make sure access key and tab order for the element actually redirect to the
  // file picking button.
  RefPtr<HTMLButtonElement> buttonElement =
    HTMLButtonElement::FromContentOrNull(button);

  if (!aAccessKey.IsEmpty()) {
    buttonElement->SetAccessKey(aAccessKey);
  }

  // Both elements are given the same tab index so that the user can tab
  // to the file control at the correct index, and then between the two
  // buttons.
  int32_t tabIndex;
  aInputElement->GetTabIndex(&tabIndex);
  buttonElement->SetTabIndex(tabIndex);

  return button.forget();
}

nsresult
nsFileControlFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  nsCOMPtr<nsIDocument> doc = mContent->GetComposedDoc();

  RefPtr<HTMLInputElement> fileContent = HTMLInputElement::FromContentOrNull(mContent);

  // The access key is transferred to the "Choose files..." button only. In
  // effect that access key allows access to the control via that button, then
  // the user can tab between the two buttons.
  nsAutoString accessKey;
  fileContent->GetAccessKey(accessKey);

  mBrowseFilesOrDirs = MakeAnonButton(doc, "Browse", fileContent, accessKey);
  if (!mBrowseFilesOrDirs || !aElements.AppendElement(mBrowseFilesOrDirs)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Create and setup the text showing the selected files.
  RefPtr<NodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::label, nullptr,
                                                 kNameSpaceID_XUL,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_TrustedNewXULElement(getter_AddRefs(mTextContent), nodeInfo.forget());
  // NOTE: SetIsNativeAnonymousRoot() has to be called before setting any
  // attribute.
  mTextContent->SetIsNativeAnonymousRoot();
  mTextContent->SetAttr(kNameSpaceID_None, nsGkAtoms::crop,
                        NS_LITERAL_STRING("center"), false);

  // Update the displayed text to reflect the current element's value.
  nsAutoString value;
  HTMLInputElement::FromContent(mContent)->GetDisplayFileName(value);
  UpdateDisplayedValue(value, false);

  if (!aElements.AppendElement(mTextContent)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // We should be able to interact with the element by doing drag and drop.
  mContent->AddSystemEventListener(NS_LITERAL_STRING("drop"),
                                   mMouseListener, false);
  mContent->AddSystemEventListener(NS_LITERAL_STRING("dragover"),
                                   mMouseListener, false);

  SyncDisabledState();

  return NS_OK;
}

void
nsFileControlFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                             uint32_t aFilter)
{
  if (mBrowseFilesOrDirs) {
    aElements.AppendElement(mBrowseFilesOrDirs);
  }

  if (mTextContent) {
    aElements.AppendElement(mTextContent);
  }
}

NS_QUERYFRAME_HEAD(nsFileControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

void 
nsFileControlFrame::SetFocus(bool aOn, bool aRepaint)
{
}

static void
AppendBlobImplAsDirectory(nsTArray<OwningFileOrDirectory>& aArray,
                          BlobImpl* aBlobImpl,
                          nsIContent* aContent)
{
  MOZ_ASSERT(aBlobImpl);
  MOZ_ASSERT(aBlobImpl->IsDirectory());

  nsAutoString fullpath;
  ErrorResult err;
  aBlobImpl->GetMozFullPath(fullpath, err);
  if (err.Failed()) {
    err.SuppressException();
    return;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(fullpath, true, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsPIDOMWindowInner* inner = aContent->OwnerDoc()->GetInnerWindow();
  if (!inner || !inner->IsCurrentInnerWindow()) {
    return;
  }

  RefPtr<Directory> directory =
    Directory::Create(inner, file);
  MOZ_ASSERT(directory);

  OwningFileOrDirectory* element = aArray.AppendElement();
  element->SetAsDirectory() = directory;
}

/**
 * This is called when we receive a drop or a dragover.
 */
NS_IMETHODIMP
nsFileControlFrame::DnDListener::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(mFrame, "We should have been unregistered");

  bool defaultPrevented = false;
  aEvent->GetDefaultPrevented(&defaultPrevented);
  if (defaultPrevented) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDragEvent> dragEvent = do_QueryInterface(aEvent);
  if (!dragEvent) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
  dragEvent->GetDataTransfer(getter_AddRefs(dataTransfer));
  if (!IsValidDropData(dataTransfer)) {
    return NS_OK;
  }


  nsCOMPtr<nsIContent> content = mFrame->GetContent();
  bool supportsMultiple = content && content->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple);
  if (!CanDropTheseFiles(dataTransfer, supportsMultiple)) {
    dataTransfer->SetDropEffect(NS_LITERAL_STRING("none"));
    aEvent->StopPropagation();
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("dragover")) {
    // Prevent default if we can accept this drag data
    aEvent->PreventDefault();
    return NS_OK;
  }

  if (eventType.EqualsLiteral("drop")) {
    aEvent->StopPropagation();
    aEvent->PreventDefault();

    NS_ASSERTION(content, "The frame has no content???");

    HTMLInputElement* inputElement = HTMLInputElement::FromContent(content);
    NS_ASSERTION(inputElement, "No input element for this file upload control frame!");

    nsCOMPtr<nsIDOMFileList> fileList;
    dataTransfer->GetFiles(getter_AddRefs(fileList));

    RefPtr<BlobImpl> webkitDir;
    nsresult rv =
      GetBlobImplForWebkitDirectory(fileList, getter_AddRefs(webkitDir));
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsTArray<OwningFileOrDirectory> array;
    if (webkitDir) {
      AppendBlobImplAsDirectory(array, webkitDir, content);
      inputElement->MozSetDndFilesAndDirectories(array);
    } else {
      bool blinkFileSystemEnabled =
        Preferences::GetBool("dom.webkitBlink.filesystem.enabled", false);
      if (blinkFileSystemEnabled) {
        FileList* files = static_cast<FileList*>(fileList.get());
        if (files) {
          for (uint32_t i = 0; i < files->Length(); ++i) {
            File* file = files->Item(i);
            if (file) {
              if (file->Impl() && file->Impl()->IsDirectory()) {
                AppendBlobImplAsDirectory(array, file->Impl(), content);
              } else {
                OwningFileOrDirectory* element = array.AppendElement();
                element->SetAsFile() = file;
              }
            }
          }
        }
      }

      // This is rather ugly. Pass the directories as Files using SetFiles,
      // but then if blink filesystem API is enabled, it wants
      // FileOrDirectory array.
      inputElement->SetFiles(fileList, true);
      if (blinkFileSystemEnabled) {
        inputElement->UpdateEntries(array);
      }
      nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(), content,
                                           NS_LITERAL_STRING("input"), true,
                                           false);
      nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(), content,
                                           NS_LITERAL_STRING("change"), true,
                                           false);
    }
  }

  return NS_OK;
}

nsresult
nsFileControlFrame::DnDListener::GetBlobImplForWebkitDirectory(nsIDOMFileList* aFileList,
                                                               BlobImpl** aBlobImpl)
{
  *aBlobImpl = nullptr;

  HTMLInputElement* inputElement =
    HTMLInputElement::FromContent(mFrame->GetContent());
  bool webkitDirPicker =
    Preferences::GetBool("dom.webkitBlink.dirPicker.enabled", false) &&
    inputElement->HasAttr(kNameSpaceID_None, nsGkAtoms::webkitdirectory);
  if (!webkitDirPicker) {
    return NS_OK;
  }

  if (!aFileList) {
    return NS_ERROR_FAILURE;
  }

  FileList* files = static_cast<FileList*>(aFileList);
  // webkitdirectory doesn't care about the length of the file list but
  // only about the first item on it.
  uint32_t len = files->Length();
  if (len) {
    File* file = files->Item(0);
    if (file) {
      BlobImpl* impl = file->Impl();
      if (impl && impl->IsDirectory()) {
        RefPtr<BlobImpl> retVal = impl;
        retVal.swap(*aBlobImpl);
        return NS_OK;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

bool
nsFileControlFrame::DnDListener::IsValidDropData(nsIDOMDataTransfer* aDOMDataTransfer)
{
  nsCOMPtr<DataTransfer> dataTransfer = do_QueryInterface(aDOMDataTransfer);
  NS_ENSURE_TRUE(dataTransfer, false);

  // We only support dropping files onto a file upload control
  nsTArray<nsString> types;
  dataTransfer->GetTypes(types, CallerType::System);

  return types.Contains(NS_LITERAL_STRING("Files"));
}

bool
nsFileControlFrame::DnDListener::CanDropTheseFiles(nsIDOMDataTransfer* aDOMDataTransfer,
                                                   bool aSupportsMultiple)
{
  nsCOMPtr<DataTransfer> dataTransfer = do_QueryInterface(aDOMDataTransfer);
  NS_ENSURE_TRUE(dataTransfer, false);

  nsCOMPtr<nsIDOMFileList> fileList;
  dataTransfer->GetFiles(getter_AddRefs(fileList));

  RefPtr<BlobImpl> webkitDir;
  nsresult rv =
    GetBlobImplForWebkitDirectory(fileList, getter_AddRefs(webkitDir));
  // Just check if either there isn't webkitdirectory attribute, or
  // fileList has a directory which can be dropped to the element.
  // No need to use webkitDir for anything here.
  NS_ENSURE_SUCCESS(rv, false);

  uint32_t listLength = 0;
  if (fileList) {
    fileList->GetLength(&listLength);
  }
  return listLength <= 1 || aSupportsMultiple;
}

nscoord
nsFileControlFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  // Our min width is our pref width
  result = GetPrefISize(aRenderingContext);
  return result;
}

void
nsFileControlFrame::SyncDisabledState()
{
  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    mBrowseFilesOrDirs->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled,
                                EmptyString(), true);
  } else {
    mBrowseFilesOrDirs->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  }
}

nsresult
nsFileControlFrame::AttributeChanged(int32_t  aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::tabindex) {
    if (aModType == nsIDOMMutationEvent::REMOVAL) {
      mBrowseFilesOrDirs->UnsetAttr(aNameSpaceID, aAttribute, true);
    } else {
      nsAutoString value;
      mContent->GetAttr(aNameSpaceID, aAttribute, value);
      mBrowseFilesOrDirs->SetAttr(aNameSpaceID, aAttribute, value, true);
    }
  }

  return nsBlockFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

void
nsFileControlFrame::ContentStatesChanged(EventStates aStates)
{
  if (aStates.HasState(NS_EVENT_STATE_DISABLED)) {
    nsContentUtils::AddScriptRunner(new SyncDisabledStateEvent(this));
  }
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsFileControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FileControl"), aResult);
}
#endif

void
nsFileControlFrame::UpdateDisplayedValue(const nsAString& aValue, bool aNotify)
{
  mTextContent->SetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue, aNotify);
}

nsresult
nsFileControlFrame::SetFormProperty(nsIAtom* aName,
                                    const nsAString& aValue)
{
  if (nsGkAtoms::value == aName) {
    UpdateDisplayedValue(aValue, true);
  }
  return NS_OK;
}

void
nsFileControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  BuildDisplayListForInline(aBuilder, aDirtyRect, aLists);
}

#ifdef ACCESSIBILITY
a11y::AccType
nsFileControlFrame::AccessibleType()
{
  return a11y::eHTMLFileInputType;
}
#endif

////////////////////////////////////////////////////////////
// Mouse listener implementation

NS_IMPL_ISUPPORTS(nsFileControlFrame::MouseListener,
                  nsIDOMEventListener)
