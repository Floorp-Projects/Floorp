/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TextEditorTest_h__
#define __TextEditorTest_h__

#include "nscore.h"
#include "prtypes.h"

class nsIEditor;
class nsIPlaintextEditor;
#ifdef DEBUG

#include "nsCOMPtr.h"

class TextEditorTest
{
public:

  void Run(nsIEditor *aEditor, PRInt32 *outNumTests, PRInt32 *outNumTestsFailed);
  TextEditorTest();
  ~TextEditorTest();

protected:

  /** create an empty document */
  nsresult InitDoc();

  nsresult RunUnitTest(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed);

  nsresult TestInsertBreak();

  nsresult TestTextProperties();

  nsCOMPtr<nsIPlaintextEditor> mTextEditor;
  nsCOMPtr<nsIEditor> mEditor;
};

#endif /* DEBUG */

#endif
