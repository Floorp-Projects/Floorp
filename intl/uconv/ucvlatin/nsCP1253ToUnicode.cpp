/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "pratom.h"
#include "nsCP1253ToUnicode.h"
#include "nsUCvLatinDll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRUint16 gMappingTable[] = {
#include "cp1253.ut"
};

static PRBool gFastTableInit = PR_FALSE;
static PRUnichar gFastTable[256];

//----------------------------------------------------------------------
// Class nsCP1253ToUnicode [implementation]

NS_IMPL_ISUPPORTS(nsCP1253ToUnicode, kIUnicodeDecoderIID);

nsCP1253ToUnicode::nsCP1253ToUnicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsCP1253ToUnicode::~nsCP1253ToUnicode() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

nsresult nsCP1253ToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsCP1253ToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

uMappingTable* nsCP1253ToUnicode::GetMappingTable() 
{
  return (uMappingTable*) &gMappingTable;
}

PRUnichar * nsCP1253ToUnicode::GetFastTable() 
{
  return gFastTable;
}

PRBool nsCP1253ToUnicode::GetFastTableInitState() 
{
  return gFastTableInit;
}

void nsCP1253ToUnicode::SetFastTableInit() 
{
  gFastTableInit = PR_TRUE;
}
