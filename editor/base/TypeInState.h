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

#ifndef ChangeAttributeTxn_h__
#define ChangeAttributeTxn_h__

#include "nsIDOMSelectionListener.h"
#include "nsColor.h"
#include "nsCoord.h"

class TypeInState : public nsIDOMSelectionListener
{
public:

  NS_DECL_ISUPPORTS

  TypeInState();
  void Reset();
  virtual ~TypeInState();

  NS_IMETHOD NotifySelectionChanged();

  PRBool IsSet(PRUint32 aStyle);
  PRBool IsAnySet();
  void   UnSet(PRUint32 aStyle);

  void SetBold(PRBool aIsSet);
  PRBool GetBold(); 

  void SetItalic(PRBool aIsSet);
  PRBool GetItalic();

  void SetUnderline(PRBool aIsSet);
  PRBool GetUnderline();
  
  void SetFontFace(nsString aFace);
  nsString GetFontFace();
  
  void SetFontColor(nscolor aColor);
  nscolor GetFontColor();
  
  void SetFontSize(nscoord aFontSize);
  nscoord GetFontSize();

protected:
  PRBool   mBold;
  PRBool   mItalic;
  PRBool   mUnderline;
  nsString mFontFace;
  nscolor  mFontColor;
  nscoord  mFontSize;
  PRUint32 mIsSet;
};

#define NS_TYPEINSTATE_BOLD       0x00000001
#define NS_TYPEINSTATE_ITALIC     0x00000002
#define NS_TYPEINSTATE_UNDERLINE  0x00000004
#define NS_TYPEINSTATE_FONTFACE   0x00000008
#define NS_TYPEINSTATE_FONTCOLOR  0x00000010
#define NS_TYPEINSTATE_FONTSIZE   0x00000020

/* ----- inline method definitions ----- */
inline
TypeInState::TypeInState()
{ 
  NS_INIT_REFCNT();
  Reset(); 
};

inline
void TypeInState::Reset()
{
  mBold = PR_FALSE;
  mItalic = PR_FALSE;
  mUnderline = PR_FALSE;
  mFontColor = NS_RGB(0,0,0);
  mFontSize = 0;
  mIsSet = 0;
};

inline 
PRBool TypeInState::IsSet(PRUint32 aStyle)
{
  return (PRBool)(mIsSet & aStyle);
};

inline 
void TypeInState::UnSet(PRUint32 aStyle)
{
  mIsSet &= ~aStyle;
};

inline
PRBool TypeInState::IsAnySet()
{
  return (PRBool)(0!=mIsSet);
}

inline
void TypeInState::SetBold(PRBool aIsSet) 
{ 
  mBold = aIsSet; 
  mIsSet |= NS_TYPEINSTATE_BOLD;
};

inline
PRBool TypeInState::GetBold() 
{ return mBold;};

inline
void TypeInState::SetItalic(PRBool aIsSet) 
{ 
  mItalic = aIsSet; 
  mIsSet |= NS_TYPEINSTATE_ITALIC;
};

inline
PRBool TypeInState::GetItalic() 
{ return mItalic; };

inline
void TypeInState::SetUnderline(PRBool aIsSet) 
{ 
  mUnderline = aIsSet; 
  mIsSet |= NS_TYPEINSTATE_UNDERLINE;
};

inline
PRBool TypeInState::GetUnderline() 
{ return mUnderline; };

inline
void TypeInState::SetFontFace(nsString aFace)
{
  mFontFace = aFace;
  mIsSet |= NS_TYPEINSTATE_FONTFACE;
};

inline
nsString TypeInState::GetFontFace()
{ return mFontFace; };

inline
void TypeInState::SetFontColor(nscolor aColor)
{
  mFontColor = aColor;
  mIsSet |= NS_TYPEINSTATE_FONTCOLOR;
};

inline
nscolor TypeInState::GetFontColor()
{ return mFontColor; };

inline
void TypeInState::SetFontSize(nscoord aSize)
{
  mFontSize = aSize;
  mIsSet |= NS_TYPEINSTATE_FONTSIZE;
};

inline
nscoord TypeInState::GetFontSize()
{ return mFontSize; };


#endif
