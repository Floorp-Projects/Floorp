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
#include "nsIEditProperty.h"

class TypeInState : public nsIDOMSelectionListener
{
public:

  NS_DECL_ISUPPORTS

  TypeInState();
  void Reset();
  virtual ~TypeInState();

  NS_IMETHOD NotifySelectionChanged();

  void GetEnumForName(nsIAtom *aPropName, PRUint32 &aEnum);

  void SetProp(PRUint32 aProp, PRBool aSet);

  void SetPropValue(PRUint32 aProp, const nsString &aValue);

  PRBool IsSet(PRUint32 aStyle);
  PRBool IsAnySet();
  void   UnSet(PRUint32 aStyle);

  void SetBold(PRBool aIsSet);
  PRBool GetBold(); 

  void SetItalic(PRBool aIsSet);
  PRBool GetItalic();

  void SetUnderline(PRBool aIsSet);
  PRBool GetUnderline();
  
  void SetFontFace(const nsString &aFace);
  void GetFontFace(nsString &aFace);
  
  void SetFontColor(const nsString &aColor);
  void GetFontColor(nsString &aColor);
  
  void SetFontSize(const nsString &aSize);
  void GetFontSize(nsString &aSize);

protected:
  PRBool   mBold;
  PRBool   mItalic;
  PRBool   mUnderline;
  nsString mFontFace;
  nsString mFontColor;
  nsString mFontSize;
  PRUint32 mIsSet;
};

#define NS_TYPEINSTATE_UNKNOWN    0x00000000
#define NS_TYPEINSTATE_BOLD       0x00000001
#define NS_TYPEINSTATE_ITALIC     0x00000002
#define NS_TYPEINSTATE_UNDERLINE  0x00000004
#define NS_TYPEINSTATE_FONTFACE   0x00000008
#define NS_TYPEINSTATE_FONTCOLOR  0x00000010
#define NS_TYPEINSTATE_FONTSIZE   0x00000020

/* ----- inline method definitions ----- */
inline
void TypeInState::Reset()
{
  mBold = PR_FALSE;
  mItalic = PR_FALSE;
  mUnderline = PR_FALSE;
  mIsSet = 0;
};

inline
TypeInState::TypeInState()
{
  NS_INIT_REFCNT();
  Reset();
};

inline 
void TypeInState::GetEnumForName(nsIAtom *aPropName, PRUint32 &aEnum)
{
  aEnum = NS_TYPEINSTATE_UNKNOWN;
  if (nsIEditProperty::b==aPropName) { aEnum = NS_TYPEINSTATE_BOLD; }
  else if (nsIEditProperty::i==aPropName) { aEnum = NS_TYPEINSTATE_ITALIC; }
  else if (nsIEditProperty::u==aPropName) { aEnum = NS_TYPEINSTATE_UNDERLINE; }
  else if (nsIEditProperty::face==aPropName) { aEnum = NS_TYPEINSTATE_FONTFACE; }
  else if (nsIEditProperty::color==aPropName) { aEnum = NS_TYPEINSTATE_FONTCOLOR; }
  else if (nsIEditProperty::size==aPropName) { aEnum = NS_TYPEINSTATE_FONTSIZE; }
}

inline void TypeInState::SetProp(PRUint32 aProp, PRBool aSet)
{
  switch (aProp)
  {
    case NS_TYPEINSTATE_BOLD:
      SetBold(aSet);
      break;
    case NS_TYPEINSTATE_ITALIC:
      SetItalic(aSet);
      break;
    case NS_TYPEINSTATE_UNDERLINE:
      SetUnderline(aSet);
      break;
  }
}

inline void TypeInState::SetPropValue(PRUint32 aProp, const nsString &aValue)
{
  switch (aProp)
  {
    case NS_TYPEINSTATE_FONTFACE:
      SetFontFace(aValue);
      break;
    case NS_TYPEINSTATE_FONTCOLOR:
      SetFontColor(aValue);
      break;
    case NS_TYPEINSTATE_FONTSIZE:
      SetFontSize(aValue);
      break;
  }
}

 
inline 
PRBool TypeInState::IsSet(PRUint32 aStyle)
{
  if ((PRBool)(mIsSet & aStyle))
    return PR_TRUE;
  else
    return PR_FALSE;
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
void TypeInState::SetFontFace(const nsString &aFace)
{
  mFontFace = aFace;
  mIsSet |= NS_TYPEINSTATE_FONTFACE;
};

inline
void TypeInState::GetFontFace(nsString &aFace)
{ aFace = mFontFace; };

inline
void TypeInState::SetFontColor(const nsString &aColor)
{
  mFontColor = aColor;
  mIsSet |= NS_TYPEINSTATE_FONTCOLOR;
};

inline
void TypeInState::GetFontColor(nsString &aColor)
{ aColor = mFontColor; };

inline
void TypeInState::SetFontSize(const nsString &aSize)
{
  mFontSize = aSize;
  mIsSet |= NS_TYPEINSTATE_FONTSIZE;
};

inline
void TypeInState::GetFontSize(nsString &aSize)
{ aSize = mFontSize; };


#endif
