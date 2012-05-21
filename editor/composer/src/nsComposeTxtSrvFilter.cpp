/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComposeTxtSrvFilter.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"

nsComposeTxtSrvFilter::nsComposeTxtSrvFilter() :
  mIsForMail(false)
{

  mBlockQuoteAtom  = do_GetAtom("blockquote");
  mSpanAtom        = do_GetAtom("span");
  mTableAtom       = do_GetAtom("table");
  mMozQuoteAtom    = do_GetAtom("_moz_quote");
  mClassAtom       = do_GetAtom("class");
  mTypeAtom        = do_GetAtom("type");
  mScriptAtom      = do_GetAtom("script");
  mTextAreaAtom    = do_GetAtom("textarea");
  mSelectAreaAtom  = do_GetAtom("select");
  mMapAtom         = do_GetAtom("map");
  mCiteAtom        = do_GetAtom("cite");
  mTrueAtom        = do_GetAtom("true");
  mMozSignatureAtom= do_GetAtom("moz-signature");
}

NS_IMPL_ISUPPORTS1(nsComposeTxtSrvFilter, nsITextServicesFilter)

NS_IMETHODIMP 
nsComposeTxtSrvFilter::Skip(nsIDOMNode* aNode, bool *_retval)
{
  *_retval = false;

  // Check to see if we can skip this node
  // For nodes that are blockquotes, we must make sure
  // their type is "cite"
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (content) {
    nsIAtom *tag = content->Tag();
    if (tag == mBlockQuoteAtom) {
      if (mIsForMail) {
        *_retval = content->AttrValueIs(kNameSpaceID_None, mTypeAtom,
                                        mCiteAtom, eIgnoreCase);
      }
    } else if (tag == mSpanAtom) {
      if (mIsForMail) {
        *_retval = content->AttrValueIs(kNameSpaceID_None, mMozQuoteAtom,
                                        mTrueAtom, eIgnoreCase);
        if (!*_retval) {
          *_retval = content->AttrValueIs(kNameSpaceID_None, mClassAtom,
                                          mMozSignatureAtom, eCaseMatters);
        }
      }         
    } else if (tag == mScriptAtom ||
               tag == mTextAreaAtom ||
               tag == mSelectAreaAtom ||
               tag == mMapAtom) {
      *_retval = true;
    } else if (tag == mTableAtom) {
      if (mIsForMail) {
        *_retval =
          content->AttrValueIs(kNameSpaceID_None, mClassAtom,
                               NS_LITERAL_STRING("moz-email-headers-table"),
                               eCaseMatters);
      } 
    }
  }

  return NS_OK;
}
