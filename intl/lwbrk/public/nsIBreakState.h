/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBREAKSTATE_IID)

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
  virtual ~name () { }  ;                                    \
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
NS_IMPL_ISUPPORTS1( name , nsIBreakState)

#endif  /* nsIBreakState_h__ */
