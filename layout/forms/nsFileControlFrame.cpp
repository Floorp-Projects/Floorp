/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFileControlFrame.h"

#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"
#include "nsIFilePicker.h"
#include "nsIDOMMouseEvent.h"
#include "nsINodeInfo.h"
#include "nsIDOMEventTarget.h"
#include "nsIFile.h"
#include "nsHTMLInputElement.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsEventListenerManager.h"

#include "nsInterfaceHashtable.h"
#include "nsURIHashKey.h"
#include "nsNetCID.h"
#include "nsWeakReference.h"
#include "nsIVariant.h"
#include "mozilla/Services.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDOMFile.h"
#include "nsEventStates.h"
#include "nsTextControlFrame.h"

#include "nsIDOMDOMStringList.h"
#include "nsIDOMDragEvent.h"
#include "nsContentList.h"
#include "nsIDOMMutationEvent.h"

using namespace mozilla;

#define SYNC_TEXT 0x1
#define SYNC_BUTTON 0x2

nsIFrame*
NS_NewFileControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFileControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsFileControlFrame)

nsFileControlFrame::nsFileControlFrame(nsStyleContext* aContext):
  nsBlockFrame(aContext)
{
  AddStateBits(NS_BLOCK_FLOAT_MGR);
}


void
nsFileControlFrame::Init(nsIContent* aContent,
                         nsIFrame*   aParent,
                         nsIFrame*   aPrevInFlow)
{
  nsBlockFrame::Init(aContent, aParent, aPrevInFlow);

  mMouseListener = new BrowseMouseListener(this);
}

void
nsFileControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  ENSURE_TRUE(mContent);

  // Remove the drag events
  if (mContent) {
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("drop"),
                                        mMouseListener, false);
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("dragover"),
                                        mMouseListener, false);
  }

  // remove mMouseListener as a mouse event listener (bug 40533, bug 355931)

  if (mBrowse) {
    mBrowse->RemoveSystemEventListener(NS_LITERAL_STRING("click"),
                                       mMouseListener, false);
  }
  nsContentUtils::DestroyAnonymousContent(&mBrowse);

  if (mTextContent) {
    mTextContent->RemoveSystemEventListener(NS_LITERAL_STRING("click"),
                                            mMouseListener, false);
  }
  nsContentUtils::DestroyAnonymousContent(&mTextContent);

  mMouseListener->ForgetFrame();
  nsBlockFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsFileControlFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();
  nsCOMPtr<nsINodeInfo> nodeInfo;

  // Create and setup the file picking button.
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::input, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_NewHTMLElement(getter_AddRefs(mBrowse), nodeInfo.forget(),
                    dom::NOT_FROM_PARSER);
  // NOTE: SetNativeAnonymous() has to be called before setting any attribute.
  mBrowse->SetNativeAnonymous();
  mBrowse->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                   NS_LITERAL_STRING("button"), false);

  // Make sure access key and tab order for the element actually redirect to the
  // file picking button.
  nsCOMPtr<nsIDOMHTMLInputElement> fileContent = do_QueryInterface(mContent);
  nsCOMPtr<nsIDOMHTMLInputElement> browseControl = do_QueryInterface(mBrowse);

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
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::label, nullptr,
                                                 kNameSpaceID_XUL,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_TrustedNewXULElement(getter_AddRefs(mTextContent), nodeInfo.forget());
  // NOTE: SetNativeAnonymous() has to be called before setting any attribute.
  mTextContent->SetNativeAnonymous();
  mTextContent->SetAttr(kNameSpaceID_None, nsGkAtoms::crop,
                        NS_LITERAL_STRING("center"), false);

  // Update the displayed text to reflect the current element's value.
  nsAutoString value;
  nsHTMLInputElement::FromContent(mContent)->GetDisplayFileName(value);
  UpdateDisplayedValue(value, false);

  if (!aElements.AppendElement(mTextContent)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // We should be able to interact with the element by doing drag and drop or
  // clicking.
  mContent->AddSystemEventListener(NS_LITERAL_STRING("drop"),
                                   mMouseListener, false);
  mContent->AddSystemEventListener(NS_LITERAL_STRING("dragover"),
                                   mMouseListener, false);
  mBrowse->AddSystemEventListener(NS_LITERAL_STRING("click"),
                                  mMouseListener, false);
  mTextContent->AddSystemEventListener(NS_LITERAL_STRING("click"),
                                       mMouseListener, false);

  SyncDisabledState();

  return NS_OK;
}

void
nsFileControlFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                             uint32_t aFilter)
{
  aElements.MaybeAppendElement(mTextContent);
  aElements.MaybeAppendElement(mBrowse);
}

NS_QUERYFRAME_HEAD(nsFileControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

void 
nsFileControlFrame::SetFocus(bool aOn, bool aRepaint)
{
}

bool ShouldProcessMouseClick(nsIDOMEvent* aMouseEvent)
{
  // only allow the left button
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_TRUE(mouseEvent, false);
  bool defaultPrevented = false;
  aMouseEvent->GetPreventDefault(&defaultPrevented);
  if (defaultPrevented) {
    return false;
  }

  uint16_t whichButton;
  if (NS_FAILED(mouseEvent->GetButton(&whichButton)) || whichButton != 0) {
    return false;
  }

  int32_t clickCount;
  if (NS_FAILED(mouseEvent->GetDetail(&clickCount)) || clickCount > 1) {
    return false;
  }

  return true;
}

/**
 * This is called when we receive any registered events on the control.
 * We've only registered for drop, dragover and click events.
 */
NS_IMETHODIMP
nsFileControlFrame::BrowseMouseListener::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(mFrame, "We should have been unregistered");

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("click")) {
    if (!ShouldProcessMouseClick(aEvent))
      return NS_OK;
    
    nsHTMLInputElement* input =
      nsHTMLInputElement::FromContent(mFrame->GetContent());
    return input ? input->FireAsyncClickHandler() : NS_OK;
  }

  bool defaultPrevented = false;
  aEvent->GetPreventDefault(&defaultPrevented);
  if (defaultPrevented) {
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMDragEvent> dragEvent = do_QueryInterface(aEvent);
  if (!dragEvent || !IsValidDropData(dragEvent)) {
    return NS_OK;
  }

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

    nsHTMLInputElement* inputElement = nsHTMLInputElement::FromContent(content);
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
nsFileControlFrame::BrowseMouseListener::IsValidDropData(nsIDOMDragEvent* aEvent)
{
  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
  aEvent->GetDataTransfer(getter_AddRefs(dataTransfer));
  NS_ENSURE_TRUE(dataTransfer, false);

  nsCOMPtr<nsIDOMDOMStringList> types;
  dataTransfer->GetTypes(getter_AddRefs(types));
  NS_ENSURE_TRUE(types, false);

  // We only support dropping files onto a file upload control
  bool typeSupported;
  types->Contains(NS_LITERAL_STRING("Files"), &typeSupported);
  return typeSupported;
}

nscoord
nsFileControlFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  // Our min width is our pref width
  result = GetPrefWidth(aRenderingContext);
  return result;
}

void
nsFileControlFrame::SyncDisabledState()
{
  nsEventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    mBrowse->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, EmptyString(),
                     true);
  } else {
    mBrowse->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  }
}

NS_IMETHODIMP
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
nsFileControlFrame::ContentStatesChanged(nsEventStates aStates)
{
  if (aStates.HasState(NS_EVENT_STATE_DISABLED)) {
    nsContentUtils::AddScriptRunner(new SyncDisabledStateEvent(this));
  }
}

bool
nsFileControlFrame::IsLeaf() const
{
  return true;
}

#ifdef DEBUG
NS_IMETHODIMP
nsFileControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FileControl"), aResult);
}
#endif

void
nsFileControlFrame::UpdateDisplayedValue(const nsAString& aValue, bool aNotify)
{
  nsXPIDLString value;
  if (aValue.IsEmpty()) {
    if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
      nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                         "NoFilesSelected", value);
    } else {
      nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                         "NoFileSelected", value);
    }
  } else {
    value = aValue;
  }
  mTextContent->SetAttr(kNameSpaceID_None, nsGkAtoms::value, value, aNotify);
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

nsresult
nsFileControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  aValue.Truncate();  // initialize out param

  if (nsGkAtoms::value == aName) {
    nsHTMLInputElement* inputElement =
      nsHTMLInputElement::FromContent(mContent);

    if (inputElement) {
      inputElement->GetDisplayFileName(aValue);
    }
  }
  return NS_OK;
}

void
nsFileControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  // box-shadow
  if (StyleBorder()->mBoxShadow) {
    aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
      nsDisplayBoxShadowOuter(aBuilder, this));
  }

  // Our background is inherited to the text input, and we don't really want to
  // paint it or out padding and borders (which we never have anyway, per
  // styles in forms.css) -- doing it just makes us look ugly in some cases and
  // has no effect in others.
  nsDisplayListCollection tempList;
  nsBlockFrame::BuildDisplayList(aBuilder, aDirtyRect, tempList);

  tempList.BorderBackground()->DeleteAll();

  // Clip height only
  nsRect clipRect(aBuilder->ToReferenceFrame(this), GetSize());
  clipRect.width = GetVisualOverflowRect().XMost();
  nscoord radii[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  OverflowClip(aBuilder, tempList, aLists, clipRect, radii);

  // Disabled file controls don't pass mouse events to their children, so we
  // put an invisible item in the display list above the children
  // just to catch events
  nsEventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED) && IsVisibleForPainting(aBuilder)) {
    aLists.Content()->AppendNewToTop(
      new (aBuilder) nsDisplayEventReceiver(aBuilder, this));
  }

  DisplaySelectionOverlay(aBuilder, aLists.Content());
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

NS_IMPL_ISUPPORTS1(nsFileControlFrame::MouseListener,
                   nsIDOMEventListener)
