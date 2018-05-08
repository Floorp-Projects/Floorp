/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A fake content node class so that the image frame for
 *   content: url(foo);
 * in CSS can have an nsIImageLoadingContent but use an
 * imgIRequest that's already been loaded from the style system.
 */

#include "nsContentCreatorFunctions.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsImageLoadingContent.h"
#include "imgIRequest.h"
#include "nsNodeInfoManager.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/NameSpaceConstants.h"

using namespace mozilla;

class nsGenConImageContent final : public nsGenericHTMLElement,
                                   public nsImageLoadingContent
{
public:
  explicit nsGenConImageContent(already_AddRefed<dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
    // nsImageLoadingContent starts out broken, so we start out
    // suppressed to match it.
    AddStatesSilently(NS_EVENT_STATE_SUPPRESSED);
    MOZ_ASSERT(IsInNamespace(kNameSpaceID_XHTML), "Someone messed up our nodeinfo");
  }

  nsresult Init(imgRequestProxy* aImageRequest)
  {
    // No need to notify, since we have no frame.
    return UseAsPrimaryRequest(aImageRequest, false, eImageLoadType_Normal);
  }

  // nsIContent overrides
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;
  virtual EventStates IntrinsicState() const override;

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override
  {
    MOZ_ASSERT(IsInNativeAnonymousSubtree());
    if (aVisitor.mEvent->mMessage == eLoad ||
        aVisitor.mEvent->mMessage == eLoadError) {
      // Don't propagate the events to the parent.
      return;
    }
    nsGenericHTMLElement::GetEventTargetParent(aVisitor);
  }

protected:
  nsIContent* AsContent() override { return this; }

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
  virtual nsresult Clone(dom::NodeInfo* aNodeInfo,
                         nsINode** aResult,
                         bool aPreallocateChildren) const override;

private:
  virtual ~nsGenConImageContent();

public:
  NS_DECL_ISUPPORTS_INHERITED
};

NS_IMPL_ISUPPORTS_INHERITED(nsGenConImageContent,
                            nsGenericHTMLElement,
                            nsIImageLoadingContent,
                            imgINotificationObserver)

NS_IMPL_ELEMENT_CLONE(nsGenConImageContent)

namespace mozilla {
namespace dom {

already_AddRefed<nsIContent>
CreateGenConImageContent(nsIDocument* aDocument, imgRequestProxy* aImageRequest)
{
  MOZ_ASSERT(aImageRequest, "Must have request!");
  RefPtr<NodeInfo> nodeInfo =
    aDocument->NodeInfoManager()->
      GetNodeInfo(nsGkAtoms::mozgeneratedcontentimage,
                  nullptr,
                  kNameSpaceID_XHTML,
                  nsINode::ELEMENT_NODE);
  // Work around not being able to bind a non-const lvalue reference
  // to an rvalue of non-reference type by just creating an rvalue
  // reference.  And we can't change the constructor signature,
  // because then the macro-generated Clone() method fails to compile.
  already_AddRefed<NodeInfo>&& rvalue = nodeInfo.forget();
  RefPtr<nsGenConImageContent> it = new nsGenConImageContent(rvalue);
  nsresult rv = it->Init(aImageRequest);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return it.forget();
}

} // namespace dom
} // namespace mozilla

nsGenConImageContent::~nsGenConImageContent()
{
  DestroyImageLoadingContent();
}

nsresult
nsGenConImageContent::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                 nsIContent* aBindingParent,
                                 bool aCompileEventHandlers)
{
  nsresult rv;
  rv = nsGenericHTMLElement::BindToTree(aDocument, aParent, aBindingParent,
                                aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsImageLoadingContent::BindToTree(aDocument, aParent, aBindingParent,
                                    aCompileEventHandlers);
  return NS_OK;
}

void
nsGenConImageContent::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsImageLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

EventStates
nsGenConImageContent::IntrinsicState() const
{
  EventStates state = nsGenericHTMLElement::IntrinsicState();

  EventStates imageState = nsImageLoadingContent::ImageState();
  if (imageState.HasAtLeastOneOfStates(NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_USERDISABLED)) {
    // We should never be in an error state; if the image fails to load, we
    // just go to the suppressed state.
    imageState |= NS_EVENT_STATE_SUPPRESSED;
    imageState &= ~NS_EVENT_STATE_BROKEN;
  }
  imageState &= ~NS_EVENT_STATE_LOADING;
  return state | imageState;
}

JSObject*
nsGenConImageContent::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::HTMLElementBinding::Wrap(aCx, this, aGivenProto);
}

