/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Oleg Romashin. Portions created by Oleg Romashin are Copyright (C) Oleg Romashin.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#ifndef EmbedContextMenuInfo_h__
#define EmbedContextMenuInfo_h__
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMEvent.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "nsIDOMEventTarget.h"
#include "nsRect.h"
// for strings
#ifdef MOZILLA_INTERNAL_API
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#endif
#include "EmbedWindow.h"
#include "nsIDOMNSHTMLElement.h"
//*****************************************************************************
// class EmbedContextMenuInfo
//
//*****************************************************************************
class EmbedContextMenuInfo : public nsISupports
{
public:
  EmbedContextMenuInfo(EmbedPrivate *aOwner);
  virtual ~EmbedContextMenuInfo(void);
  NS_DECL_ISUPPORTS
  nsresult          GetFormControlType(nsIDOMEvent *aDOMEvent);
  nsresult          UpdateContextData(nsIDOMEvent *aDOMEvent);
  nsresult          UpdateContextData(void *aEvent);
  const char*       GetSelectedText();
  nsresult          GetElementForScroll(nsIDOMDocument *targetDOMDocument);
  nsresult          GetElementForScroll(nsIDOMEvent *aEvent);
  nsresult          CheckDomImageElement(nsIDOMNode *node, nsString& aHref,
                                       PRInt32 *aWidth, PRInt32 *aHeight);
  nsresult          GetImageRequest(imgIRequest **aRequest, nsIDOMNode *aDOMNode);
  nsString          GetCtxDocTitle(void) { return mCtxDocTitle; };


  PRInt32                 mX, mY, mObjWidth, mObjHeight, mCtxFrameNum;
  nsString                mCtxURI, mCtxHref, mCtxImgHref;
  PRUint32                mEmbedCtxType;
  PRInt32 mCtxFormType;
  nsCOMPtr<nsIDOMNode>    mEventNode;
  nsCOMPtr<nsIDOMEventTarget> mEventTarget;
  nsCOMPtr<nsIDOMDocument>mCtxDocument;
  nsRect               mFormRect;
  nsCOMPtr<nsIDOMWindow>  mCtxDomWindow;
  nsCOMPtr<nsIDOMEvent>   mCtxEvent;
  nsCOMPtr<nsIDOMNSHTMLElement> mNSHHTMLElement;
  nsCOMPtr<nsIDOMNSHTMLElement> mNSHHTMLElementSc;
private:
  nsresult          SetFrameIndex();
  nsresult          SetFormControlType(nsIDOMEventTarget *originalTarget);
  nsresult          CheckDomHtmlNode(nsIDOMNode *aNode = nsnull);

  EmbedPrivate           *mOwner;
  nsCOMPtr<nsIDOMNode>    mOrigNode;
  nsString                mCtxDocTitle;
}; // class EmbedContextMenuInfo
#endif // EmbedContextMenuInfo_h__
