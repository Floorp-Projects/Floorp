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

#ifndef nsIEditorIMESupport_h__
#define nsIEditorIMESupport_h__


#define NS_IEDITORIMESUPPORT_IID                   \
{ /* {4805e680-49b9-11d3-9ce4-ed60bd6cb5bc} */     \
0x4805e680, 0x49b9, 0x11d3,                        \
{ 0x9c, 0xe4, 0xed, 0x60, 0xbd, 0x6c, 0xb5, 0xbc } }


class nsIPrivateTextRangeList;
struct nsTextEventReply;
struct nsReconversionEventReply;

class nsIEditorIMESupport : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITORIMESUPPORT_IID; return iid; }


  /**
   * BeginComposition(nsTextEventReply* aReply) Handles the start of inline input composition.
   */

  NS_IMETHOD BeginComposition(nsTextEventReply *aReply) = 0;

  /**
   * SetCompositionString() Sets the inline input composition string.
   * BeginComposition must be called prior to this.
   */

  NS_IMETHOD SetCompositionString(const nsString& aCompositionString, nsIPrivateTextRangeList* aTextRange, nsTextEventReply* aReply) = 0;

  /**
   * EndComposition() Handles the end of inline input composition.
   */

  NS_IMETHOD EndComposition(void) = 0;

  /**
   * QueryComposition()  Get the composition position
   */
  NS_IMETHOD QueryComposition(nsTextEventReply *aReply) = 0;

  /**
   * ForceCompositionEnd() force the composition end
   */
  NS_IMETHOD ForceCompositionEnd() = 0;
  
  /**
   * GetReconvertionString()  Get the reconvertion string
   */
  NS_IMETHOD GetReconversionString(nsReconversionEventReply *aReply) = 0;
};


#endif // nsIEditorIMESupport_h__
