/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFileControlFrame.h"

#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/dom/HTMLButtonElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsUnicodeProperties.h"
#include "mozilla/EventStates.h"
#include "nsTextNode.h"
#include "nsTextFrame.h"

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame* NS_NewFileControlFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsFileControlFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsFileControlFrame)

nsFileControlFrame::nsFileControlFrame(ComputedStyle* aStyle,
                                       nsPresContext* aPresContext)
    : nsBlockFrame(aStyle, aPresContext, kClassID) {
  AddStateBits(NS_BLOCK_FLOAT_MGR);
}

void nsFileControlFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                              nsIFrame* aPrevInFlow) {
  nsBlockFrame::Init(aContent, aParent, aPrevInFlow);

  mMouseListener = new DnDListener(this);
}

bool nsFileControlFrame::CropTextToWidth(gfxContext& aRenderingContext,
                                         const nsIFrame* aFrame, nscoord aWidth,
                                         nsString& aText) {
  if (aText.IsEmpty()) {
    return false;
  }

  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForFrame(aFrame, 1.0f);

  // see if the text will completely fit in the width given
  nscoord textWidth = nsLayoutUtils::AppUnitWidthOfStringBidi(
      aText, aFrame, *fm, aRenderingContext);
  if (textWidth <= aWidth) {
    return false;
  }

  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
  const nsDependentString& kEllipsis = nsContentUtils::GetLocalizedEllipsis();

  // see if the width is even smaller than the ellipsis
  fm->SetTextRunRTL(false);
  textWidth = nsLayoutUtils::AppUnitWidthOfString(kEllipsis, *fm, drawTarget);
  if (textWidth >= aWidth) {
    aText = kEllipsis;
    return true;
  }

  // determine how much of the string will fit in the max width
  nscoord totalWidth = textWidth;
  using mozilla::unicode::ClusterIterator;
  using mozilla::unicode::ClusterReverseIterator;
  ClusterIterator leftIter(aText.Data(), aText.Length());
  ClusterReverseIterator rightIter(aText.Data(), aText.Length());
  const char16_t* leftPos = leftIter;
  const char16_t* rightPos = rightIter;
  const char16_t* pos;
  ptrdiff_t length;
  nsAutoString leftString, rightString;

  while (leftPos < rightPos) {
    leftIter.Next();
    pos = leftIter;
    length = pos - leftPos;
    textWidth =
        nsLayoutUtils::AppUnitWidthOfString(leftPos, length, *fm, drawTarget);
    if (totalWidth + textWidth > aWidth) {
      break;
    }

    leftString.Append(leftPos, length);
    leftPos = pos;
    totalWidth += textWidth;

    if (leftPos >= rightPos) {
      break;
    }

    rightIter.Next();
    pos = rightIter;
    length = rightPos - pos;
    textWidth =
        nsLayoutUtils::AppUnitWidthOfString(pos, length, *fm, drawTarget);
    if (totalWidth + textWidth > aWidth) {
      break;
    }

    rightString.Insert(pos, 0, length);
    rightPos = pos;
    totalWidth += textWidth;
  }

  aText = leftString + kEllipsis + rightString;
  return true;
}

void nsFileControlFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aMetrics,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
  // Restore the uncropped filename.
  nsAutoString filename;
  HTMLInputElement::FromNode(mContent)->GetDisplayFileName(filename);

  bool done = false;
  while (true) {
    UpdateDisplayedValue(filename, false);  // update the text node
    AddStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
    LinesBegin()->MarkDirty();
    nsBlockFrame::Reflow(aPresContext, aMetrics, aReflowInput, aStatus);
    if (done) {
      break;
    }
    nscoord lineISize = LinesBegin()->ISize();
    const auto cbWM = aMetrics.GetWritingMode();
    const auto wm = GetWritingMode();
    nscoord iSize =
        wm.IsOrthogonalTo(cbWM) ? aMetrics.BSize(cbWM) : aMetrics.ISize(cbWM);
    auto bp = GetLogicalUsedBorderAndPadding(wm);
    nscoord contentISize = iSize - bp.IStartEnd(wm);
    if (lineISize > contentISize) {
      // The filename overflows - crop it and reflow again (once).
      // NOTE: the label frame might have bidi-continuations
      auto* labelFrame = mTextContent->GetPrimaryFrame();
      nscoord labelBP =
          labelFrame->GetLogicalUsedBorderAndPadding(wm).IStartEnd(wm);
      auto* lastLabelCont = labelFrame->LastContinuation();
      if (lastLabelCont != labelFrame) {
        labelBP +=
            lastLabelCont->GetLogicalUsedBorderAndPadding(wm).IStartEnd(wm);
      }
      auto* buttonFrame = mBrowseFilesOrDirs->GetPrimaryFrame();
      nscoord availableISizeForLabel =
          contentISize - buttonFrame->ISize(wm) -
          buttonFrame->GetLogicalUsedMargin(wm).IStartEnd(wm);
      if (CropTextToWidth(*aReflowInput.mRenderingContext, labelFrame,
                          availableISizeForLabel - labelBP, filename)) {
        nsBlockFrame::DidReflow(aPresContext, &aReflowInput);
        aStatus.Reset();
        labelFrame->MarkSubtreeDirty();
        labelFrame->AddStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
        mCachedMinISize = NS_INTRINSIC_ISIZE_UNKNOWN;
        mCachedPrefISize = NS_INTRINSIC_ISIZE_UNKNOWN;
        done = true;
        continue;
      }
    }
    break;
  }
}

void nsFileControlFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                     PostDestroyData& aPostDestroyData) {
  NS_ENSURE_TRUE_VOID(mContent);

  // Remove the events.
  if (mContent) {
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("drop"),
                                        mMouseListener, false);
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("dragover"),
                                        mMouseListener, false);
  }

  aPostDestroyData.AddAnonymousContent(mTextContent.forget());
  aPostDestroyData.AddAnonymousContent(mBrowseFilesOrDirs.forget());

  mMouseListener->ForgetFrame();
  nsBlockFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

static already_AddRefed<Element> MakeAnonButton(Document* aDoc,
                                                const char* labelKey,
                                                HTMLInputElement* aInputElement,
                                                const nsAString& aAccessKey) {
  RefPtr<Element> button = aDoc->CreateHTMLElement(nsGkAtoms::button);
  // NOTE: SetIsNativeAnonymousRoot() has to be called before setting any
  // attribute.
  button->SetIsNativeAnonymousRoot();

  // Set the file picking button text depending on the current locale.
  nsAutoString buttonTxt;
  nsContentUtils::GetMaybeLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                          labelKey, aDoc, buttonTxt);

  // Set the browse button text. It's a bit of a pain to do because we want to
  // make sure we are not notifying.
  RefPtr<nsTextNode> textContent = new (button->NodeInfo()->NodeInfoManager())
      nsTextNode(button->NodeInfo()->NodeInfoManager());

  textContent->SetText(buttonTxt, false);

  nsresult rv = button->AppendChildTo(textContent, false);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  // Make sure access key and tab order for the element actually redirect to the
  // file picking button.
  RefPtr<HTMLButtonElement> buttonElement =
      HTMLButtonElement::FromNodeOrNull(button);

  if (!aAccessKey.IsEmpty()) {
    buttonElement->SetAccessKey(aAccessKey, IgnoreErrors());
  }

  // We allow tabbing over the input itself, not the button.
  buttonElement->SetTabIndex(-1, IgnoreErrors());
  return button.forget();
}

nsresult nsFileControlFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  nsCOMPtr<Document> doc = mContent->GetComposedDoc();

  RefPtr<HTMLInputElement> fileContent =
      HTMLInputElement::FromNodeOrNull(mContent);

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
  mTextContent = doc->CreateHTMLElement(nsGkAtoms::label);
  // NOTE: SetIsNativeAnonymousRoot() has to be called before setting any
  // attribute.
  mTextContent->SetIsNativeAnonymousRoot();
  RefPtr<nsTextNode> text =
      new (doc->NodeInfoManager()) nsTextNode(doc->NodeInfoManager());
  mTextContent->AppendChildTo(text, false);

  // Update the displayed text to reflect the current element's value.
  nsAutoString value;
  fileContent->GetDisplayFileName(value);
  UpdateDisplayedValue(value, false);

  aElements.AppendElement(mTextContent);

  // We should be able to interact with the element by doing drag and drop.
  mContent->AddSystemEventListener(NS_LITERAL_STRING("drop"), mMouseListener,
                                   false);
  mContent->AddSystemEventListener(NS_LITERAL_STRING("dragover"),
                                   mMouseListener, false);

  SyncDisabledState();

  return NS_OK;
}

void nsFileControlFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
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

void nsFileControlFrame::SetFocus(bool aOn, bool aRepaint) {}

static void AppendBlobImplAsDirectory(nsTArray<OwningFileOrDirectory>& aArray,
                                      BlobImpl* aBlobImpl,
                                      nsIContent* aContent) {
  MOZ_ASSERT(aBlobImpl);
  MOZ_ASSERT(aBlobImpl->IsDirectory());

  nsAutoString fullpath;
  ErrorResult err;
  aBlobImpl->GetMozFullPath(fullpath, SystemCallerGuarantee(), err);
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

  RefPtr<Directory> directory = Directory::Create(inner->AsGlobal(), file);
  MOZ_ASSERT(directory);

  OwningFileOrDirectory* element = aArray.AppendElement();
  element->SetAsDirectory() = directory;
}

/**
 * This is called when we receive a drop or a dragover.
 */
NS_IMETHODIMP
nsFileControlFrame::DnDListener::HandleEvent(Event* aEvent) {
  NS_ASSERTION(mFrame, "We should have been unregistered");

  if (aEvent->DefaultPrevented()) {
    return NS_OK;
  }

  DragEvent* dragEvent = aEvent->AsDragEvent();
  if (!dragEvent) {
    return NS_OK;
  }

  RefPtr<DataTransfer> dataTransfer = dragEvent->GetDataTransfer();
  if (!IsValidDropData(dataTransfer)) {
    return NS_OK;
  }

  RefPtr<HTMLInputElement> inputElement =
      HTMLInputElement::FromNode(mFrame->GetContent());
  bool supportsMultiple =
      inputElement->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple);
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

    RefPtr<FileList> fileList =
        dataTransfer->GetFiles(*nsContentUtils::GetSystemPrincipal());

    RefPtr<BlobImpl> webkitDir;
    nsresult rv =
        GetBlobImplForWebkitDirectory(fileList, getter_AddRefs(webkitDir));
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsTArray<OwningFileOrDirectory> array;
    if (webkitDir) {
      AppendBlobImplAsDirectory(array, webkitDir, inputElement);
      inputElement->MozSetDndFilesAndDirectories(array);
    } else {
      bool blinkFileSystemEnabled =
          StaticPrefs::dom_webkitBlink_filesystem_enabled();
      bool dirPickerEnabled = StaticPrefs::dom_input_dirpicker();
      if (blinkFileSystemEnabled || dirPickerEnabled) {
        FileList* files = static_cast<FileList*>(fileList.get());
        if (files) {
          for (uint32_t i = 0; i < files->Length(); ++i) {
            File* file = files->Item(i);
            if (file) {
              if (file->Impl() && file->Impl()->IsDirectory()) {
                AppendBlobImplAsDirectory(array, file->Impl(), inputElement);
              } else {
                OwningFileOrDirectory* element = array.AppendElement();
                element->SetAsFile() = file;
              }
            }
          }
        }
      }

      // Entries API.
      if (blinkFileSystemEnabled) {
        // This is rather ugly. Pass the directories as Files using SetFiles,
        // but then if blink filesystem API is enabled, it wants
        // FileOrDirectory array.
        inputElement->SetFiles(fileList, true);
        inputElement->UpdateEntries(array);
      }
      // Directory Upload API
      else if (dirPickerEnabled) {
        inputElement->SetFilesOrDirectories(array, true);
      }
      // Normal DnD
      else {
        inputElement->SetFiles(fileList, true);
      }

      RefPtr<TextEditor> textEditor;
      DebugOnly<nsresult> rvIgnored =
          nsContentUtils::DispatchInputEvent(inputElement);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "Failed to dispatch input event");
      nsContentUtils::DispatchTrustedEvent(
          inputElement->OwnerDoc(), static_cast<nsINode*>(inputElement),
          NS_LITERAL_STRING("change"), CanBubble::eYes, Cancelable::eNo);
    }
  }

  return NS_OK;
}

nsresult nsFileControlFrame::DnDListener::GetBlobImplForWebkitDirectory(
    FileList* aFileList, BlobImpl** aBlobImpl) {
  *aBlobImpl = nullptr;

  HTMLInputElement* inputElement =
      HTMLInputElement::FromNode(mFrame->GetContent());
  bool webkitDirPicker =
      StaticPrefs::dom_webkitBlink_dirPicker_enabled() &&
      inputElement->HasAttr(kNameSpaceID_None, nsGkAtoms::webkitdirectory);
  if (!webkitDirPicker) {
    return NS_OK;
  }

  if (!aFileList) {
    return NS_ERROR_FAILURE;
  }

  // webkitdirectory doesn't care about the length of the file list but
  // only about the first item on it.
  uint32_t len = aFileList->Length();
  if (len) {
    File* file = aFileList->Item(0);
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

bool nsFileControlFrame::DnDListener::IsValidDropData(
    DataTransfer* aDataTransfer) {
  if (!aDataTransfer) {
    return false;
  }

  // We only support dropping files onto a file upload control
  return aDataTransfer->HasFile();
}

bool nsFileControlFrame::DnDListener::CanDropTheseFiles(
    DataTransfer* aDataTransfer, bool aSupportsMultiple) {
  RefPtr<FileList> fileList =
      aDataTransfer->GetFiles(*nsContentUtils::GetSystemPrincipal());

  RefPtr<BlobImpl> webkitDir;
  nsresult rv =
      GetBlobImplForWebkitDirectory(fileList, getter_AddRefs(webkitDir));
  // Just check if either there isn't webkitdirectory attribute, or
  // fileList has a directory which can be dropped to the element.
  // No need to use webkitDir for anything here.
  NS_ENSURE_SUCCESS(rv, false);

  uint32_t listLength = 0;
  if (fileList) {
    listLength = fileList->Length();
  }
  return listLength <= 1 || aSupportsMultiple;
}

nscoord nsFileControlFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  // Our min inline size is our pref inline size
  result = GetPrefISize(aRenderingContext);
  return result;
}

nscoord nsFileControlFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  // Make sure we measure with the uncropped filename.
  if (mCachedPrefISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    nsAutoString filename;
    HTMLInputElement::FromNode(mContent)->GetDisplayFileName(filename);
    UpdateDisplayedValue(filename, false);
  }

  result = nsBlockFrame::GetPrefISize(aRenderingContext);
  return result;
}

void nsFileControlFrame::SyncDisabledState() {
  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    mBrowseFilesOrDirs->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled,
                                EmptyString(), true);
  } else {
    mBrowseFilesOrDirs->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  }
}

nsresult nsFileControlFrame::AttributeChanged(int32_t aNameSpaceID,
                                              nsAtom* aAttribute,
                                              int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::tabindex) {
    if (aModType == MutationEvent_Binding::REMOVAL) {
      mBrowseFilesOrDirs->UnsetAttr(aNameSpaceID, aAttribute, true);
    } else {
      nsAutoString value;
      mContent->AsElement()->GetAttr(aNameSpaceID, aAttribute, value);
      mBrowseFilesOrDirs->SetAttr(aNameSpaceID, aAttribute, value, true);
    }
  }

  return nsBlockFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

void nsFileControlFrame::ContentStatesChanged(EventStates aStates) {
  if (aStates.HasState(NS_EVENT_STATE_DISABLED)) {
    nsContentUtils::AddScriptRunner(new SyncDisabledStateEvent(this));
  }
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsFileControlFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("FileControl"), aResult);
}
#endif

void nsFileControlFrame::UpdateDisplayedValue(const nsAString& aValue,
                                              bool aNotify) {
  auto* text = Text::FromNode(mTextContent->GetFirstChild());
  uint32_t oldLength = aNotify ? 0 : text->TextLength();
  text->SetText(aValue, aNotify);
  if (!aNotify) {
    // We can't notify during Reflow so we need to tell the text frame
    // about the text content change we just did.
    if (auto* textFrame = static_cast<nsTextFrame*>(text->GetPrimaryFrame())) {
      textFrame->NotifyNativeAnonymousTextnodeChange(oldLength);
    }
    nsBlockFrame* label = do_QueryFrame(mTextContent->GetPrimaryFrame());
    if (label && label->LinesBegin() != label->LinesEnd()) {
      label->AddStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
      label->LinesBegin()->MarkDirty();
    }
  }
}

nsresult nsFileControlFrame::SetFormProperty(nsAtom* aName,
                                             const nsAString& aValue) {
  if (nsGkAtoms::value == aName) {
    UpdateDisplayedValue(aValue, true);
  }
  return NS_OK;
}

void nsFileControlFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  BuildDisplayListForInline(aBuilder, aLists);
}

#ifdef ACCESSIBILITY
a11y::AccType nsFileControlFrame::AccessibleType() {
  return a11y::eHTMLFileInputType;
}
#endif

////////////////////////////////////////////////////////////
// Mouse listener implementation

NS_IMPL_ISUPPORTS(nsFileControlFrame::MouseListener, nsIDOMEventListener)
