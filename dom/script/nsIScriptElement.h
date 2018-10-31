/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptElement_h___
#define nsIScriptElement_h___

#include "nsISupports.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsIScriptLoaderObserver.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIParser.h"
#include "nsIContent.h"
#include "nsContentCreatorFunctions.h"
#include "mozilla/CORSMode.h"

// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_ISCRIPTELEMENT_IID \
{ 0xe60fca9b, 0x1b96, 0x4e4e, \
 { 0xa9, 0xb4, 0xdc, 0x98, 0x4f, 0x88, 0x3f, 0x9c } }

/**
 * Internal interface implemented by script elements
 */
class nsIScriptElement : public nsIScriptLoaderObserver
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTELEMENT_IID)

  explicit nsIScriptElement(mozilla::dom::FromParser aFromParser)
    : mLineNumber(1),
      mColumnNumber(1),
      mAlreadyStarted(false),
      mMalformed(false),
      mDoneAddingChildren(aFromParser == mozilla::dom::NOT_FROM_PARSER ||
                          aFromParser == mozilla::dom::FROM_PARSER_FRAGMENT),
      mForceAsync(aFromParser == mozilla::dom::NOT_FROM_PARSER ||
                  aFromParser == mozilla::dom::FROM_PARSER_FRAGMENT),
      mFrozen(false),
      mIsModule(false),
      mDefer(false),
      mAsync(false),
      mExternal(false),
      mParserCreated(aFromParser == mozilla::dom::FROM_PARSER_FRAGMENT ?
                     mozilla::dom::NOT_FROM_PARSER : aFromParser),
                     // Fragment parser-created scripts (if executable)
                     // behave like script-created scripts.
      mCreatorParser(nullptr)
  {
  }

  /**
   * Content type identifying the scripting language. Can be empty, in
   * which case javascript will be assumed.
   * Return false if type attribute is not found.
   */
  virtual bool GetScriptType(nsAString& type) = 0;

  /**
   * Location of script source text. Can return null, in which case
   * this is assumed to be an inline script element.
   */
  nsIURI* GetScriptURI()
  {
    MOZ_ASSERT(mFrozen, "Not ready for this call yet!");
    return mUri;
  }

  nsIPrincipal* GetScriptURITriggeringPrincipal()
  {
    MOZ_ASSERT(mFrozen, "Not ready for this call yet!");
    return mSrcTriggeringPrincipal;
  }

  /**
   * Script source text for inline script elements.
   */
  virtual void GetScriptText(nsAString& text) = 0;

  virtual void GetScriptCharset(nsAString& charset) = 0;

  /**
   * Freezes the return values of the following methods so that subsequent
   * modifications to the attributes don't change execution behavior:
   *  - GetScriptIsModule()
   *  - GetScriptDeferred()
   *  - GetScriptAsync()
   *  - GetScriptURI()
   *  - GetScriptExternal()
   */
  virtual void FreezeExecutionAttrs(nsIDocument* aOwnerDoc) = 0;

  /**
   * Is the script a module script. Currently only supported by HTML scripts.
   */
  bool GetScriptIsModule()
  {
    MOZ_ASSERT(mFrozen, "Not ready for this call yet!");
    return mIsModule;
  }

  /**
   * Is the script deferred. Currently only supported by HTML scripts.
   */
  bool GetScriptDeferred()
  {
    MOZ_ASSERT(mFrozen, "Not ready for this call yet!");
    return mDefer;
  }

  /**
   * Is the script async. Currently only supported by HTML scripts.
   */
  bool GetScriptAsync()
  {
    MOZ_ASSERT(mFrozen, "Not ready for this call yet!");
    return mAsync;
  }

  /**
   * Is the script an external script?
   */
  bool GetScriptExternal()
  {
    MOZ_ASSERT(mFrozen, "Not ready for this call yet!");
    return mExternal;
  }

  /**
   * Returns how the element was created.
   */
  mozilla::dom::FromParser GetParserCreated()
  {
    return mParserCreated;
  }

  void SetScriptLineNumber(uint32_t aLineNumber)
  {
    mLineNumber = aLineNumber;
  }

  uint32_t GetScriptLineNumber()
  {
    return mLineNumber;
  }

  void SetScriptColumnNumber(uint32_t aColumnNumber)
  {
    mColumnNumber = aColumnNumber;
  }

  uint32_t GetScriptColumnNumber()
  {
    return mColumnNumber;
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
    mUri = nullptr;
    mCreatorParser = nullptr;
    mParserCreated = mozilla::dom::NOT_FROM_PARSER;
    mForceAsync = !GetAsyncState();

    // Reset state set by FreezeExecutionAttrs().
    mFrozen = false;
    mIsModule = false;
    mExternal = false;
    mAsync = false;
    mDefer = false;
  }

  void SetCreatorParser(nsIParser* aParser)
  {
    mCreatorParser = do_GetWeakReference(aParser);
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
      parser->PushDefinedInsertionPoint();
    }
  }

  /**
   * Informs the creator parser that the evaluation of this script is ending
   */
  void EndEvaluating()
  {
    nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
    if (parser) {
      parser->PopDefinedInsertionPoint();
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

  /**
   * Fire an error event
   */
  virtual nsresult FireErrorEvent() = 0;

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
   * Since we've removed the XPCOM interface to HTML elements, we need a way to
   * retreive async state from script elements without bringing the type in.
   */
  virtual bool GetAsyncState() = 0;

  /**
   * The start line number of the script.
   */
  uint32_t mLineNumber;

  /**
   * The start column number of the script.
   */
  uint32_t mColumnNumber;

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
   * The effective moduleness.
   */
  bool mIsModule;

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
   * The triggering principal for the src URL.
   */
  nsCOMPtr<nsIPrincipal> mSrcTriggeringPrincipal;

  /**
   * The creator parser of a non-defer, non-async parser-inserted script.
   */
  nsWeakPtr mCreatorParser;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptElement, NS_ISCRIPTELEMENT_IID)

#endif // nsIScriptElement_h___
