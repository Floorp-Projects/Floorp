/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */

#ifndef DTDUTILS_
#define DTDUTILS_

#include "nsHTMLTags.h"
#include "nsHTMLTokens.h"
#include "nsCRT.h"

/***************************************************************
  Before digging into the NavDTD, we'll define a helper 
  class called CTagStack.

  Simply put, we've built ourselves a little data structure that
  serves as a stack for htmltags (and associated bits). 
  What's special is that if you #define _dynstack 1, the stack
  size can grow dynamically (like you'ld want in a release build.)
  If you don't #define _dynstack 1, then the stack is a fixed size,
  equal to the eStackSize enum. This makes debugging easier, because
  you can see the htmltags on the stack if its not dynamic.
 ***************************************************************/

//#define _dynstack 1
class nsTagStack {
  enum {eStackSize=200};

public:

            nsTagStack(int aDefaultSize=50);
            ~nsTagStack();
  void      Push(eHTMLTags aTag);
  eHTMLTags Pop();
  eHTMLTags First() const;
  eHTMLTags Last() const;
  void      Empty(void); 

  nsTagStack* mPrevious;
  int         mSize;
  int         mCount;

#ifdef _dynstack
  eHTMLTags*  mTags;
  PRBool*     mBits;
#else
  eHTMLTags   mTags[eStackSize];
  PRBool      mBits[eStackSize];
#endif
};


/***************************************************************
  The dtdcontext class defines the current tag context (both
  structural and stylistic). This small utility class allows us
  to push/pop contexts at will, which makes handling styles in
  certainly (unfriendly) elements (like tables) much easier.
 ***************************************************************/

class nsDTDContext {
public:
                nsDTDContext(int aDefaultSize=50);
                ~nsDTDContext();
  void          pushStyleStack(nsTagStack* aStack=0);
  nsTagStack*   popStyleStack();

  nsTagStack    mElements;  //no need to hide these members. 
  nsTagStack*   mStyles;    //the dtd should have direct access to them.
};



#endif


