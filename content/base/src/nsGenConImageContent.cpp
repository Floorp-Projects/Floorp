
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
#include "nsXMLElement.h"
#include "nsImageLoadingContent.h"
#include "imgIRequest.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"

using namespace mozilla;

class nsGenConImageContent MOZ_FINAL : public nsXMLElement,
                                       public nsImageLoadingContent
{
public:
  explicit nsGenConImageContent(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsXMLElement(aNodeInfo)
  {
    // nsImageLoadingContent starts out broken, so we start out
    // suppressed to match it.
    AddStatesSilently(NS_EVENT_STATE_SUPPRESSED);
  }

  nsresult Init(imgRequestProxy* aImageRequest)
  {
    // No need to notify, since we have no frame.
    return UseAsPrimaryRequest(aImageRequest, false, eImageLoadType_Normal);
  }

  // nsIContent overrides
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);
  virtual EventStates IntrinsicState() const;

  virtual nsresult PreHandleEvent(EventChainPreVisitor& aVisitor)
  {
    MOZ_ASSERT(IsInNativeAnonymousSubtree());
    if (aVisitor.mEvent->message == NS_LOAD ||
        aVisitor.mEvent->message == NS_LOAD_ERROR) {
      // Don't propagate the events to the parent.
      return NS_OK;
    }
    return nsXMLElement::PreHandleEvent(aVisitor);
  }
  
private:
  virtual ~nsGenConImageContent();

public:
  NS_DECL_ISUPPORTS_INHERITED
};

NS_IMPL_ISUPPORTS_INHERITED(nsGenConImageContent,
                            nsXMLElement,
                            nsIImageLoadingContent,
                            imgINotificationObserver,
                            imgIOnloadBlocker)

nsresult
NS_NewGenConImageContent(nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                         imgRequestProxy* aImageRequest)
{
  NS_PRECONDITION(aImageRequest, "Must have request!");
  nsGenConImageContent *it = new nsGenConImageContent(aNodeInfo);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult = it);
  nsresult rv = it->Init(aImageRequest);
  if (NS_FAILED(rv))
    NS_RELEASE(*aResult);
  return rv;
}

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
  rv = nsXMLElement::BindToTree(aDocument, aParent, aBindingParent,
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
  nsXMLElement::UnbindFromTree(aDeep, aNullParent);
}

EventStates
nsGenConImageContent::IntrinsicState() const
{
  EventStates state = nsXMLElement::IntrinsicState();

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
