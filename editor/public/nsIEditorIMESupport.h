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

#ifndef nsIEditorIMESupport_h__
#define nsIEditorIMESupport_h__


#define NS_IEDITORIMESUPPORT_IID                   \
{ /* {4805e680-49b9-11d3-9ce4-ed60bd6cb5bc} */     \
0x4805e680, 0x49b9, 0x11d3,                        \
{ 0x9c, 0xe4, 0xed, 0x60, 0xbd, 0x6c, 0xb5, 0xbc } }


class nsIPrivateTextRangeList;
struct nsTextEventReply;

class nsIEditorIMESupport : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITORIMESUPPORT_IID; return iid; }


  /**
   * BeginComposition() Handles the start of inline input composition.
   */

  NS_IMETHOD BeginComposition(void) = 0;

  /**
   * SetCompositionString() Sets the inline input composition string.
   * BeginComposition must be called prior to this.
   */

  NS_IMETHOD SetCompositionString(const nsString& aCompositionString, nsIPrivateTextRangeList* aTextRange, nsTextEventReply* aReply) = 0;

  /**
   * BeginComposition() Handles the end of inline input composition.
   */

  NS_IMETHOD EndComposition(void) = 0;

};


#endif // nsIEditorIMESupport_h__
