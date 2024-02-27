/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_SCRIPT_NSISCRIPTLOADEROBSERVER_H_
#define MOZILLA_DOM_SCRIPT_NSISCRIPTLOADEROBSERVER_H_

#include "nsISupports.h"
#include "js/GCAnnotations.h"

class nsIScriptElement;
class nsIURI;

#define NS_ISCRIPTLOADEROBSERVER_IID                 \
  {                                                  \
    0x7b787204, 0x76fb, 0x4764, {                    \
      0x96, 0xf1, 0xfb, 0x7a, 0x66, 0x6d, 0xb4, 0xf4 \
    }                                                \
  }

class NS_NO_VTABLE nsIScriptLoaderObserver : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTLOADEROBSERVER_IID)

  /**
   * The script is available for evaluation. For inline scripts, this
   * method will be called synchronously. For externally loaded scripts,
   * this method will be called when the load completes.
   *
   * @param aResult A result code representing the result of loading
   *        a script. If this is a failure code, script evaluation
   *        will not occur.
   * @param aElement The element being processed.
   * @param aIsInline Is this an inline classic script (as opposed to an
   *        externally loaded classic script or module script)?
   * @param aURI What is the URI of the script (the document URI if
   *        it is inline).
   * @param aLineNo At what line does the script appear (generally 1
   *        if it is a loaded script).
   */
  JS_HAZ_CAN_RUN_SCRIPT NS_IMETHOD ScriptAvailable(nsresult aResult,
                                                   nsIScriptElement* aElement,
                                                   bool aIsInlineClassicScript,
                                                   nsIURI* aURI,
                                                   uint32_t aLineNo) = 0;

  /**
   * The script has been evaluated.
   *
   * @param aResult A result code representing the success or failure of
   *        the script evaluation.
   * @param aElement The element being processed.
   * @param aIsInline Is this an inline script or externally loaded?
   */
  JS_HAZ_CAN_RUN_SCRIPT MOZ_CAN_RUN_SCRIPT NS_IMETHOD ScriptEvaluated(
      nsresult aResult, nsIScriptElement* aElement, bool aIsInline) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptLoaderObserver,
                              NS_ISCRIPTLOADEROBSERVER_IID)

#define NS_DECL_NSISCRIPTLOADEROBSERVER                                    \
  NS_IMETHOD ScriptAvailable(nsresult aResult, nsIScriptElement* aElement, \
                             bool aIsInlineClassicScript, nsIURI* aURI,    \
                             uint32_t aLineNo) override;                   \
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD ScriptEvaluated(                           \
      nsresult aResult, nsIScriptElement* aElement, bool aIsInline) override;

#endif  // MOZILLA_DOM_SCRIPT_NSISCRIPTLOADEROBSERVER_H_
