/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIEditRules_h__
#define nsIEditRules_h__

#include "nsISupports.h"
#include "nscore.h"

class nsIAtom;
class nsIDOMNode;
class nsIEditor;
class nsIDOMSelection;

#define NS_IEDITRULES_IID \
{/* d64999a0-d255-11d2-86dc-000064657374*/ \
0xd64999a0, 0xd255, 0x11d2, \
{0x86, 0xdc, 0x0, 0x00, 0x64, 0x65, 0x73, 0x74} }

#define NS_IEDITRULESFACTORY_IID \
{ /* f6ab69d0-d255-11d2-86dc-000064657374*/ \
0xf6ab69d0, 0xd255, 0x11d2, \
{ 0x86, 0xdc, 0x0, 0x00, 0x64, 0x65, 0x73, 0x74} }

/** Object that encapsulates content-specific editing rules.
  *  
  * To be a good citizen, edit rules must live by these restrictions:
  * 1. All data manipulation is through the editor.  
  *    Content nodes in the document tree must <B>not</B> be manipulated directly.
  *    Content nodes in document fragments that are not part of the document itself
  *    may be manipulated at will.  Operations on document fragments must <B>not</B>
  *    go through the editor.
  * 2. Selection must not be explicitly set by the rule method.  
  *    Any manipulation of Selection must be done by the editor.
  */
class nsIEditRules  : public nsISupports
{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_IEDITRULES_IID; return iid; }

  /** General Initialization of the rules for a content type
    * @param aEditor    The editor for the document
    * @param aNextRule  The next rule in the rules chain.
    *                   Should be called if this rules object doesn't handle the request.
    */
  NS_IMETHOD Init(nsIEditor *aEditor, nsIEditRules *aNextRule)=0;

  /** notification that InsertBreak is about to start. 
    * @param aSelection   The selection where insertion will take place
    * @param aCancel      [OUT] PR_FALSE if the insert break should continue.
    *                           PR_TRUE if the insert break should be aborted.
    */
  NS_IMETHOD WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel)=0;

  /** notification that InsertBreak is completed. 
    * @param aSelection   The selection immediately following the insertion of the break.
    */
  NS_IMETHOD DidInsertBreak(nsIDOMSelection *aSelection, nsresult aResult)=0;

  /** return the tag for the content to be created for the break. 
    * @param aTag  [OUT] the tag used to create the content that represents a break.
    */
  NS_IMETHOD GetInsertBreakTag(nsIAtom **aTag)=0;
  

};

#endif //nsIEditRules_h__

