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
#include "nsIDOMHTMLScriptElement.h"
#include "mozilla/CORSMode.h"

#define NS_ISCRIPTELEMENT_IID \
{ 0x24ab3ff2, 0xd75e, 0x4be4, \
  { 0x8d, 0x50, 0xd6, 0x75, 0x31, 0x29, 0xab, 0x65 } }

/**
 * Internal interface implemented by script elements
 */
class nsIScriptElement : public nsIScriptLoaderObserver {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTELEMENT_IID)

  nsIScriptElement(mozilla::dom::FromParser aFromParser)
    : mLineNumber(0),
      mAlreadyStarted(false),
      mMalformed(false),
      mDoneAddingChildren(aFromParser == mozilla::dom::NOT_FROM_PARSER ||
                          aFromParser == mozilla::dom::FROM_PARSER_FRAGMENT),
      mForceAsync(aFromParser == mozilla::dom::NOT_FROM_PARSER ||
                  aFromParser == mozilla::dom::FROM_PARSER_FRAGMENT),
      mFrozen(false),
      mDefer(false),
      mAsync(false),
      mExternal(false),
      mParserCreated(aFromParser == mozilla::dom::FROM_PARSER_FRAGMENT ?
                     mozilla::dom::NOT_FROM_PARSER : aFromParser),
                     // Fragment parser-created scripts (if executable)
                     // behave like script-created scripts.
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
  bool GetScriptDeferred()
  {
    NS_PRECONDITION(mFrozen, "Not ready for this call yet!");
    return mDefer;
  }

  /**
   * Is the script async. Currently only supported by HTML scripts.
   */
  bool GetScriptAsync()
  {
    NS_PRECONDITION(mFrozen, "Not ready for this call yet!");
    return mAsync;  
  }

  /**
   * Is the script an external script?
   */
  bool GetScriptExternal()
  {
    NS_PRECONDITION(mFrozen, "Not ready for this call yet!");
    return mExternal;
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
    mMalformed = true;
  }
  bool IsMalformed()
  {
    return mMalformed;
  }

  void PreventExecution()
  {
    mAlreadyStarted = true;
  }

  void LoseParserInsertedness()
  {
    mFrozen = false;
    mUri = nsnull;
    mCreatorParser = nsnull;
    mParserCreated = mozilla::dom::NOT_FROM_PARSER;
    bool async = false;
    nsCOMPtr<nsIDOMHTMLScriptElement> htmlScript = do_QueryInterface(this);
    if (htmlScript) {
      htmlScript->GetAsync(&async);
    }
    mForceAsync = !async;
  }

  void SetCreatorParser(nsIParser* aParser)
  {
    mCreatorParser = getter_AddRefs(NS_GetWeakReference(aParser));
  }

  /**
   * Unblocks the creator parser
   */
  void UnblockParser()
  {
    nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
    if (parser) {
      parser->UnblockParser();
    }
  }

  /**
   * Attempts to resume parsing asynchronously
   */
  void ContinueParserAsync()
  {
    nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
    if (parser) {
      parser->ContinueInterruptedParsingAsync();
    }
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

  /**
   * This method is called when the parser finishes creating the script
   * element's children, if any are present.
   *
   * @return whether the parser will be blocked while this script is being
   *         loaded
   */
  bool AttemptToExecute()
  {
    mDoneAddingChildren = true;
    bool block = MaybeProcessScript();
    if (!mAlreadyStarted) {
      // Need to lose parser-insertedness here to allow another script to cause
      // execution later.
      LoseParserInsertedness();
    }
    return block;
  }

  /**
   * Get the CORS mode of the script element
   */
  virtual mozilla::CORSMode GetCORSMode() const
  {
    /* Default to no CORS */
    return mozilla::CORS_NONE;
  }

protected:
  /**
   * Processes the script if it's in the document-tree and links to or
   * contains a script. Once it has been evaluated there is no way to make it
   * reevaluate the script, you'll have to create a new element. This also means
   * that when adding a src attribute to an element that already contains an
   * inline script, the script referenced by the src attribute will not be
   * loaded.
   *
   * In order to be able to use multiple childNodes, or to use the
   * fallback mechanism of using both inline script and linked script you have
   * to add all attributes and childNodes before adding the element to the
   * document-tree.
   *
   * @return whether the parser will be blocked while this script is being
   *         loaded
   */
  virtual bool MaybeProcessScript() = 0;

  /**
   * The start line number of the script.
   */
  PRUint32 mLineNumber;
  
  /**
   * The "already started" flag per HTML5.
   */
  bool mAlreadyStarted;
  
  /**
   * The script didn't have an end tag.
   */
  bool mMalformed;
  
  /**
   * False if parser-inserted but the parser hasn't triggered running yet.
   */
  bool mDoneAddingChildren;

  /**
   * If true, the .async property returns true instead of reflecting the
   * content attribute.
   */
  bool mForceAsync;

  /**
   * Whether src, defer and async are frozen.
   */
  bool mFrozen;
  
  /**
   * The effective deferredness.
   */
  bool mDefer;
  
  /**
   * The effective asyncness.
   */
  bool mAsync;
  
  /**
   * The effective externalness. A script can be external with mUri being null
   * if the src attribute contained an invalid URL string.
   */
  bool mExternal;

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
