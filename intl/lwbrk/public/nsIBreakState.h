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
#ifndef nsIBreakState_h__
#define nsIBreakState_h__


#include "nsISupports.h"

#include "nscore.h"

// {EE874261-C0BF-11d2-B3AF-00805F8A6670}
#define NS_IBREAKSTATE_IID \
{ 0xee874261, 0xc0bf, 0x11d2, \
    { 0xb3, 0xaf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } };



class nsIBreakState : public nsISupports
{
public:
  NS_IMETHOD Set (PRUint32 aPos, PRBool aDone) = 0; 
  NS_IMETHOD GetText (const PRUnichar** oText) = 0; 
  NS_IMETHOD Length (PRUint32* oLen) = 0; 
  NS_IMETHOD IsDone (PRBool *oDone) = 0; 
  NS_IMETHOD Current (PRUint32* oPos) = 0; 
  NS_IMETHOD SetPrivate (PRUint32  aPriv) = 0; 
  NS_IMETHOD GetPrivate (PRUint32* oPriv) = 0; 

};


#define IMPL_NS_IBREAKSTATE(name)                            \
class name : public nsIBreakState {                          \
  NS_DECL_ISUPPORTS                                          \
  name (const PRUnichar *aText, PRUint32 aLen) {             \
     NS_INIT_REFCNT();                                       \
     mText = aText; mLen = aLen;                             \
     mPos = 0; mPriv = 0; mDone = PR_FALSE;                  \
  } ;                                                        \
  ~name () { }  ;                                            \
                                                             \
public:                                                      \
  NS_IMETHOD Set (PRUint32 aPos, PRBool aDone)               \
              { mPos = aPos; mDone = aDone; return NS_OK; }; \
                                                             \
  NS_IMETHOD GetText (const PRUnichar** oText)               \
              { *oText = mText; return NS_OK; };             \
                                                             \
  NS_IMETHOD Length (PRUint32* oLen)                         \
              { *oLen = mLen; return NS_OK; };               \
                                                             \
  NS_IMETHOD IsDone (PRBool *oDone)                          \
              { *oDone = mDone; return NS_OK; };             \
                                                             \
  NS_IMETHOD Current (PRUint32* oPos)                        \
              { *oPos = mPos; return NS_OK; };               \
                                                             \
  NS_IMETHOD SetPrivate (PRUint32  aPriv)                    \
              { mPriv = aPriv; return NS_OK; };              \
                                                             \
  NS_IMETHOD GetPrivate (PRUint32* oPriv)                    \
              { *oPriv = mPriv ; return NS_OK; };            \
                                                             \
  PRUint32 Current() { return mPos;};                        \
                                                             \
  PRUint32 IsDone()  { return mDone;};                       \
                                                             \
protected:                                                   \
  PRUint32  mPos;                                            \
  PRBool    mDone;                                           \
  PRUint32  mPriv;                                           \
  const PRUnichar*mText;                                     \
  PRUint32  mLen;                                            \
};                                                           \
NS_DEFINE_IID(kIBreakStateIID, NS_IBREAKSTATE_IID);          \
NS_IMPL_ISUPPORTS( name , kIBreakStateIID);

#endif  /* nsIBreakState_h__ */
