/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXSTYLESHEETCOMPILEHANDLERS_H
#define TRANSFRMX_TXSTYLESHEETCOMPILEHANDLERS_H

#include "nsError.h"
#include "txNamespaceMap.h"
#include "txExpandedNameMap.h"

struct txStylesheetAttr;
class txStylesheetCompilerState;

using HandleStartFn = nsresult (*)(int32_t aNamespaceID, nsAtom* aLocalName,
                                   nsAtom* aPrefix,
                                   txStylesheetAttr* aAttributes,
                                   int32_t aAttrCount,
                                   txStylesheetCompilerState& aState);
using HandleEndFn = void (*)(txStylesheetCompilerState& aState);
using HandleTextFn = nsresult (*)(const nsAString& aStr,
                                  txStylesheetCompilerState& aState);

struct txElementHandler {
  int32_t mNamespaceID;
  const char* mLocalName;
  HandleStartFn mStartFunction;
  HandleEndFn mEndFunction;
};

class txHandlerTable {
 public:
  txHandlerTable(const HandleTextFn aTextHandler,
                 const txElementHandler* aLREHandler,
                 const txElementHandler* aOtherHandler);
  nsresult init(const txElementHandler* aHandlers, uint32_t aCount);
  const txElementHandler* find(int32_t aNamespaceID, nsAtom* aLocalName);

  const HandleTextFn mTextHandler;
  const txElementHandler* const mLREHandler;

  static bool init();
  static void shutdown();

 private:
  const txElementHandler* const mOtherHandler;
  txExpandedNameMap<const txElementHandler> mHandlers;
};

extern txHandlerTable* gTxRootHandler;
extern txHandlerTable* gTxEmbedHandler;

#endif  // TRANSFRMX_TXSTYLESHEETCOMPILEHANDLERS_H
