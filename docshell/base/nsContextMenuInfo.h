/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContextMenuInfo_h__
#define nsContextMenuInfo_h__

#include "nsCOMPtr.h"
#include "nsIContextMenuListener2.h"
#include "nsIDOMNode.h"
#include "nsIDOMEvent.h"
#include "imgIContainer.h"
#include "imgIRequest.h"

class ChromeContextMenuListener;
class imgRequestProxy;

// Helper class for implementors of nsIContextMenuListener2
class nsContextMenuInfo : public nsIContextMenuInfo
{
  friend class ChromeContextMenuListener;

public:
  nsContextMenuInfo();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTEXTMENUINFO

private:
  virtual ~nsContextMenuInfo();

  void SetMouseEvent(nsIDOMEvent* aEvent) { mMouseEvent = aEvent; }
  void SetDOMNode(nsIDOMNode* aNode) { mDOMNode = aNode; }
  void SetAssociatedLink(nsIDOMNode* aLink) { mAssociatedLink = aLink; }

  nsresult GetImageRequest(nsIDOMNode* aDOMNode, imgIRequest** aRequest);

  bool HasBackgroundImage(nsIDOMNode* aDOMNode);

  nsresult GetBackgroundImageRequest(nsIDOMNode* aDOMNode,
                                     imgRequestProxy** aRequest);

  nsresult GetBackgroundImageRequestInternal(nsIDOMNode* aDOMNode,
                                             imgRequestProxy** aRequest);

private:
  nsCOMPtr<nsIDOMEvent> mMouseEvent;
  nsCOMPtr<nsIDOMNode> mDOMNode;
  nsCOMPtr<nsIDOMNode> mAssociatedLink;
};

#endif // nsContextMenuInfo_h__
