/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#ifndef nsContextMenuInfo_h__
#define nsContextMenuInfo_h__

#include "nsCOMPtr.h"
#include "nsIContextMenuListener2.h"
#include "nsIDOMNode.h"
#include "nsIDOMEvent.h"
#include "imgIContainer.h"
#include "imgIRequest.h"

class ChromeContextMenuListener;

//*****************************************************************************
// class nsContextMenuInfo
//
// Helper class for implementors of nsIContextMenuListener2
//*****************************************************************************
 
class nsContextMenuInfo : public nsIContextMenuInfo
{
  friend class ChromeContextMenuListener;
  
public:    
                    nsContextMenuInfo();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTEXTMENUINFO
  
private:
  virtual           ~nsContextMenuInfo();

  void              SetMouseEvent(nsIDOMEvent *aEvent)
                    { mMouseEvent = aEvent; }

  void              SetDOMNode(nsIDOMNode *aNode)
                    { mDOMNode = aNode; }
                    
  void              SetAssociatedLink(nsIDOMNode *aLink)
                    { mAssociatedLink = aLink; }

  nsresult          GetImageRequest(nsIDOMNode *aDOMNode,
                                    imgIRequest **aRequest);

  PRBool            HasBackgroundImage(nsIDOMNode *aDOMNode);

  nsresult          GetBackgroundImageRequest(nsIDOMNode *aDOMNode,
                                              imgIRequest **aRequest);

  nsresult          GetBackgroundImageRequestInternal(nsIDOMNode *aDOMNode,
                                                      imgIRequest **aRequest);
  
private:
  nsCOMPtr<nsIDOMEvent>   mMouseEvent;
  nsCOMPtr<nsIDOMNode>    mDOMNode;  
  nsCOMPtr<nsIDOMNode>    mAssociatedLink;

}; // class nsContextMenuInfo


#endif // nsContextMenuInfo_h__
