/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2
-*- 
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

/**
 * This class manages a string in the following forms:
 *
 *    /S=Steve/G=Mansour/Nd=10000/
 *
 * It creates a hash table where the property names are the keys.
 * This code probably exists somewhere else.
 *
 * sman
 *
 */

#ifdef XP_PC
#include "windows.h"
#endif

#include "nscalcore.h"
#include "nsString.h"
#include "xp_mcom.h"
#include "jdefines.h"
#include "julnstr.h"
#include "nspr.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsX400Parser.h"


nsX400Parser::nsX400Parser()
{
  Init();
}

nsX400Parser::nsX400Parser(const JulianString& sValue)
{
  Init();
  SetValue(sValue.GetBuffer());
}

nsX400Parser::nsX400Parser(const char* psValue)
{
  Init();
  SetValue(psValue);
}

nsX400Parser::~nsX400Parser()
{
  if (mppVals)
    delete [] mppVals;
  if (mppKeys)
    delete [] mppKeys;
}

nsresult nsX400Parser::Init()
{
  miSize = 0;
  miLength = 0;
  mppVals = 0;
  mppKeys = 0;

  EnsureSize(40);

  if (0 == mppVals || 0 == mppKeys)
    return 1;   /* XXX: memory allocation error */

  return NS_OK;
}

nsresult nsX400Parser::EnsureSize(PRInt32 aSize)
{
  if (aSize > miSize)
  {
    /*
     * Allocate new buffers with a bit of extra space...
     */
    PRInt32 iOldSize = miSize;
    miSize = aSize + 10;
    JulianString **ppVals = mppVals;
    JulianString **ppKeys = mppKeys;

    void** aVoid = new void*[miSize];
    mppVals = (JulianString**)aVoid;
    void** bVoid = new void*[miSize];
    mppKeys = (JulianString**)bVoid;

    if (0 == mppVals || 0 == mppKeys)
    {
      if (mppVals)
        delete mppVals;
      mppVals = ppVals;
      if (mppKeys)
        delete mppKeys;
      mppKeys = ppKeys;
      return 1;   /* XXX: memory allocation error */
    }

    /*
     * Initialize the new buffers to known values...
     */
    nsCRT::memset(mppVals,0,miSize);
    nsCRT::memset(mppKeys,0,miSize);

    /*
     * Copy the old stuff into the new buffers...
     */
    for (int i = 0; i < iOldSize; i++)
    {
      mppVals[i] = ppVals[i];
      mppKeys[i] = ppKeys[i];
    }

    /*
     * Free the old buffers...
     */
    delete [] ppVals;
    delete [] ppKeys;
  }

  return NS_OK;
}

/**
 * Find the requested key...
 */
nsresult nsX400Parser::FindKey(const char* aKey, PRInt32* aIndex)
{
  for (PRInt32 i = 0; i < miLength; i++)
  {
    if ((*mppKeys[i]) == aKey)
    {
      *aIndex = i;
      return NS_OK;
    }
  }
  *aIndex = -1;
  return NS_OK;
}

nsresult nsX400Parser::SetValue(const char* psVal)
{
  msValue = psVal;
  Parse();
  return NS_OK;
}

nsresult nsX400Parser::Set(const char* aKey, const char* aVal)
{
  PRInt32 i;
  FindKey(aKey,&i);
  if (i < 0)
    i = miLength + 1;
  if (NS_OK != EnsureSize(i))
    return 1;   /* XXX: out of memory */

  JulianString* psKey = mppKeys[i];
  JulianString* psVal = mppVals[i];

  if (0 == psKey)
  {
    psKey = new JulianString(aKey);
    mppKeys[i] = psKey;
  }
  else
    *psKey = aKey;

  if (0 == psVal)
  {
    psVal = new JulianString(aVal);
    mppVals[i] = psVal;
  }
  else
    *psVal = aVal;

  if (0 == psKey || 0 == psVal)
    return 1;   /* XXX: out of memory */
  
  return NS_OK;
}

nsresult nsX400Parser::Get(const char* aKey, char **ppsVal )
{
  PRInt32 i;
  FindKey(aKey,&i);
  if (i >= 0)
  {
    *ppsVal = (*mppVals[i]).GetBuffer();
    return NS_OK;
  }

  *ppsVal = 0;
  return NS_OK;

}

nsresult nsX400Parser::DestroyEntry(PRInt32 i)
{
  if (0 != mppKeys[i])
  {
    delete mppKeys[i];
    mppKeys[i] = 0;
  }

  if (0 != mppVals[i])
  {
    delete mppVals[i];
    mppVals[i] = 0;
  }
  return NS_OK;
}

/**
 * Delete an entry.
 */
nsresult nsX400Parser::Delete(const char* aKey)
{
  PRInt32 i;
  FindKey(aKey,&i);
  if (i >= 0)
  {
    DestroyEntry(i);
    /*
     * move entries from i+1 to miLength - 1 up one slot...
     */
    for (i++; i < miLength; i++)
    {
      mppKeys[i-1] = mppKeys[i];
      mppVals[i-1] = mppVals[i];
    }
    --miLength;
    mppKeys[miLength] = 0;
    mppVals[miLength] = 0;
  }
  return NS_OK;
}

/**
 * Build the list of properties and values...
 *
 *    /S=Steve/G=Mansour/Nd=10000/
 *     |    |
 *     |    +-- Value
 *     +------- Property
 */
nsresult nsX400Parser::Parse()
{
  size_t iLen = msValue.GetStrlen();
  size_t iHead, iTail, i;
  char* p = msValue.GetBuffer();
  JulianString sTemp;
  JulianString* psKey;
  JulianString* psVal;
  
  /*
   * skip leading slash...
   */
  if (0 == p)
    return NS_OK;     /* starting with an empty string */

  for (iHead = 0; *p && '/' == p[iHead]; iHead++)
    /* keep looking */ ;

  if (0 == p[iHead])
    return NS_OK;     /* starting with an empty string */

  /*
   * delete anything in the old arrays...
   */
  if (0 != mppKeys)
  {
    for (i = 0; i < (size_t)miLength; i++)
      DestroyEntry((PRInt32)i);
  }

  /*
   *  Parse off parts until done...
   */
  for (iTail = 0, ++iHead; iTail < iLen - 1; iHead = iTail+1)
  {
    /*
     * grab the next "key=property" substring
     */
    if ( -1 == (iTail = msValue.Find('/',iHead)))
      iTail = iLen - 1;
    if (iHead == iTail)
      break;
    sTemp = msValue.indexSubstr(iHead,iTail);

    /*
     * key and value are separated by '='
     */
    if (-1 == (i = msValue.Find('=',iHead)))
      break;
    if (iHead+1 >= i)
      break;

    /*
     * add this pair to the list...
     */
    psKey = new JulianString( msValue.indexSubstr(iHead,i-1) );
    psVal = new JulianString( msValue.indexSubstr(i+1,iTail) );
    EnsureSize(miLength + 1);
    mppKeys[miLength] = psKey;
    mppVals[miLength] = psVal;
  }

  return NS_OK;
}

nsresult nsX400Parser::Assemble()
{
  msValue = "/";
  for (int i = 0; i < miLength; i++)
  {
    msValue += *mppKeys[i];
    msValue += "=";
    msValue += *mppVals[i];
  }
  msValue += "/";
  return NS_OK;
}
