/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsComposeTxtSrvFilter.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"

nsComposeTxtSrvFilter::nsComposeTxtSrvFilter() :
  mIsForMail(PR_FALSE)
{

  mBlockQuoteAtom  = do_GetAtom("blockquote");
  mPreAtom         = do_GetAtom("pre");
  mSpanAtom        = do_GetAtom("span");
  mMozQuoteAtom    = do_GetAtom("_moz_quote");
  mTypeAtom        = do_GetAtom("type");
  mScriptAtom      = do_GetAtom("script");
  mTextAreaAtom    = do_GetAtom("textarea");
  mSelectAreaAtom  = do_GetAtom("select");
  mMapAtom         = do_GetAtom("map");
}

NS_IMPL_ISUPPORTS1(nsComposeTxtSrvFilter, nsITextServicesFilter)

NS_IMETHODIMP 
nsComposeTxtSrvFilter::Skip(nsIDOMNode* aNode, PRBool *_retval)
{
  *_retval = PR_FALSE;

  // Check to see if we can skip this node
  // For nodes that are blockquotes, we must make sure
  // their type is "cite"
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (content) {
    nsIAtom *tag = content->Tag();
    if (tag == mBlockQuoteAtom) {
      if (mIsForMail) {
        nsAutoString cite;
        if (NS_SUCCEEDED(content->GetAttr(kNameSpaceID_None, mTypeAtom, cite))) {
          *_retval = cite.LowerCaseEqualsLiteral("cite");
        }
      }
    } else if (tag == mPreAtom || tag == mSpanAtom) {
      if (mIsForMail) {
        nsAutoString mozQuote;
        if (NS_SUCCEEDED(content->GetAttr(kNameSpaceID_None, mMozQuoteAtom, mozQuote))) {
          *_retval = mozQuote.LowerCaseEqualsLiteral("true");            
        }
      }         
    } else if (tag == mScriptAtom ||
               tag == mTextAreaAtom ||
               tag == mSelectAreaAtom ||
               tag == mMapAtom) {
      *_retval = PR_TRUE;
    }
  }

  return NS_OK;
}
