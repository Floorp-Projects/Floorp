/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsTextEditUtils_h__
#define nsTextEditUtils_h__

#include "prtypes.h"  // for PRBool
#include "nsError.h"  // for nsresult
#include "nsString.h" // for nsAReadableString
class nsIDOMNode;
class nsIEditor;
class nsPlaintextEditor;

class nsTextEditUtils
{
public:
  static PRBool NodeIsType(nsIDOMNode *aNode, const nsAReadableString& aTag);

  // from nsTextEditRules:
  static PRBool IsBody(nsIDOMNode *aNode);
  static PRBool IsBreak(nsIDOMNode *aNode);
  static PRBool IsMozBR(nsIDOMNode *aNode);
  static PRBool HasMozAttr(nsIDOMNode *aNode);
  static PRBool InBody(nsIDOMNode *aNode, nsIEditor *aEditor);
};

/***************************************************************************
 * stack based helper class for detecting end of editor initialization, in
 * order to triger "end of init" initialization of the edit rules.
 */
class nsAutoEditInitRulesTrigger
{
  private:
    nsPlaintextEditor *mEd;
    nsresult &mRes;
  public:
  nsAutoEditInitRulesTrigger( nsPlaintextEditor *aEd, nsresult &aRes);
  ~nsAutoEditInitRulesTrigger();
};


#endif /* nsTextEditUtils_h__ */

