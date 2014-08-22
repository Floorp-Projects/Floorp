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
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/DOMStringList.h"
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
  nsContentUtils::DestroyAnonymousContent(&mBrowse);

  mMouseListener->ForgetFrame();
  nsBlockFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsFileControlFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  nsCOMPtr<nsIDocument> doc = mContent->GetComposedDoc();

  // Create and setup the file picking button.
  mBrowse = doc->CreateHTMLElement(nsGkAtoms::button);
  // NOTE: SetIsNativeAnonymousRoot() has to be called before setting any
  // attribute.
  mBrowse->SetIsNativeAnonymousRoot();
  mBrowse->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                   NS_LITERAL_STRING("button"), false);

  // Set the file picking button text depending on the current locale.
  nsXPIDLString buttonTxt;
  nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                     "Browse", buttonTxt);

  // Set the browse button text. It's a bit of a pain to do because we want to
  // make sure we are not notifying.
  nsRefPtr<nsTextNode> textContent =
    new nsTextNode(mBrowse->NodeInfo()->NodeInfoManager());

  textContent->SetText(buttonTxt, false);

  nsresult rv = mBrowse->AppendChildTo(textContent, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure access key and tab order for the element actually redirect to the
  // file picking button.
  nsRefPtr<HTMLInputElement> fileContent = HTMLInputElement::FromContentOrNull(mContent);
  nsRefPtr<HTMLButtonElement> browseControl = HTMLButtonElement::FromContentOrNull(mBrowse);

  nsAutoString accessKey;
  fileContent->GetAccessKey(accessKey);
  browseControl->SetAccessKey(accessKey);

  int32_t tabIndex;
  fileContent->GetTabIndex(&tabIndex);
  browseControl->SetTabIndex(tabIndex);

  if (!aElements.AppendElement(mBrowse)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Create and setup the text showing the selected files.
  nsRefPtr<NodeInfo> nodeInfo;
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
  if (mBrowse) {
    aElements.AppendElement(mBrowse);
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
  if (!dragEvent || !IsValidDropData(dragEvent)) {
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

    nsIContent* content = mFrame->GetContent();
    NS_ASSERTION(content, "The frame has no content???");

    HTMLInputElement* inputElement = HTMLInputElement::FromContent(content);
    NS_ASSERTION(inputElement, "No input element for this file upload control frame!");

    nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
    dragEvent->GetDataTransfer(getter_AddRefs(dataTransfer));

    nsCOMPtr<nsIDOMFileList> fileList;
    dataTransfer->GetFiles(getter_AddRefs(fileList));

    inputElement->SetFiles(fileList, true);
    nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(), content,
                                         NS_LITERAL_STRING("change"), true,
                                         false);
  }

  return NS_OK;
}

/* static */ bool
nsFileControlFrame::DnDListener::IsValidDropData(nsIDOMDragEvent* aEvent)
{
  nsCOMPtr<nsIDOMDataTransfer> domDataTransfer;
  aEvent->GetDataTransfer(getter_AddRefs(domDataTransfer));
  nsCOMPtr<DataTransfer> dataTransfer = do_QueryInterface(domDataTransfer);
  NS_ENSURE_TRUE(dataTransfer, false);

  // We only support dropping files onto a file upload control
  nsRefPtr<DOMStringList> types = dataTransfer->Types();
  return types->Contains(NS_LITERAL_STRING("Files"));
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
    mBrowse->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, EmptyString(),
                     true);
  } else {
    mBrowse->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  }
}

nsresult
nsFileControlFrame::AttributeChanged(int32_t  aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::tabindex) {
    if (aModType == nsIDOMMutationEvent::REMOVAL) {
      mBrowse->UnsetAttr(aNameSpaceID, aAttribute, true);
    } else {
      nsAutoString value;
      mContent->GetAttr(aNameSpaceID, aAttribute, value);
      mBrowse->SetAttr(aNameSpaceID, aAttribute, value, true);
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
