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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsObjectArray.h"

//----------------------------------------------------------------------------
// Class nsObject [implementation]

// XXX clarify why I can't have this method as a pure virtual one?!
nsObject::~nsObject()
{
}

//----------------------------------------------------------------------------
// Class nsObjectsArray [implementation]

nsObjectArray::nsObjectArray(PRInt32 aCapacity)
{
  Init(aCapacity);
}

nsObjectArray::nsObjectArray()
{
  Init(64);
}

nsObjectArray::~nsObjectArray()
{
  for (PRInt32 i = 0; i < mUsage; i++) delete mArray[i];
  if (mArray != NULL) delete [] mArray;
}

nsObject ** nsObjectArray::GetArray()
{
  return mArray;
}

PRInt32 nsObjectArray::GetUsage()
{
  return mUsage;
}

void nsObjectArray::Init(PRInt32 aCapacity)
{
  mCapacity = aCapacity;
  mUsage = 0;
  mArray = NULL;
}

nsresult nsObjectArray::InsureCapacity(PRInt32 aCapacity)
{
  PRInt32 i;

  if (mArray == NULL) {
    mArray = new nsObject * [mCapacity];
    if (mArray == NULL) return NS_ERROR_OUT_OF_MEMORY;
  }

  if (aCapacity > mCapacity) {
    while (aCapacity > mCapacity) mCapacity *= 2;
    nsObject ** newArray = new nsObject * [mCapacity];
    if (newArray == NULL) return NS_ERROR_OUT_OF_MEMORY;
    for (i = 0; i < mUsage; i++) newArray[i] = mArray[i];
    delete [] mArray;
    mArray = newArray;
  }

  return NS_OK;
}

nsresult nsObjectArray::AddObject(nsObject * aObject)
{
  nsresult res;

  res = InsureCapacity(mUsage + 1);
  if (NS_FAILED(res)) return res;

  (mArray)[mUsage++] = aObject;
  return NS_OK;
}

