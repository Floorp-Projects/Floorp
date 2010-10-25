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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com> (original author)
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

#ifndef nsIScriptElement_h___
#define nsIScriptElement_h___

#include "nsISupports.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsIScriptLoaderObserver.h"
#include "nsWeakPtr.h"
#include "nsIParser.h"
#include "nsContentCreatorFunctions.h"

#define NS_ISCRIPTELEMENT_IID \
{ 0x6d625b30, 0xfac4, 0x11de, \
{ 0x8a, 0x39, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 } }

/**
 * Internal interface implemented by script elements
 */
class nsIScriptElement : public nsIScriptLoaderObserver {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTELEMENT_IID)

  nsIScriptElement(mozilla::dom::FromParser aFromParser)
    : mLineNumber(0),
      mAlreadyStarted(PR_FALSE),
      mMalformed(PR_FALSE),
      mDoneAddingChildren(PR_TRUE),
      mFrozen(PR_FALSE),
      mDefer(PR_FALSE),
      mAsync(PR_FALSE),
      mParserCreated(aFromParser),
      mCreatorParser(nsnull)
  {
  }

  /**
   * Content type identifying the scripting language. Can be empty, in
   * which case javascript will be assumed.
   */
  virtual void GetScriptType(nsAString& type) = 0;
    
  /**
   * Location of script source text. Can return null, in which case
   * this is assumed to be an inline script element.
   */
  nsIURI* GetScriptURI()
  {
    NS_PRECONDITION(mFrozen, "Not ready for this call yet!");
    return mUri;
  }
  
  /**
   * Script source text for inline script elements.
   */
  virtual void GetScriptText(nsAString& text) = 0;

  virtual void GetScriptCharset(nsAString& charset) = 0;

  /**
   * Freezes the return values of GetScriptDeferred(), GetScriptAsync() and
   * GetScriptURI() so that subsequent modifications to the attributes don't
   * change execution behavior.
   */
  virtual void FreezeUriAsyncDefer() = 0;

  /**
   * Is the script deferred. Currently only supported by HTML scripts.
   */
  PRBool GetScriptDeferred()
  {
    NS_PRECONDITION(mFrozen, "Not ready for this call yet!");
    return mDefer;
  }

  /**
   * Is the script async. Currently only supported by HTML scripts.
   */
  PRBool GetScriptAsync()
  {
    NS_PRECONDITION(mFrozen, "Not ready for this call yet!");
    return mAsync;  
  }

  /**
   * Returns how the element was created.
   */
  mozilla::dom::FromParser GetParserCreated()
  {
    return mParserCreated;
  }

  void SetScriptLineNumber(PRUint32 aLineNumber)
  {
    mLineNumber = aLineNumber;
  }
  PRUint32 GetScriptLineNumber()
  {
    return mLineNumber;
  }

  void SetIsMalformed()
  {
    mMalformed = PR_TRUE;
  }
  PRBool IsMalformed()
  {
    return mMalformed;
  }

  void PreventExecution()
  {
    mAlreadyStarted = PR_TRUE;
  }

  void LoseParserInsertedness()
  {
    mFrozen = PR_FALSE;
    mUri = nsnull;
    mCreatorParser = nsnull;
    mParserCreated = mozilla::dom::NOT_FROM_PARSER;
  }

  void SetCreatorParser(nsIParser* aParser)
  {
    mCreatorParser = getter_AddRefs(NS_GetWeakReference(aParser));
  }

  /**
   * Informs the creator parser that the evaluation of this script is starting
   */
  void BeginEvaluating()
  {
    nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
    if (parser) {
      parser->BeginEvaluatingParserInsertedScript();
    }
  }

  /**
   * Informs the creator parser that the evaluation of this script is ending
   */
  void EndEvaluating()
  {
    nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
    if (parser) {
      parser->EndEvaluatingParserInsertedScript();
    }
  }
  
  /**
   * Retrieves a pointer to the creator parser if this has one or null if not
   */
  already_AddRefed<nsIParser> GetCreatorParser()
  {
    nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
    return parser.forget();
  }

protected:
  /**
   * The start line number of the script.
   */
  PRUint32 mLineNumber;
  
  /**
   * The "already started" flag per HTML5.
   */
  PRPackedBool mAlreadyStarted;
  
  /**
   * The script didn't have an end tag.
   */
  PRPackedBool mMalformed;
  
  /**
   * False if parser-inserted but the parser hasn't triggered running yet.
   */
  PRPackedBool mDoneAddingChildren;

  /**
   * Whether src, defer and async are frozen.
   */
  PRPackedBool mFrozen;
  
  /**
   * The effective deferredness.
   */
  PRPackedBool mDefer;
  
  /**
   * The effective asyncness.
   */
  PRPackedBool mAsync;
  
  /**
   * Whether this element was parser-created.
   */
  mozilla::dom::FromParser mParserCreated;

  /**
   * The effective src (or null if no src).
   */
  nsCOMPtr<nsIURI> mUri;
  
  /**
   * The creator parser of a non-defer, non-async parser-inserted script.
   */
  nsWeakPtr mCreatorParser;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptElement, NS_ISCRIPTELEMENT_IID)

#endif // nsIScriptElement_h___
