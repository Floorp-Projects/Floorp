/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLFormElement.h"

#include "jsapi.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/HTMLFormControlsCollection.h"
#include "mozilla/dom/HTMLFormElementBinding.h"
#include "mozilla/Move.h"
#include "nsIHTMLDocument.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsIFormControlFrame.h"
#include "nsError.h"
#include "nsContentUtils.h"
#include "nsInterfaceHashtable.h"
#include "nsContentList.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsIMutableArray.h"
#include "mozilla/BinarySearch.h"
#include "nsQueryObject.h"

// form submission
#include "HTMLFormSubmissionConstants.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/Telemetry.h"
#include "nsIFormSubmitObserver.h"
#include "nsIObserverService.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsISimpleEnumerator.h"
#include "nsRange.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIPrompt.h"
#include "nsISecurityUITelemetry.h"
#include "nsIStringBundle.h"

// radio buttons
#include "mozilla/dom/HTMLInputElement.h"
#include "nsIRadioVisitor.h"
#include "RadioNodeList.h"

#include "nsLayoutUtils.h"

#include "mozAutoDocUpdate.h"
#include "nsIHTMLCollection.h"

#include "nsIConstraintValidation.h"

#include "nsSandboxFlags.h"

#include "nsIContentSecurityPolicy.h"

// images
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLButtonElement.h"

// construction, destruction
NS_IMPL_NS_NEW_HTML_ELEMENT(Form)

namespace mozilla {
namespace dom {

static const uint8_t NS_FORM_AUTOCOMPLETE_ON  = 1;
static const uint8_t NS_FORM_AUTOCOMPLETE_OFF = 0;

static const nsAttrValue::EnumTable kFormAutocompleteTable[] = {
  { "on",  NS_FORM_AUTOCOMPLETE_ON },
  { "off", NS_FORM_AUTOCOMPLETE_OFF },
  { nullptr, 0 }
};
// Default autocomplete value is 'on'.
static const nsAttrValue::EnumTable* kFormDefaultAutocomplete = &kFormAutocompleteTable[0];

bool HTMLFormElement::gFirstFormSubmitted = false;
bool HTMLFormElement::gPasswordManagerInitialized = false;

HTMLFormElement::HTMLFormElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mControls(new HTMLFormControlsCollection(this)),
    mSelectedRadioButtons(2),
    mRequiredRadioButtonCounts(2),
    mValueMissingRadioGroups(2),
    mPendingSubmission(nullptr),
    mSubmittingRequest(nullptr),
    mDefaultSubmitElement(nullptr),
    mFirstSubmitInElements(nullptr),
    mFirstSubmitNotInElements(nullptr),
    mImageNameLookupTable(FORM_CONTROL_LIST_HASHTABLE_LENGTH),
    mPastNameLookupTable(FORM_CONTROL_LIST_HASHTABLE_LENGTH),
    mSubmitPopupState(openAbused),
    mInvalidElementsCount(0),
    mGeneratingSubmit(false),
    mGeneratingReset(false),
    mIsSubmitting(false),
    mDeferSubmission(false),
    mNotifiedObservers(false),
    mNotifiedObserversResult(false),
    mSubmitInitiatedFromUserInput(false),
    mEverTriedInvalidSubmit(false)
{
  // We start out valid.
  AddStatesSilently(NS_EVENT_STATE_VALID);
}

HTMLFormElement::~HTMLFormElement()
{
  if (mControls) {
    mControls->DropFormReference();
  }

  Clear();
}

// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLFormElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLFormElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mControls)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mImageNameLookupTable)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPastNameLookupTable)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelectedRadioButtons)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLFormElement,
                                                nsGenericHTMLElement)
  tmp->Clear();
  tmp->mExpandoAndGeneration.OwnerUnlinked();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLFormElement,
                                             nsGenericHTMLElement,
                                             nsIForm,
                                             nsIWebProgressListener,
                                             nsIRadioGroupContainer)

// EventTarget
void
HTMLFormElement::AsyncEventRunning(AsyncEventDispatcher* aEvent)
{
  if (mFormPasswordEventDispatcher == aEvent) {
    mFormPasswordEventDispatcher = nullptr;
  }
}

NS_IMPL_ELEMENT_CLONE(HTMLFormElement)

nsIHTMLCollection*
HTMLFormElement::Elements()
{
  return mControls;
}

nsresult
HTMLFormElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                               const nsAttrValueOrString* aValue, bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::action || aName == nsGkAtoms::target) {
      if (mPendingSubmission) {
        // aha, there is a pending submission that means we're in
        // the script and we need to flush it. let's tell it
        // that the event was ignored to force the flush.
        // the second argument is not playing a role at all.
        FlushPendingSubmission();
      }
      // Don't forget we've notified the password manager already if the
      // page sets the action/target in the during submit. (bug 343182)
      bool notifiedObservers = mNotifiedObservers;
      ForgetCurrentSubmission();
      mNotifiedObservers = notifiedObservers;
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNamespaceID, aName, aValue,
                                             aNotify);
}

nsresult
HTMLFormElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                              const nsAttrValue* aValue,
                              const nsAttrValue* aOldValue,
                              nsIPrincipal* aSubjectPrincipal,
                              bool aNotify)
{
  if (aName == nsGkAtoms::novalidate && aNameSpaceID == kNameSpaceID_None) {
    // Update all form elements states because they might be [no longer]
    // affected by :-moz-ui-valid or :-moz-ui-invalid.
    for (uint32_t i = 0, length = mControls->mElements.Length();
         i < length; ++i) {
      mControls->mElements[i]->UpdateState(true);
    }

    for (uint32_t i = 0, length = mControls->mNotInElements.Length();
         i < length; ++i) {
      mControls->mNotInElements[i]->UpdateState(true);
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aOldValue, aSubjectPrincipal, aNotify);
}

void HTMLFormElement::GetAutocomplete(nsAString& aValue)
{
  GetEnumAttr(nsGkAtoms::autocomplete, kFormDefaultAutocomplete->tag, aValue);
}

void HTMLFormElement::GetEnctype(nsAString& aValue)
{
  GetEnumAttr(nsGkAtoms::enctype, kFormDefaultEnctype->tag, aValue);
}

void HTMLFormElement::GetMethod(nsAString& aValue)
{
  GetEnumAttr(nsGkAtoms::method, kFormDefaultMethod->tag, aValue);
}

void
HTMLFormElement::Submit(ErrorResult& aRv)
{
  // Send the submit event
  if (mPendingSubmission) {
    // aha, we have a pending submission that was not flushed
    // (this happens when form.submit() is called twice)
    // we have to delete it and build a new one since values
    // might have changed inbetween (we emulate IE here, that's all)
    mPendingSubmission = nullptr;
  }

  aRv = DoSubmitOrReset(nullptr, eFormSubmit);
}

void
HTMLFormElement::Reset()
{
  InternalFormEvent event(true, eFormReset);
  EventDispatcher::Dispatch(static_cast<nsIContent*>(this), nullptr, &event);
}

bool
HTMLFormElement::ParseAttribute(int32_t aNamespaceID,
                                nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::method) {
      return aResult.ParseEnumValue(aValue, kFormMethodTable, false);
    }
    if (aAttribute == nsGkAtoms::enctype) {
      return aResult.ParseEnumValue(aValue, kFormEnctypeTable, false);
    }
    if (aAttribute == nsGkAtoms::autocomplete) {
      return aResult.ParseEnumValue(aValue, kFormAutocompleteTable, false);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

nsresult
HTMLFormElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(aDocument));
  if (htmlDoc) {
    htmlDoc->AddedForm();
  }

  return rv;
}

template<typename T>
static void
MarkOrphans(const nsTArray<T*>& aArray)
{
  uint32_t length = aArray.Length();
  for (uint32_t i = 0; i < length; ++i) {
    aArray[i]->SetFlags(MAYBE_ORPHAN_FORM_ELEMENT);
  }
}

static void
CollectOrphans(nsINode* aRemovalRoot,
               const nsTArray<nsGenericHTMLFormElement*>& aArray
#ifdef DEBUG
               , HTMLFormElement* aThisForm
#endif
               )
{
  // Put a script blocker around all the notifications we're about to do.
  nsAutoScriptBlocker scriptBlocker;

  // Walk backwards so that if we remove elements we can just keep iterating
  uint32_t length = aArray.Length();
  for (uint32_t i = length; i > 0; --i) {
    nsGenericHTMLFormElement* node = aArray[i-1];

    // Now if MAYBE_ORPHAN_FORM_ELEMENT is not set, that would mean that the
    // node is in fact a descendant of the form and hence should stay in the
    // form.  If it _is_ set, then we need to check whether the node is a
    // descendant of aRemovalRoot.  If it is, we leave it in the form.
#ifdef DEBUG
    bool removed = false;
#endif
    if (node->HasFlag(MAYBE_ORPHAN_FORM_ELEMENT)) {
      node->UnsetFlags(MAYBE_ORPHAN_FORM_ELEMENT);
      if (!nsContentUtils::ContentIsDescendantOf(node, aRemovalRoot)) {
        node->ClearForm(true, false);

        // When a form control loses its form owner, its state can change.
        node->UpdateState(true);
#ifdef DEBUG
        removed = true;
#endif
      }
    }

#ifdef DEBUG
    if (!removed) {
      HTMLFormElement* form = node->GetForm();
      NS_ASSERTION(form == aThisForm, "How did that happen?");
    }
#endif /* DEBUG */
  }
}

static void
CollectOrphans(nsINode* aRemovalRoot,
               const nsTArray<HTMLImageElement*>& aArray
#ifdef DEBUG
               , HTMLFormElement* aThisForm
#endif
               )
{
  // Walk backwards so that if we remove elements we can just keep iterating
  uint32_t length = aArray.Length();
  for (uint32_t i = length; i > 0; --i) {
    HTMLImageElement* node = aArray[i-1];

    // Now if MAYBE_ORPHAN_FORM_ELEMENT is not set, that would mean that the
    // node is in fact a descendant of the form and hence should stay in the
    // form.  If it _is_ set, then we need to check whether the node is a
    // descendant of aRemovalRoot.  If it is, we leave it in the form.
#ifdef DEBUG
    bool removed = false;
#endif
    if (node->HasFlag(MAYBE_ORPHAN_FORM_ELEMENT)) {
      node->UnsetFlags(MAYBE_ORPHAN_FORM_ELEMENT);
      if (!nsContentUtils::ContentIsDescendantOf(node, aRemovalRoot)) {
        node->ClearForm(true);

#ifdef DEBUG
        removed = true;
#endif
      }
    }

#ifdef DEBUG
    if (!removed) {
      HTMLFormElement* form = node->GetForm();
      NS_ASSERTION(form == aThisForm, "How did that happen?");
    }
#endif /* DEBUG */
  }
}

void
HTMLFormElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIHTMLDocument> oldDocument = do_QueryInterface(GetUncomposedDoc());

  // Mark all of our controls as maybe being orphans
  MarkOrphans(mControls->mElements);
  MarkOrphans(mControls->mNotInElements);
  MarkOrphans(mImageElements);

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  nsINode* ancestor = this;
  nsINode* cur;
  do {
    cur = ancestor->GetParentNode();
    if (!cur) {
      break;
    }
    ancestor = cur;
  } while (1);

  CollectOrphans(ancestor, mControls->mElements
#ifdef DEBUG
                 , this
#endif
                 );
  CollectOrphans(ancestor, mControls->mNotInElements
#ifdef DEBUG
                 , this
#endif
                 );
  CollectOrphans(ancestor, mImageElements
#ifdef DEBUG
                 , this
#endif
                 );

  if (oldDocument) {
    oldDocument->RemovedForm();
  }
  ForgetCurrentSubmission();
}

void
HTMLFormElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mWantsWillHandleEvent = true;
  if (aVisitor.mEvent->mOriginalTarget == static_cast<nsIContent*>(this)) {
    uint32_t msg = aVisitor.mEvent->mMessage;
    if (msg == eFormSubmit) {
      if (mGeneratingSubmit) {
        aVisitor.mCanHandle = false;
        return;
      }
      mGeneratingSubmit = true;

      // let the form know that it needs to defer the submission,
      // that means that if there are scripted submissions, the
      // latest one will be deferred until after the exit point of the handler.
      mDeferSubmission = true;
    } else if (msg == eFormReset) {
      if (mGeneratingReset) {
        aVisitor.mCanHandle = false;
        return;
      }
      mGeneratingReset = true;
    }
  }
  nsGenericHTMLElement::GetEventTargetParent(aVisitor);
}

void
HTMLFormElement::WillHandleEvent(EventChainPostVisitor& aVisitor)
{
  // If this is the bubble stage and there is a nested form below us which
  // received a submit event we do *not* want to handle the submit event
  // for this form too.
  if ((aVisitor.mEvent->mMessage == eFormSubmit ||
       aVisitor.mEvent->mMessage == eFormReset) &&
      aVisitor.mEvent->mFlags.mInBubblingPhase &&
      aVisitor.mEvent->mOriginalTarget != static_cast<nsIContent*>(this)) {
    aVisitor.mEvent->StopPropagation();
  }
}

nsresult
HTMLFormElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  if (aVisitor.mEvent->mOriginalTarget == static_cast<nsIContent*>(this)) {
    EventMessage msg = aVisitor.mEvent->mMessage;
    if (msg == eFormSubmit) {
      // let the form know not to defer subsequent submissions
      mDeferSubmission = false;
    }

    if (aVisitor.mEventStatus == nsEventStatus_eIgnore) {
      switch (msg) {
        case eFormReset:
        case eFormSubmit: {
          if (mPendingSubmission && msg == eFormSubmit) {
            // tell the form to forget a possible pending submission.
            // the reason is that the script returned true (the event was
            // ignored) so if there is a stored submission, it will miss
            // the name/value of the submitting element, thus we need
            // to forget it and the form element will build a new one
            mPendingSubmission = nullptr;
          }
          DoSubmitOrReset(aVisitor.mEvent, msg);
          break;
        }
        default:
          break;
      }
    } else {
      if (msg == eFormSubmit) {
        // tell the form to flush a possible pending submission.
        // the reason is that the script returned false (the event was
        // not ignored) so if there is a stored submission, it needs to
        // be submitted immediatelly.
        FlushPendingSubmission();
      }
    }

    if (msg == eFormSubmit) {
      mGeneratingSubmit = false;
    } else if (msg == eFormReset) {
      mGeneratingReset = false;
    }
  }
  return NS_OK;
}

nsresult
HTMLFormElement::DoSubmitOrReset(WidgetEvent* aEvent,
                                 EventMessage aMessage)
{
  // Make sure the presentation is up-to-date
  nsIDocument* doc = GetComposedDoc();
  if (doc) {
    doc->FlushPendingNotifications(FlushType::ContentAndNotify);
  }

  // JBK Don't get form frames anymore - bug 34297

  // Submit or Reset the form
  if (eFormReset == aMessage) {
    return DoReset();
  }

  if (eFormSubmit == aMessage) {
    // Don't submit if we're not in a document or if we're in
    // a sandboxed frame and form submit is disabled.
    if (!doc || (doc->GetSandboxFlags() & SANDBOXED_FORMS)) {
      return NS_OK;
    }
    return DoSubmit(aEvent);
  }

  MOZ_ASSERT(false);
  return NS_OK;
}

nsresult
HTMLFormElement::DoReset()
{
  mEverTriedInvalidSubmit = false;
  // JBK walk the elements[] array instead of form frame controls - bug 34297
  uint32_t numElements = GetElementCount();
  for (uint32_t elementX = 0; elementX < numElements; ++elementX) {
    // Hold strong ref in case the reset does something weird
    nsCOMPtr<nsIFormControl> controlNode = GetElementAt(elementX);
    if (controlNode) {
      controlNode->Reset();
    }
  }

  return NS_OK;
}

#define NS_ENSURE_SUBMIT_SUCCESS(rv)                                          \
  if (NS_FAILED(rv)) {                                                        \
    ForgetCurrentSubmission();                                                \
    return rv;                                                                \
  }

nsresult
HTMLFormElement::DoSubmit(WidgetEvent* aEvent)
{
  NS_ASSERTION(GetComposedDoc(), "Should never get here without a current doc");

  if (mIsSubmitting) {
    NS_WARNING("Preventing double form submission");
    // XXX Should this return an error?
    return NS_OK;
  }

  // Mark us as submitting so that we don't try to submit again
  mIsSubmitting = true;
  NS_ASSERTION(!mWebProgress && !mSubmittingRequest, "Web progress / submitting request should not exist here!");

  nsAutoPtr<HTMLFormSubmission> submission;

  //
  // prepare the submission object
  //
  nsresult rv = BuildSubmission(getter_Transfers(submission), aEvent);
  if (NS_FAILED(rv)) {
    mIsSubmitting = false;
    return rv;
  }

  // XXXbz if the script global is that for an sXBL/XBL2 doc, it won't
  // be a window...
  nsPIDOMWindowOuter *window = OwnerDoc()->GetWindow();

  if (window) {
    mSubmitPopupState = window->GetPopupControlState();
  } else {
    mSubmitPopupState = openAbused;
  }

  mSubmitInitiatedFromUserInput = EventStateManager::IsHandlingUserInput();

  if(mDeferSubmission) {
    // we are in an event handler, JS submitted so we have to
    // defer this submission. let's remember it and return
    // without submitting
    mPendingSubmission = submission;
    // ensure reentrancy
    mIsSubmitting = false;
    return NS_OK;
  }

  //
  // perform the submission
  //
  return SubmitSubmission(submission);
}

nsresult
HTMLFormElement::BuildSubmission(HTMLFormSubmission** aFormSubmission,
                                 WidgetEvent* aEvent)
{
  NS_ASSERTION(!mPendingSubmission, "tried to build two submissions!");

  // Get the originating frame (failure is non-fatal)
  nsGenericHTMLElement* originatingElement = nullptr;
  if (aEvent) {
    InternalFormEvent* formEvent = aEvent->AsFormEvent();
    if (formEvent) {
      nsIContent* originator = formEvent->mOriginator;
      if (originator) {
        if (!originator->IsHTMLElement()) {
          return NS_ERROR_UNEXPECTED;
        }
        originatingElement = static_cast<nsGenericHTMLElement*>(originator);
      }
    }
  }

  nsresult rv;

  //
  // Get the submission object
  //
  rv = HTMLFormSubmission::GetFromForm(this, originatingElement,
                                       aFormSubmission);
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  //
  // Dump the data into the submission object
  //
  rv = WalkFormElements(*aFormSubmission);
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  return NS_OK;
}

nsresult
HTMLFormElement::SubmitSubmission(HTMLFormSubmission* aFormSubmission)
{
  nsresult rv;
  Element* originatingElement = aFormSubmission->GetOriginatingElement();

  //
  // Get the action and target
  //
  nsCOMPtr<nsIURI> actionURI;
  rv = GetActionURL(getter_AddRefs(actionURI), originatingElement);
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  if (!actionURI) {
    mIsSubmitting = false;
    return NS_OK;
  }

  // If there is no link handler, then we won't actually be able to submit.
  nsIDocument* doc = GetComposedDoc();
  nsCOMPtr<nsISupports> container = doc ? doc->GetContainer() : nullptr;
  nsCOMPtr<nsILinkHandler> linkHandler(do_QueryInterface(container));
  if (!linkHandler || IsEditable()) {
    mIsSubmitting = false;
    return NS_OK;
  }

  // javascript URIs are not really submissions; they just call a function.
  // Also, they may synchronously call submit(), and we want them to be able to
  // do so while still disallowing other double submissions. (Bug 139798)
  // Note that any other URI types that are of equivalent type should also be
  // added here.
  // XXXbz this is a mess.  The real issue here is that nsJSChannel sets the
  // LOAD_BACKGROUND flag, so doesn't notify us, compounded by the fact that
  // the JS executes before we forget the submission in OnStateChange on
  // STATE_STOP.  As a result, we have to make sure that we simply pretend
  // we're not submitting when submitting to a JS URL.  That's kinda bogus, but
  // there we are.
  bool schemeIsJavaScript = false;
  if (NS_SUCCEEDED(actionURI->SchemeIs("javascript", &schemeIsJavaScript)) &&
      schemeIsJavaScript) {
    mIsSubmitting = false;
  }

  // The target is the originating element formtarget attribute if the element
  // is a submit control and has such an attribute.
  // Otherwise, the target is the form owner's target attribute,
  // if it has such an attribute.
  // Finally, if one of the child nodes of the head element is a base element
  // with a target attribute, then the value of the target attribute of the
  // first such base element; or, if there is no such element, the empty string.
  nsAutoString target;
  if (!(originatingElement && originatingElement->GetAttr(kNameSpaceID_None,
                                                          nsGkAtoms::formtarget,
                                                          target)) &&
      !GetAttr(kNameSpaceID_None, nsGkAtoms::target, target)) {
    GetBaseTarget(target);
  }

  //
  // Notify observers of submit
  //
  bool cancelSubmit = false;
  if (mNotifiedObservers) {
    cancelSubmit = mNotifiedObserversResult;
  } else {
    rv = NotifySubmitObservers(actionURI, &cancelSubmit, true);
    NS_ENSURE_SUBMIT_SUCCESS(rv);
  }

  if (cancelSubmit) {
    mIsSubmitting = false;
    return NS_OK;
  }

  cancelSubmit = false;
  rv = NotifySubmitObservers(actionURI, &cancelSubmit, false);
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  if (cancelSubmit) {
    mIsSubmitting = false;
    return NS_OK;
  }

  //
  // Submit
  //
  nsCOMPtr<nsIDocShell> docShell;

  {
    nsAutoPopupStatePusher popupStatePusher(mSubmitPopupState);

    AutoHandlingUserInputStatePusher userInpStatePusher(
                                       mSubmitInitiatedFromUserInput,
                                       nullptr, doc);

    nsCOMPtr<nsIInputStream> postDataStream;
    rv = aFormSubmission->GetEncodedSubmission(actionURI,
                                               getter_AddRefs(postDataStream),
                                               actionURI);
    NS_ENSURE_SUBMIT_SUCCESS(rv);

    rv = linkHandler->OnLinkClickSync(this, actionURI,
                                      target.get(),
                                      VoidString(),
                                      postDataStream,
                                      nullptr, false,
                                      getter_AddRefs(docShell),
                                      getter_AddRefs(mSubmittingRequest),
                                      EventStateManager::IsHandlingUserInput());
    NS_ENSURE_SUBMIT_SUCCESS(rv);
  }

  // Even if the submit succeeds, it's possible for there to be no docshell
  // or request; for example, if it's to a named anchor within the same page
  // the submit will not really do anything.
  if (docShell) {
    // If the channel is pending, we have to listen for web progress.
    bool pending = false;
    mSubmittingRequest->IsPending(&pending);
    if (pending && !schemeIsJavaScript) {
      nsCOMPtr<nsIWebProgress> webProgress = do_GetInterface(docShell);
      NS_ASSERTION(webProgress, "nsIDocShell not converted to nsIWebProgress!");
      rv = webProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_ALL);
      NS_ENSURE_SUBMIT_SUCCESS(rv);
      mWebProgress = do_GetWeakReference(webProgress);
      NS_ASSERTION(mWebProgress, "can't hold weak ref to webprogress!");
    } else {
      ForgetCurrentSubmission();
    }
  } else {
    ForgetCurrentSubmission();
  }

  return rv;
}

nsresult
HTMLFormElement::DoSecureToInsecureSubmitCheck(nsIURI* aActionURL,
                                               bool* aCancelSubmit)
{
  *aCancelSubmit = false;

  // Only ask the user about posting from a secure URI to an insecure URI if
  // this element is in the root document. When this is not the case, the mixed
  // content blocker will take care of security for us.
  nsIDocument* parent = OwnerDoc()->GetParentDocument();
  bool isRootDocument = (!parent || nsContentUtils::IsChromeDoc(parent));
  if (!isRootDocument) {
    return NS_OK;
  }

  nsIPrincipal* principal = NodePrincipal();
  if (!principal) {
    *aCancelSubmit = true;
    return NS_OK;
  }
  nsCOMPtr<nsIURI> principalURI;
  nsresult rv = principal->GetURI(getter_AddRefs(principalURI));
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!principalURI) {
    principalURI = OwnerDoc()->GetDocumentURI();
  }
  bool formIsHTTPS;
  rv = principalURI->SchemeIs("https", &formIsHTTPS);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bool actionIsHTTPS;
  rv = aActionURL->SchemeIs("https", &actionIsHTTPS);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bool actionIsJS;
  rv = aActionURL->SchemeIs("javascript", &actionIsJS);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!formIsHTTPS || actionIsHTTPS || actionIsJS) {
    return NS_OK;
  }

  if (nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackURL(aActionURL)) {
    return NS_OK;
  }

  if (nsMixedContentBlocker::IsPotentiallyTrustworthyOnion(aActionURL)) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = OwnerDoc()->GetWindow();
  if (!window) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
  if (!docShell) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(docShell);
  if (!prompt) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIStringBundle> stringBundle;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
    mozilla::services::GetStringBundleService();
  if (!stringBundleService) {
    return NS_ERROR_FAILURE;
  }
  rv = stringBundleService->CreateBundle(
    "chrome://global/locale/browser.properties",
    getter_AddRefs(stringBundle));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoString title;
  nsAutoString message;
  nsAutoString cont;
  stringBundle->GetStringFromName(
    "formPostSecureToInsecureWarning.title", title);
  stringBundle->GetStringFromName(
    "formPostSecureToInsecureWarning.message", message);
  stringBundle->GetStringFromName(
    "formPostSecureToInsecureWarning.continue", cont);
  int32_t buttonPressed;
  bool checkState = false; // this is unused (ConfirmEx requires this parameter)
  rv = prompt->ConfirmEx(title.get(), message.get(),
                         (nsIPrompt::BUTTON_TITLE_IS_STRING *
                          nsIPrompt::BUTTON_POS_0) +
                         (nsIPrompt::BUTTON_TITLE_CANCEL *
                          nsIPrompt::BUTTON_POS_1),
                         cont.get(), nullptr, nullptr, nullptr,
                            &checkState, &buttonPressed);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aCancelSubmit = (buttonPressed == 1);
  uint32_t telemetryBucket =
    nsISecurityUITelemetry::WARNING_CONFIRM_POST_TO_INSECURE_FROM_SECURE;
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SECURITY_UI,
                                 telemetryBucket);
  if (!*aCancelSubmit) {
    // The user opted to continue, so note that in the next telemetry bucket.
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::SECURITY_UI,
                                   telemetryBucket + 1);
  }
  return NS_OK;
}

nsresult
HTMLFormElement::NotifySubmitObservers(nsIURI* aActionURL,
                                       bool* aCancelSubmit,
                                       bool    aEarlyNotify)
{
  // If this is the first form, bring alive the first form submit
  // category observers
  if (!gFirstFormSubmitted) {
    gFirstFormSubmitted = true;
    NS_CreateServicesFromCategory(NS_FIRST_FORMSUBMIT_CATEGORY,
                                  nullptr,
                                  NS_FIRST_FORMSUBMIT_CATEGORY);
  }

  if (!aEarlyNotify) {
    nsresult rv = DoSecureToInsecureSubmitCheck(aActionURL, aCancelSubmit);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (*aCancelSubmit) {
      return NS_OK;
    }
  }

  // Notify observers that the form is being submitted.
  nsCOMPtr<nsIObserverService> service =
    mozilla::services::GetObserverService();
  if (!service)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISimpleEnumerator> theEnum;
  nsresult rv = service->EnumerateObservers(aEarlyNotify ?
                                            NS_EARLYFORMSUBMIT_SUBJECT :
                                            NS_FORMSUBMIT_SUBJECT,
                                            getter_AddRefs(theEnum));
  NS_ENSURE_SUCCESS(rv, rv);

  if (theEnum) {
    nsCOMPtr<nsISupports> inst;
    *aCancelSubmit = false;

    // XXXbz what do the submit observers actually want?  The window
    // of the document this is shown in?  Or something else?
    // sXBL/XBL2 issue
    nsCOMPtr<nsPIDOMWindowOuter> window = OwnerDoc()->GetWindow();

    bool loop = true;
    while (NS_SUCCEEDED(theEnum->HasMoreElements(&loop)) && loop) {
      theEnum->GetNext(getter_AddRefs(inst));

      nsCOMPtr<nsIFormSubmitObserver> formSubmitObserver(
                      do_QueryInterface(inst));
      if (formSubmitObserver) {
        rv = formSubmitObserver->Notify(this,
                                        window ? window->GetCurrentInnerWindow() : nullptr,
                                        aActionURL,
                                        aCancelSubmit);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      if (*aCancelSubmit) {
        return NS_OK;
      }
    }
  }

  return rv;
}


nsresult
HTMLFormElement::WalkFormElements(HTMLFormSubmission* aFormSubmission)
{
  // This shouldn't be called recursively, so use a rather large value
  // for the preallocated buffer.
  AutoTArray<RefPtr<nsGenericHTMLFormElement>, 100> sortedControls;
  nsresult rv = mControls->GetSortedControls(sortedControls);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t len = sortedControls.Length();

  //
  // Walk the list of nodes and call SubmitNamesValues() on the controls
  //
  for (uint32_t i = 0; i < len; ++i) {
    // Tell the control to submit its name/value pairs to the submission
    sortedControls[i]->SubmitNamesValues(aFormSubmission);
  }

  return NS_OK;
}

// nsIForm

NS_IMETHODIMP_(uint32_t)
HTMLFormElement::GetElementCount() const
{
  return mControls->Length();
}

Element*
HTMLFormElement::IndexedGetter(uint32_t aIndex, bool &aFound)
{
  Element* element = mControls->mElements.SafeElementAt(aIndex, nullptr);
  aFound = element != nullptr;
  return element;
}

NS_IMETHODIMP_(nsIFormControl*)
HTMLFormElement::GetElementAt(int32_t aIndex) const
{
  return mControls->mElements.SafeElementAt(aIndex, nullptr);
}

/**
 * Compares the position of aControl1 and aControl2 in the document
 * @param aControl1 First control to compare.
 * @param aControl2 Second control to compare.
 * @param aForm Parent form of the controls.
 * @return < 0 if aControl1 is before aControl2,
 *         > 0 if aControl1 is after aControl2,
 *         0 otherwise
 */
/* static */ int32_t
HTMLFormElement::CompareFormControlPosition(Element* aElement1,
                                            Element* aElement2,
                                            const nsIContent* aForm)
{
  NS_ASSERTION(aElement1 != aElement2, "Comparing a form control to itself");

  // If an element has a @form, we can assume it *might* be able to not have
  // a parent and still be in the form.
  NS_ASSERTION((aElement1->HasAttr(kNameSpaceID_None, nsGkAtoms::form) ||
                aElement1->GetParent()) &&
               (aElement2->HasAttr(kNameSpaceID_None, nsGkAtoms::form) ||
                aElement2->GetParent()),
               "Form controls should always have parents");

  // If we pass aForm, we are assuming both controls are form descendants which
  // is not always the case. This function should work but maybe slower.
  // However, checking if both elements are form descendants may be slow too...
  // TODO: remove the prevent asserts fix, see bug 598468.
#ifdef DEBUG
  nsLayoutUtils::gPreventAssertInCompareTreePosition = true;
  int32_t rVal = nsLayoutUtils::CompareTreePosition(aElement1, aElement2, aForm);
  nsLayoutUtils::gPreventAssertInCompareTreePosition = false;

  return rVal;
#else // DEBUG
  return nsLayoutUtils::CompareTreePosition(aElement1, aElement2, aForm);
#endif // DEBUG
}

#ifdef DEBUG
/**
 * Checks that all form elements are in document order. Asserts if any pair of
 * consecutive elements are not in increasing document order.
 *
 * @param aControls List of form controls to check.
 * @param aForm Parent form of the controls.
 */
/* static */ void
HTMLFormElement::AssertDocumentOrder(
  const nsTArray<nsGenericHTMLFormElement*>& aControls, nsIContent* aForm)
{
  // TODO: remove the return statement with bug 598468.
  // This is done to prevent asserts in some edge cases.
  return;

  // Only iterate if aControls is not empty, since otherwise
  // |aControls.Length() - 1| will be a very large unsigned number... not what
  // we want here.
  if (!aControls.IsEmpty()) {
    for (uint32_t i = 0; i < aControls.Length() - 1; ++i) {
      NS_ASSERTION(CompareFormControlPosition(aControls[i], aControls[i + 1],
                                              aForm) < 0,
                   "Form controls not ordered correctly");
    }
  }
}

/**
 * Copy of the above function, but with RefPtrs.
 *
 * @param aControls List of form controls to check.
 * @param aForm Parent form of the controls.
 */
/* static */ void
HTMLFormElement::AssertDocumentOrder(
  const nsTArray<RefPtr<nsGenericHTMLFormElement>>& aControls, nsIContent* aForm)
{
  // TODO: remove the return statement with bug 598468.
  // This is done to prevent asserts in some edge cases.
  return;

  // Only iterate if aControls is not empty, since otherwise
  // |aControls.Length() - 1| will be a very large unsigned number... not what
  // we want here.
  if (!aControls.IsEmpty()) {
    for (uint32_t i = 0; i < aControls.Length() - 1; ++i) {
      NS_ASSERTION(CompareFormControlPosition(aControls[i], aControls[i + 1],
                                              aForm) < 0,
                   "Form controls not ordered correctly");
    }
  }
}
#endif

void
HTMLFormElement::PostPasswordEvent()
{
  // Don't fire another add event if we have a pending add event.
  if (mFormPasswordEventDispatcher.get()) {
    return;
  }

  mFormPasswordEventDispatcher =
    new AsyncEventDispatcher(this, NS_LITERAL_STRING("DOMFormHasPassword"),
                             true, true);
  mFormPasswordEventDispatcher->PostDOMEvent();
}

namespace {

struct FormComparator
{
  Element* const mChild;
  HTMLFormElement* const mForm;
  FormComparator(Element* aChild, HTMLFormElement* aForm)
    : mChild(aChild), mForm(aForm) {}
  int operator()(Element* aElement) const {
    return HTMLFormElement::CompareFormControlPosition(mChild, aElement, mForm);
  }
};

} // namespace

// This function return true if the element, once appended, is the last one in
// the array.
template<typename ElementType>
static bool
AddElementToList(nsTArray<ElementType*>& aList, ElementType* aChild,
                 HTMLFormElement* aForm)
{
  NS_ASSERTION(aList.IndexOf(aChild) == aList.NoIndex,
               "aChild already in aList");

  const uint32_t count = aList.Length();
  ElementType* element;
  bool lastElement = false;

  // Optimize most common case where we insert at the end.
  int32_t position = -1;
  if (count > 0) {
    element = aList[count - 1];
    position =
      HTMLFormElement::CompareFormControlPosition(aChild, element, aForm);
  }

  // If this item comes after the last element, or the elements array is
  // empty, we append to the end. Otherwise, we do a binary search to
  // determine where the element should go.
  if (position >= 0 || count == 0) {
    // WEAK - don't addref
    aList.AppendElement(aChild);
    lastElement = true;
  }
  else {
    size_t idx;
    BinarySearchIf(aList, 0, count, FormComparator(aChild, aForm), &idx);

    // WEAK - don't addref
    aList.InsertElementAt(idx, aChild);
  }

  return lastElement;
}

nsresult
HTMLFormElement::AddElement(nsGenericHTMLFormElement* aChild,
                            bool aUpdateValidity, bool aNotify)
{
  // If an element has a @form, we can assume it *might* be able to not have
  // a parent and still be in the form.
  NS_ASSERTION(aChild->HasAttr(kNameSpaceID_None, nsGkAtoms::form) ||
               aChild->GetParent(),
               "Form control should have a parent");

  // Determine whether to add the new element to the elements or
  // the not-in-elements list.
  bool childInElements = HTMLFormControlsCollection::ShouldBeInElements(aChild);
  nsTArray<nsGenericHTMLFormElement*>& controlList = childInElements ?
      mControls->mElements : mControls->mNotInElements;

  bool lastElement = AddElementToList(controlList, aChild, this);

#ifdef DEBUG
  AssertDocumentOrder(controlList, this);
#endif

  int32_t type = aChild->ControlType();

  //
  // If it is a password control, and the password manager has not yet been
  // initialized, initialize the password manager
  //
  if (type == NS_FORM_INPUT_PASSWORD) {
    if (!gPasswordManagerInitialized) {
      gPasswordManagerInitialized = true;
      NS_CreateServicesFromCategory(NS_PASSWORDMANAGER_CATEGORY,
                                    nullptr,
                                    NS_PASSWORDMANAGER_CATEGORY);
    }
    PostPasswordEvent();
  }

  // Default submit element handling
  if (aChild->IsSubmitControl()) {
    // Update mDefaultSubmitElement, mFirstSubmitInElements,
    // mFirstSubmitNotInElements.

    nsGenericHTMLFormElement** firstSubmitSlot =
      childInElements ? &mFirstSubmitInElements : &mFirstSubmitNotInElements;

    // The new child is the new first submit in its list if the firstSubmitSlot
    // is currently empty or if the child is before what's currently in the
    // slot.  Note that if we already have a control in firstSubmitSlot and
    // we're appending this element can't possibly replace what's currently in
    // the slot.  Also note that aChild can't become the mDefaultSubmitElement
    // unless it replaces what's in the slot.  If it _does_ replace what's in
    // the slot, it becomes the default submit if either the default submit is
    // what's in the slot or the child is earlier than the default submit.
    nsGenericHTMLFormElement* oldDefaultSubmit = mDefaultSubmitElement;
    if (!*firstSubmitSlot ||
        (!lastElement &&
         CompareFormControlPosition(aChild, *firstSubmitSlot, this) < 0)) {
      // Update mDefaultSubmitElement if it's currently in a valid state.
      // Valid state means either non-null or null because there are in fact
      // no submit elements around.
      if ((mDefaultSubmitElement ||
           (!mFirstSubmitInElements && !mFirstSubmitNotInElements)) &&
          (*firstSubmitSlot == mDefaultSubmitElement ||
           CompareFormControlPosition(aChild,
                                      mDefaultSubmitElement, this) < 0)) {
        mDefaultSubmitElement = aChild;
      }
      *firstSubmitSlot = aChild;
    }

    MOZ_ASSERT(mDefaultSubmitElement == mFirstSubmitInElements ||
               mDefaultSubmitElement == mFirstSubmitNotInElements ||
               !mDefaultSubmitElement,
               "What happened here?");

    // Notify that the state of the previous default submit element has changed
    // if the element which is the default submit element has changed.  The new
    // default submit element is responsible for its own state update.
    if (oldDefaultSubmit && oldDefaultSubmit != mDefaultSubmitElement) {
      oldDefaultSubmit->UpdateState(aNotify);
    }
  }

  // If the element is subject to constraint validaton and is invalid, we need
  // to update our internal counter.
  if (aUpdateValidity) {
    nsCOMPtr<nsIConstraintValidation> cvElmt = do_QueryObject(aChild);
    if (cvElmt &&
        cvElmt->IsCandidateForConstraintValidation() && !cvElmt->IsValid()) {
      UpdateValidity(false);
    }
  }

  // Notify the radio button it's been added to a group
  // This has to be done _after_ UpdateValidity() call to prevent the element
  // being count twice.
  if (type == NS_FORM_INPUT_RADIO) {
    RefPtr<HTMLInputElement> radio =
      static_cast<HTMLInputElement*>(aChild);
    radio->AddedToRadioGroup();
  }

  return NS_OK;
}

nsresult
HTMLFormElement::AddElementToTable(nsGenericHTMLFormElement* aChild,
                                   const nsAString& aName)
{
  return mControls->AddElementToTable(aChild, aName);
}


nsresult
HTMLFormElement::RemoveElement(nsGenericHTMLFormElement* aChild,
                               bool aUpdateValidity)
{
  RemoveElementFromPastNamesMap(aChild);

  //
  // Remove it from the radio group if it's a radio button
  //
  nsresult rv = NS_OK;
  if (aChild->ControlType() == NS_FORM_INPUT_RADIO) {
    RefPtr<HTMLInputElement> radio =
      static_cast<HTMLInputElement*>(aChild);
    radio->WillRemoveFromRadioGroup();
  }

  // Determine whether to remove the child from the elements list
  // or the not in elements list.
  bool childInElements = HTMLFormControlsCollection::ShouldBeInElements(aChild);
  nsTArray<nsGenericHTMLFormElement*>& controls = childInElements ?
      mControls->mElements :  mControls->mNotInElements;

  // Find the index of the child. This will be used later if necessary
  // to find the default submit.
  size_t index = controls.IndexOf(aChild);
  NS_ENSURE_STATE(index != controls.NoIndex);

  controls.RemoveElementAt(index);

  // Update our mFirstSubmit* values.
  nsGenericHTMLFormElement** firstSubmitSlot =
    childInElements ? &mFirstSubmitInElements : &mFirstSubmitNotInElements;
  if (aChild == *firstSubmitSlot) {
    *firstSubmitSlot = nullptr;

    // We are removing the first submit in this list, find the new first submit
    uint32_t length = controls.Length();
    for (uint32_t i = index; i < length; ++i) {
      nsGenericHTMLFormElement* currentControl = controls[i];
      if (currentControl->IsSubmitControl()) {
        *firstSubmitSlot = currentControl;
        break;
      }
    }
  }

  if (aChild == mDefaultSubmitElement) {
    // Need to reset mDefaultSubmitElement.  Do this asynchronously so
    // that we're not doing it while the DOM is in flux.
    mDefaultSubmitElement = nullptr;
    nsContentUtils::AddScriptRunner(new RemoveElementRunnable(this));

    // Note that we don't need to notify on the old default submit (which is
    // being removed) because it's either being removed from the DOM or
    // changing attributes in a way that makes it responsible for sending its
    // own notifications.
  }

  // If the element was subject to constraint validaton and is invalid, we need
  // to update our internal counter.
  if (aUpdateValidity) {
    nsCOMPtr<nsIConstraintValidation> cvElmt = do_QueryObject(aChild);
    if (cvElmt &&
        cvElmt->IsCandidateForConstraintValidation() && !cvElmt->IsValid()) {
      UpdateValidity(true);
    }
  }

  return rv;
}

void
HTMLFormElement::HandleDefaultSubmitRemoval()
{
  if (mDefaultSubmitElement) {
    // Already got reset somehow; nothing else to do here
    return;
  }

  if (!mFirstSubmitNotInElements) {
    mDefaultSubmitElement = mFirstSubmitInElements;
  } else if (!mFirstSubmitInElements) {
    mDefaultSubmitElement = mFirstSubmitNotInElements;
  } else {
    NS_ASSERTION(mFirstSubmitInElements != mFirstSubmitNotInElements,
                 "How did that happen?");
    // Have both; use the earlier one
    mDefaultSubmitElement =
      CompareFormControlPosition(mFirstSubmitInElements,
                                 mFirstSubmitNotInElements, this) < 0 ?
      mFirstSubmitInElements : mFirstSubmitNotInElements;
  }

  MOZ_ASSERT(mDefaultSubmitElement == mFirstSubmitInElements ||
             mDefaultSubmitElement == mFirstSubmitNotInElements,
             "What happened here?");

  // Notify about change if needed.
  if (mDefaultSubmitElement) {
    mDefaultSubmitElement->UpdateState(true);
  }
}

nsresult
HTMLFormElement::RemoveElementFromTableInternal(
  nsInterfaceHashtable<nsStringHashKey,nsISupports>& aTable,
  nsIContent* aChild, const nsAString& aName)
{
  auto entry = aTable.Lookup(aName);
  if (!entry) {
    return NS_OK;
  }
  // Single element in the hash, just remove it if it's the one
  // we're trying to remove...
  if (entry.Data() == aChild) {
    entry.Remove();
    ++mExpandoAndGeneration.generation;
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(entry.Data()));
  if (content) {
    return NS_OK;
  }

  // If it's not a content node then it must be a RadioNodeList.
  MOZ_ASSERT(nsCOMPtr<RadioNodeList>(do_QueryInterface(entry.Data())));
  auto* list = static_cast<RadioNodeList*>(entry.Data().get());

  list->RemoveElement(aChild);

  uint32_t length = list->Length();

  if (!length) {
    // If the list is empty we remove if from our hash, this shouldn't
    // happen tho
    entry.Remove();
    ++mExpandoAndGeneration.generation;
  } else if (length == 1) {
    // Only one element left, replace the list in the hash with the
    // single element.
    nsIContent* node = list->Item(0);
    if (node) {
      entry.Data() = node;
    }
  }

  return NS_OK;
}

nsresult
HTMLFormElement::RemoveElementFromTable(nsGenericHTMLFormElement* aElement,
                                        const nsAString& aName)
{
  return mControls->RemoveElementFromTable(aElement, aName);
}

already_AddRefed<nsISupports>
HTMLFormElement::NamedGetter(const nsAString& aName, bool &aFound)
{
  aFound = true;

  nsCOMPtr<nsISupports> result = DoResolveName(aName, true);
  if (result) {
    AddToPastNamesMap(aName, result);
    return result.forget();
  }

  result = mImageNameLookupTable.GetWeak(aName);
  if (result) {
    AddToPastNamesMap(aName, result);
    return result.forget();
  }

  result = mPastNameLookupTable.GetWeak(aName);
  if (result) {
    return result.forget();
  }

  aFound = false;
  return nullptr;
}

void
HTMLFormElement::GetSupportedNames(nsTArray<nsString >& aRetval)
{
  // TODO https://github.com/whatwg/html/issues/1731
}

already_AddRefed<nsISupports>
HTMLFormElement::FindNamedItem(const nsAString& aName,
                               nsWrapperCache** aCache)
{
  // FIXME Get the wrapper cache from DoResolveName.

  bool found;
  nsCOMPtr<nsISupports> result = NamedGetter(aName, found);
  if (result) {
    *aCache = nullptr;
    return result.forget();
  }

  return nullptr;
}

already_AddRefed<nsISupports>
HTMLFormElement::DoResolveName(const nsAString& aName,
                               bool aFlushContent)
{
  nsCOMPtr<nsISupports> result =
    mControls->NamedItemInternal(aName, aFlushContent);
  return result.forget();
}

void
HTMLFormElement::OnSubmitClickBegin(Element* aOriginatingElement)
{
  mDeferSubmission = true;

  // Prepare to run NotifySubmitObservers early before the
  // scripts on the page get to modify the form data, possibly
  // throwing off any password manager. (bug 257781)
  nsCOMPtr<nsIURI> actionURI;
  nsresult rv;

  rv = GetActionURL(getter_AddRefs(actionURI), aOriginatingElement);
  if (NS_FAILED(rv) || !actionURI)
    return;

  // Notify observers of submit if the form is valid.
  // TODO: checking for mInvalidElementsCount is a temporary fix that should be
  // removed with bug 610402.
  if (mInvalidElementsCount == 0) {
    bool cancelSubmit = false;
    rv = NotifySubmitObservers(actionURI, &cancelSubmit, true);
    if (NS_SUCCEEDED(rv)) {
      mNotifiedObservers = true;
      mNotifiedObserversResult = cancelSubmit;
    }
  }
}

void
HTMLFormElement::OnSubmitClickEnd()
{
  mDeferSubmission = false;
}

void
HTMLFormElement::FlushPendingSubmission()
{
  if (mPendingSubmission) {
    // Transfer owning reference so that the submissioin doesn't get deleted
    // if we reenter
    nsAutoPtr<HTMLFormSubmission> submission = std::move(mPendingSubmission);

    SubmitSubmission(submission);
  }
}

void
HTMLFormElement::GetAction(nsString& aValue)
{
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::action, aValue) ||
      aValue.IsEmpty()) {
    nsIDocument* document = OwnerDoc();
    nsIURI* docURI = document->GetDocumentURI();
    if (docURI) {
      nsAutoCString spec;
      nsresult rv = docURI->GetSpec(spec);
      if (NS_FAILED(rv)) {
        return;
      }

      CopyUTF8toUTF16(spec, aValue);
    }
  } else {
    GetURIAttr(nsGkAtoms::action, nullptr, aValue);
  }
}

nsresult
HTMLFormElement::GetActionURL(nsIURI** aActionURL,
                              Element* aOriginatingElement)
{
  nsresult rv = NS_OK;

  *aActionURL = nullptr;

  //
  // Grab the URL string
  //
  // If the originating element is a submit control and has the formaction
  // attribute specified, it should be used. Otherwise, the action attribute
  // from the form element should be used.
  //
  nsAutoString action;

  if (aOriginatingElement &&
      aOriginatingElement->HasAttr(kNameSpaceID_None, nsGkAtoms::formaction)) {
#ifdef DEBUG
    nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(aOriginatingElement);
    NS_ASSERTION(formControl && formControl->IsSubmitControl(),
                 "The originating element must be a submit form control!");
#endif // DEBUG

    HTMLInputElement* inputElement = HTMLInputElement::FromNode(aOriginatingElement);
    if (inputElement) {
      inputElement->GetFormAction(action);
    } else {
      auto buttonElement = HTMLButtonElement::FromNode(aOriginatingElement);
      if (buttonElement) {
        buttonElement->GetFormAction(action);
      } else {
        NS_ERROR("Originating element must be an input or button element!");
        return NS_ERROR_UNEXPECTED;
      }
    }
  } else {
    GetAction(action);
  }

  //
  // Form the full action URL
  //

  // Get the document to form the URL.
  // We'll also need it later to get the DOM window when notifying form submit
  // observers (bug 33203)
  if (!IsInUncomposedDoc()) {
    return NS_OK; // No doc means don't submit, see Bug 28988
  }

  // Get base URL
  nsIDocument *document = OwnerDoc();
  nsIURI *docURI = document->GetDocumentURI();
  NS_ENSURE_TRUE(docURI, NS_ERROR_UNEXPECTED);

  // If an action is not specified and we are inside
  // a HTML document then reload the URL. This makes us
  // compatible with 4.x browsers.
  // If we are in some other type of document such as XML or
  // XUL, do nothing. This prevents undesirable reloading of
  // a document inside XUL.

  nsCOMPtr<nsIURI> actionURL;
  if (action.IsEmpty()) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(document));
    if (!htmlDoc) {
      // Must be a XML, XUL or other non-HTML document type
      // so do nothing.
      return NS_OK;
    }

    rv = docURI->Clone(getter_AddRefs(actionURL));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsCOMPtr<nsIURI> baseURL = GetBaseURI();
    NS_ASSERTION(baseURL, "No Base URL found in Form Submit!\n");
    if (!baseURL) {
      return NS_OK; // No base URL -> exit early, see Bug 30721
    }
    rv = NS_NewURI(getter_AddRefs(actionURL), action, nullptr, baseURL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  //
  // Verify the URL should be reached
  //
  // Get security manager, check to see if access to action URI is allowed.
  //
  nsIScriptSecurityManager *securityManager =
      nsContentUtils::GetSecurityManager();
  rv = securityManager->
    CheckLoadURIWithPrincipal(NodePrincipal(), actionURL,
                              nsIScriptSecurityManager::STANDARD);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check if CSP allows this form-action
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);
  if (csp) {
    bool permitsFormAction = true;

    // form-action is only enforced if explicitly defined in the
    // policy - do *not* consult default-src, see:
    // http://www.w3.org/TR/CSP2/#directive-default-src
    rv = csp->Permits(actionURL, nsIContentSecurityPolicy::FORM_ACTION_DIRECTIVE,
                      true, &permitsFormAction);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!permitsFormAction) {
      return NS_ERROR_CSP_FORM_ACTION_VIOLATION;
    }
  }

  // Potentially the page uses the CSP directive 'upgrade-insecure-requests'. In
  // such a case we have to upgrade the action url from http:// to https://.
  // If the actionURL is not http, then there is nothing to do.
  bool isHttpScheme = false;
  rv = actionURL->SchemeIs("http", &isHttpScheme);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isHttpScheme && document->GetUpgradeInsecureRequests(false)) {
    // let's use the old specification before the upgrade for logging
    nsAutoCString spec;
    rv = actionURL->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ConvertUTF8toUTF16 reportSpec(spec);

    // upgrade the actionURL from http:// to use https://
    nsCOMPtr<nsIURI> upgradedActionURL;
    rv = NS_GetSecureUpgradedURI(actionURL, getter_AddRefs(upgradedActionURL));
    NS_ENSURE_SUCCESS(rv, rv);
    actionURL = upgradedActionURL.forget();

    // let's log a message to the console that we are upgrading a request
    nsAutoCString scheme;
    rv = actionURL->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ConvertUTF8toUTF16 reportScheme(scheme);

    const char16_t* params[] = { reportSpec.get(), reportScheme.get() };
    CSP_LogLocalizedStr("upgradeInsecureRequest",
                        params, ArrayLength(params),
                        EmptyString(), // aSourceFile
                        EmptyString(), // aScriptSample
                        0, // aLineNumber
                        0, // aColumnNumber
                        nsIScriptError::warningFlag, "CSP",
                        document->InnerWindowID(),
                        !!document->NodePrincipal()->OriginAttributesRef().mPrivateBrowsingId);
  }

  //
  // Assign to the output
  //
  actionURL.forget(aActionURL);

  return rv;
}

NS_IMETHODIMP_(nsIFormControl*)
HTMLFormElement::GetDefaultSubmitElement() const
{
  MOZ_ASSERT(mDefaultSubmitElement == mFirstSubmitInElements ||
             mDefaultSubmitElement == mFirstSubmitNotInElements,
             "What happened here?");

  return mDefaultSubmitElement;
}

bool
HTMLFormElement::IsDefaultSubmitElement(const nsIFormControl* aControl) const
{
  MOZ_ASSERT(aControl, "Unexpected call");

  if (aControl == mDefaultSubmitElement) {
    // Yes, it is
    return true;
  }

  if (mDefaultSubmitElement ||
      (aControl != mFirstSubmitInElements &&
       aControl != mFirstSubmitNotInElements)) {
    // It isn't
    return false;
  }

  // mDefaultSubmitElement is null, but we have a non-null submit around
  // (aControl, in fact).  figure out whether it's in fact the default submit
  // and just hasn't been set that way yet.  Note that we can't just call
  // HandleDefaultSubmitRemoval because we might need to notify to handle that
  // correctly and we don't know whether that's safe right here.
  if (!mFirstSubmitInElements || !mFirstSubmitNotInElements) {
    // We only have one first submit; aControl has to be it
    return true;
  }

  // We have both kinds of submits.  Check which comes first.
  nsIFormControl* defaultSubmit =
    CompareFormControlPosition(mFirstSubmitInElements,
                               mFirstSubmitNotInElements, this) < 0 ?
      mFirstSubmitInElements : mFirstSubmitNotInElements;
  return aControl == defaultSubmit;
}

bool
HTMLFormElement::ImplicitSubmissionIsDisabled() const
{
  // Input text controls are always in the elements list.
  uint32_t numDisablingControlsFound = 0;
  uint32_t length = mControls->mElements.Length();
  for (uint32_t i = 0; i < length && numDisablingControlsFound < 2; ++i) {
    if (mControls->mElements[i]->IsSingleLineTextOrNumberControl(false)) {
      numDisablingControlsFound++;
    }
  }
  return numDisablingControlsFound != 1;
}

bool
HTMLFormElement::IsLastActiveElement(const nsIFormControl* aControl) const
{
  MOZ_ASSERT(aControl, "Unexpected call");

  for (auto* element : Reversed(mControls->mElements)) {
    if (element->IsSingleLineTextOrNumberControl(false) &&
        !element->IsDisabled()) {
      return element == aControl;
    }
  }
  return false;
}

int32_t
HTMLFormElement::Length()
{
  return mControls->Length();
}

void
HTMLFormElement::ForgetCurrentSubmission()
{
  mNotifiedObservers = false;
  mIsSubmitting = false;
  mSubmittingRequest = nullptr;
  nsCOMPtr<nsIWebProgress> webProgress = do_QueryReferent(mWebProgress);
  if (webProgress) {
    webProgress->RemoveProgressListener(this);
  }
  mWebProgress = nullptr;
}

bool
HTMLFormElement::CheckFormValidity(nsIMutableArray* aInvalidElements) const
{
  bool ret = true;

  // This shouldn't be called recursively, so use a rather large value
  // for the preallocated buffer.
  AutoTArray<RefPtr<nsGenericHTMLFormElement>, 100> sortedControls;
  if (NS_FAILED(mControls->GetSortedControls(sortedControls))) {
    return false;
  }

  uint32_t len = sortedControls.Length();

  for (uint32_t i = 0; i < len; ++i) {
    nsCOMPtr<nsIConstraintValidation> cvElmt = do_QueryObject(sortedControls[i]);
    if (cvElmt && cvElmt->IsCandidateForConstraintValidation() &&
        !cvElmt->IsValid()) {
      ret = false;
      bool defaultAction = true;
      nsContentUtils::DispatchTrustedEvent(sortedControls[i]->OwnerDoc(),
                                           static_cast<nsIContent*>(sortedControls[i]),
                                           NS_LITERAL_STRING("invalid"),
                                           false, true, &defaultAction);

      // Add all unhandled invalid controls to aInvalidElements if the caller
      // requested them.
      if (defaultAction && aInvalidElements) {
        aInvalidElements->AppendElement(ToSupports(sortedControls[i]));
      }
    }
  }

  return ret;
}

bool
HTMLFormElement::CheckValidFormSubmission()
{
  /**
   * Check for form validity: do not submit a form if there are unhandled
   * invalid controls in the form.
   * This should not be done if the form has been submitted with .submit().
   *
   * NOTE: for the moment, we are also checking that there is an observer for
   * NS_INVALIDFORMSUBMIT_SUBJECT so it will prevent blocking form submission
   * if the browser does not have implemented a UI yet.
   *
   * TODO: the check for observer should be removed later when HTML5 Forms will
   * be spread enough and authors will assume forms can't be submitted when
   * invalid. See bug 587671.
   */

  NS_ASSERTION(!HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate),
               "We shouldn't be there if novalidate is set!");

  // When .submit() is called aEvent = nullptr so we can rely on that to know if
  // we have to check the validity of the form.
  nsCOMPtr<nsIObserverService> service =
    mozilla::services::GetObserverService();
  if (!service) {
    NS_WARNING("No observer service available!");
    return true;
  }

  nsCOMPtr<nsISimpleEnumerator> theEnum;
  nsresult rv = service->EnumerateObservers(NS_INVALIDFORMSUBMIT_SUBJECT,
                                            getter_AddRefs(theEnum));
  // Return true on error here because that's what we always did
  NS_ENSURE_SUCCESS(rv, true);

  bool hasObserver = false;
  rv = theEnum->HasMoreElements(&hasObserver);

  // Do not check form validity if there is no observer for
  // NS_INVALIDFORMSUBMIT_SUBJECT.
  if (NS_SUCCEEDED(rv) && hasObserver) {
    nsCOMPtr<nsIMutableArray> invalidElements =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
    // Return true on error here because that's what we always did
    NS_ENSURE_SUCCESS(rv, true);

    if (!CheckFormValidity(invalidElements.get())) {
      // For the first invalid submission, we should update element states.
      // We have to do that _before_ calling the observers so we are sure they
      // will not interfere (like focusing the element).
      if (!mEverTriedInvalidSubmit) {
        mEverTriedInvalidSubmit = true;

        /*
         * We are going to call update states assuming elements want to
         * be notified because we can't know.
         * Submissions shouldn't happen during parsing so it _should_ be safe.
         */

        nsAutoScriptBlocker scriptBlocker;

        for (uint32_t i = 0, length = mControls->mElements.Length();
             i < length; ++i) {
          // Input elements can trigger a form submission and we want to
          // update the style in that case.
          if (mControls->mElements[i]->IsHTMLElement(nsGkAtoms::input) &&
              nsContentUtils::IsFocusedContent(mControls->mElements[i])) {
            static_cast<HTMLInputElement*>(mControls->mElements[i])
              ->UpdateValidityUIBits(true);
          }

          mControls->mElements[i]->UpdateState(true);
        }

        // Because of backward compatibility, <input type='image'> is not in
        // elements but can be invalid.
        // TODO: should probably be removed when bug 606491 will be fixed.
        for (uint32_t i = 0, length = mControls->mNotInElements.Length();
             i < length; ++i) {
          mControls->mNotInElements[i]->UpdateState(true);
        }
      }

      nsCOMPtr<nsISupports> inst;
      nsCOMPtr<nsIFormSubmitObserver> observer;
      bool more = true;
      while (NS_SUCCEEDED(theEnum->HasMoreElements(&more)) && more) {
        theEnum->GetNext(getter_AddRefs(inst));
        observer = do_QueryInterface(inst);

        if (observer) {
          observer->NotifyInvalidSubmit(this,
                                        static_cast<nsIArray*>(invalidElements));
        }
      }

      // The form is invalid. Observers have been alerted. Do not submit.
      return false;
    }
  } else {
    NS_WARNING("There is no observer for \"invalidformsubmit\". \
One should be implemented!");
  }

  return true;
}

bool
HTMLFormElement::SubmissionCanProceed(Element* aSubmitter)
{
#ifdef DEBUG
  if (aSubmitter) {
    nsCOMPtr<nsIFormControl> fc = do_QueryInterface(aSubmitter);
    MOZ_ASSERT(fc);

    uint32_t type = fc->ControlType();
    MOZ_ASSERT(type == NS_FORM_INPUT_SUBMIT ||
               type == NS_FORM_INPUT_IMAGE ||
               type == NS_FORM_BUTTON_SUBMIT,
               "aSubmitter is not a submit control?");
  }
#endif

  // Modified step 2 of
  // https://html.spec.whatwg.org/multipage/forms.html#concept-form-submit --
  // we're not checking whether the node document is disconnected yet...
  if (OwnerDoc()->GetSandboxFlags() & SANDBOXED_FORMS) {
    return false;
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) {
    return true;
  }

  if (aSubmitter &&
      aSubmitter->HasAttr(kNameSpaceID_None, nsGkAtoms::formnovalidate)) {
    return true;
  }

  return CheckValidFormSubmission();
}

void
HTMLFormElement::UpdateValidity(bool aElementValidity)
{
  if (aElementValidity) {
    --mInvalidElementsCount;
  } else {
    ++mInvalidElementsCount;
  }

  NS_ASSERTION(mInvalidElementsCount >= 0, "Something went seriously wrong!");

  // The form validity has just changed if:
  // - there are no more invalid elements ;
  // - or there is one invalid elmement and an element just became invalid.
  // If we have invalid elements and we used to before as well, do nothing.
  if (mInvalidElementsCount &&
      (mInvalidElementsCount != 1 || aElementValidity)) {
    return;
  }

  /*
   * We are going to update states assuming submit controls want to
   * be notified because we can't know.
   * UpdateValidity shouldn't be called so much during parsing so it _should_
   * be safe.
   */

  nsAutoScriptBlocker scriptBlocker;

  // Inform submit controls that the form validity has changed.
  for (uint32_t i = 0, length = mControls->mElements.Length();
       i < length; ++i) {
    if (mControls->mElements[i]->IsSubmitControl()) {
      mControls->mElements[i]->UpdateState(true);
    }
  }

  // Because of backward compatibility, <input type='image'> is not in elements
  // so we have to check for controls not in elements too.
  uint32_t length = mControls->mNotInElements.Length();
  for (uint32_t i = 0; i < length; ++i) {
    if (mControls->mNotInElements[i]->IsSubmitControl()) {
      mControls->mNotInElements[i]->UpdateState(true);
    }
  }

  UpdateState(true);
}

// nsIWebProgressListener
NS_IMETHODIMP
HTMLFormElement::OnStateChange(nsIWebProgress* aWebProgress,
                               nsIRequest* aRequest,
                               uint32_t aStateFlags,
                               nsresult aStatus)
{
  // If STATE_STOP is never fired for any reason (redirect?  Failed state
  // change?) the form element will leak.  It will be kept around by the
  // nsIWebProgressListener (assuming it keeps a strong pointer).  We will
  // consequently leak the request.
  if (aRequest == mSubmittingRequest &&
      aStateFlags & nsIWebProgressListener::STATE_STOP) {
    ForgetCurrentSubmission();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLFormElement::OnProgressChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  int32_t aCurSelfProgress,
                                  int32_t aMaxSelfProgress,
                                  int32_t aCurTotalProgress,
                                  int32_t aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
HTMLFormElement::OnLocationChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsIURI* location,
                                  uint32_t aFlags)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
HTMLFormElement::OnStatusChange(nsIWebProgress* aWebProgress,
                                nsIRequest* aRequest,
                                nsresult aStatus,
                                const char16_t* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
HTMLFormElement::OnSecurityChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  uint32_t state)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP_(int32_t)
HTMLFormElement::IndexOfControl(nsIFormControl* aControl)
{
  int32_t index = 0;
  return mControls->IndexOfControl(aControl, &index) == NS_OK ? index : 0;
}

void
HTMLFormElement::SetCurrentRadioButton(const nsAString& aName,
                                       HTMLInputElement* aRadio)
{
  mSelectedRadioButtons.Put(aName, aRadio);
}

HTMLInputElement*
HTMLFormElement::GetCurrentRadioButton(const nsAString& aName)
{
  return mSelectedRadioButtons.GetWeak(aName);
}

NS_IMETHODIMP
HTMLFormElement::GetNextRadioButton(const nsAString& aName,
                                    const bool aPrevious,
                                    HTMLInputElement* aFocusedRadio,
                                    HTMLInputElement** aRadioOut)
{
  // Return the radio button relative to the focused radio button.
  // If no radio is focused, get the radio relative to the selected one.
  *aRadioOut = nullptr;

  RefPtr<HTMLInputElement> currentRadio;
  if (aFocusedRadio) {
    currentRadio = aFocusedRadio;
  }
  else {
    mSelectedRadioButtons.Get(aName, getter_AddRefs(currentRadio));
  }

  nsCOMPtr<nsISupports> itemWithName = DoResolveName(aName, true);
  nsCOMPtr<nsINodeList> radioGroup(do_QueryInterface(itemWithName));

  if (!radioGroup) {
    return NS_ERROR_FAILURE;
  }

  int32_t index = radioGroup->IndexOf(currentRadio);
  if (index < 0) {
    return NS_ERROR_FAILURE;
  }

  uint32_t numRadios = radioGroup->Length();
  RefPtr<HTMLInputElement> radio;

  bool isRadio = false;
  do {
    if (aPrevious) {
      if (--index < 0) {
        index = numRadios -1;
      }
    }
    else if (++index >= (int32_t)numRadios) {
      index = 0;
    }
    radio = HTMLInputElement::FromNodeOrNull(radioGroup->Item(index));
    isRadio = radio && radio->ControlType() == NS_FORM_INPUT_RADIO;
    if (!isRadio) {
      continue;
    }

    nsAutoString name;
    radio->GetName(name);
    isRadio = aName.Equals(name);
  } while (!isRadio || (radio->Disabled() && radio != currentRadio));

  NS_IF_ADDREF(*aRadioOut = radio);
  return NS_OK;
}

NS_IMETHODIMP
HTMLFormElement::WalkRadioGroup(const nsAString& aName,
                                nsIRadioVisitor* aVisitor,
                                bool aFlushContent)
{
  if (aName.IsEmpty()) {
    //
    // XXX If the name is empty, it's not stored in the control list.  There
    // *must* be a more efficient way to do this.
    //
    nsCOMPtr<nsIFormControl> control;
    uint32_t len = GetElementCount();
    for (uint32_t i = 0; i < len; i++) {
      control = GetElementAt(i);
      if (control->ControlType() == NS_FORM_INPUT_RADIO) {
        nsCOMPtr<Element> controlElement = do_QueryInterface(control);
        if (controlElement &&
            controlElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                        EmptyString(), eCaseMatters) &&
            !aVisitor->Visit(control)) {
          break;
        }
      }
    }
    return NS_OK;
  }

  // Get the control / list of controls from the form using form["name"]
  nsCOMPtr<nsISupports> item = DoResolveName(aName, aFlushContent);
  if (!item) {
    return NS_ERROR_FAILURE;
  }

  // If it's just a lone radio button, then select it.
  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(item);
  if (formControl) {
    if (formControl->ControlType() == NS_FORM_INPUT_RADIO) {
      aVisitor->Visit(formControl);
    }
    return NS_OK;
  }

  nsCOMPtr<nsINodeList> nodeList = do_QueryInterface(item);
  if (!nodeList) {
    return NS_OK;
  }
  uint32_t length = nodeList->Length();
  for (uint32_t i = 0; i < length; i++) {
    nsIContent* node = nodeList->Item(i);
    nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(node);
    if (formControl && formControl->ControlType() == NS_FORM_INPUT_RADIO &&
        !aVisitor->Visit(formControl)) {
      break;
    }
  }
  return NS_OK;
}

void
HTMLFormElement::AddToRadioGroup(const nsAString& aName,
                                 HTMLInputElement* aRadio)
{
  if (aRadio->IsRequired()) {
    auto entry = mRequiredRadioButtonCounts.LookupForAdd(aName);
    if (!entry) {
      entry.OrInsert([]() { return 1; });
    } else {
      ++entry.Data();
    }
  }
}

void
HTMLFormElement::RemoveFromRadioGroup(const nsAString& aName,
                                      HTMLInputElement* aRadio)
{
  if (aRadio->IsRequired()) {
    auto entry = mRequiredRadioButtonCounts.Lookup(aName);
    if (!entry) {
      MOZ_ASSERT_UNREACHABLE("At least one radio button has to be required!");
    } else {
      MOZ_ASSERT(entry.Data() >= 1,
                 "At least one radio button has to be required!");
      if (entry.Data() <= 1) {
        entry.Remove();
      } else {
        --entry.Data();
      }
    }
  }
}

uint32_t
HTMLFormElement::GetRequiredRadioCount(const nsAString& aName) const
{
  return mRequiredRadioButtonCounts.Get(aName);
}

void
HTMLFormElement::RadioRequiredWillChange(const nsAString& aName,
                                         bool aRequiredAdded)
{
  if (aRequiredAdded) {
    mRequiredRadioButtonCounts.Put(aName,
                                   mRequiredRadioButtonCounts.Get(aName)+1);
  } else {
    uint32_t requiredNb = mRequiredRadioButtonCounts.Get(aName);
    NS_ASSERTION(requiredNb >= 1,
                 "At least one radio button has to be required!");
    if (requiredNb == 1) {
      mRequiredRadioButtonCounts.Remove(aName);
    } else {
      mRequiredRadioButtonCounts.Put(aName, requiredNb-1);
    }
  }
}

bool
HTMLFormElement::GetValueMissingState(const nsAString& aName) const
{
  return mValueMissingRadioGroups.Get(aName);
}

void
HTMLFormElement::SetValueMissingState(const nsAString& aName, bool aValue)
{
  mValueMissingRadioGroups.Put(aName, aValue);
}

EventStates
HTMLFormElement::IntrinsicState() const
{
  EventStates state = nsGenericHTMLElement::IntrinsicState();

  if (mInvalidElementsCount) {
    state |= NS_EVENT_STATE_INVALID;
  } else {
      state |= NS_EVENT_STATE_VALID;
  }

  return state;
}

void
HTMLFormElement::Clear()
{
  for (int32_t i = mImageElements.Length() - 1; i >= 0; i--) {
    mImageElements[i]->ClearForm(false);
  }
  mImageElements.Clear();
  mImageNameLookupTable.Clear();
  mPastNameLookupTable.Clear();
}

namespace {

struct PositionComparator
{
  nsIContent* const mElement;
  explicit PositionComparator(nsIContent* const aElement) : mElement(aElement) {}

  int operator()(nsIContent* aElement) const {
    if (mElement == aElement) {
      return 0;
    }
    if (nsContentUtils::PositionIsBefore(mElement, aElement)) {
      return -1;
    }
    return 1;
  }
};

struct RadioNodeListAdaptor
{
  RadioNodeList* const mList;
  explicit RadioNodeListAdaptor(RadioNodeList* aList) : mList(aList) {}
  nsIContent* operator[](size_t aIdx) const {
    return mList->Item(aIdx);
  }
};

} // namespace

nsresult
HTMLFormElement::AddElementToTableInternal(
  nsInterfaceHashtable<nsStringHashKey,nsISupports>& aTable,
  nsIContent* aChild, const nsAString& aName)
{
  auto entry = aTable.LookupForAdd(aName);
  if (!entry) {
    // No entry found, add the element
    entry.OrInsert([&aChild]() { return aChild; });
    ++mExpandoAndGeneration.generation;
  } else {
    // Found something in the hash, check its type
    nsCOMPtr<nsIContent> content = do_QueryInterface(entry.Data());

    if (content) {
      // Check if the new content is the same as the one we found in the
      // hash, if it is then we leave it in the hash as it is, this will
      // happen if a form control has both a name and an id with the same
      // value
      if (content == aChild) {
        return NS_OK;
      }

      // Found an element, create a list, add the element to the list and put
      // the list in the hash
      RadioNodeList *list = new RadioNodeList(this);

      // If an element has a @form, we can assume it *might* be able to not have
      // a parent and still be in the form.
      NS_ASSERTION((content->IsElement() &&
                    content->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::form)) ||
                   content->GetParent(), "Item in list without parent");

      // Determine the ordering between the new and old element.
      bool newFirst = nsContentUtils::PositionIsBefore(aChild, content);

      list->AppendElement(newFirst ? aChild : content.get());
      list->AppendElement(newFirst ? content.get() : aChild);


      nsCOMPtr<nsISupports> listSupports = do_QueryObject(list);

      // Replace the element with the list.
      entry.Data() = listSupports;
    } else {
      // There's already a list in the hash, add the child to the list.
      MOZ_ASSERT(nsCOMPtr<RadioNodeList>(do_QueryInterface(entry.Data())));
      auto* list = static_cast<RadioNodeList*>(entry.Data().get());

      NS_ASSERTION(list->Length() > 1,
                   "List should have been converted back to a single element");

      // Fast-path appends; this check is ok even if the child is
      // already in the list, since if it tests true the child would
      // have come at the end of the list, and the PositionIsBefore
      // will test false.
      if (nsContentUtils::PositionIsBefore(list->Item(list->Length() - 1), aChild)) {
        list->AppendElement(aChild);
        return NS_OK;
      }

      // If a control has a name equal to its id, it could be in the
      // list already.
      if (list->IndexOf(aChild) != -1) {
        return NS_OK;
      }

      size_t idx;
      DebugOnly<bool> found = BinarySearchIf(RadioNodeListAdaptor(list), 0, list->Length(),
                                             PositionComparator(aChild), &idx);
      MOZ_ASSERT(!found, "should not have found an element");

      list->InsertElementAt(aChild, idx);
    }
  }

  return NS_OK;
}

nsresult
HTMLFormElement::AddImageElement(HTMLImageElement* aChild)
{
  AddElementToList(mImageElements, aChild, this);
  return NS_OK;
}

nsresult
HTMLFormElement::AddImageElementToTable(HTMLImageElement* aChild,
                                        const nsAString& aName)
{
  return AddElementToTableInternal(mImageNameLookupTable, aChild, aName);
}

nsresult
HTMLFormElement::RemoveImageElement(HTMLImageElement* aChild)
{
  RemoveElementFromPastNamesMap(aChild);

  size_t index = mImageElements.IndexOf(aChild);
  NS_ENSURE_STATE(index != mImageElements.NoIndex);

  mImageElements.RemoveElementAt(index);
  return NS_OK;
}

nsresult
HTMLFormElement::RemoveImageElementFromTable(HTMLImageElement* aElement,
                                             const nsAString& aName)
{
  return RemoveElementFromTableInternal(mImageNameLookupTable, aElement, aName);
}

void
HTMLFormElement::AddToPastNamesMap(const nsAString& aName,
                                   nsISupports* aChild)
{
  // If candidates contains exactly one node. Add a mapping from name to the
  // node in candidates in the form element's past names map, replacing the
  // previous entry with the same name, if any.
  nsCOMPtr<nsIContent> node = do_QueryInterface(aChild);
  if (node) {
    mPastNameLookupTable.Put(aName, node);
    node->SetFlags(MAY_BE_IN_PAST_NAMES_MAP);
  }
}

void
HTMLFormElement::RemoveElementFromPastNamesMap(Element* aElement)
{
  if (!aElement->HasFlag(MAY_BE_IN_PAST_NAMES_MAP)) {
    return;
  }

  aElement->UnsetFlags(MAY_BE_IN_PAST_NAMES_MAP);

  uint32_t oldCount = mPastNameLookupTable.Count();
  for (auto iter = mPastNameLookupTable.Iter(); !iter.Done(); iter.Next()) {
    if (aElement == iter.Data()) {
      iter.Remove();
    }
  }
  if (oldCount != mPastNameLookupTable.Count()) {
    ++mExpandoAndGeneration.generation;
  }
}

JSObject*
HTMLFormElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLFormElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
