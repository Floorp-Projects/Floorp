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
#include "nsISO88597ToUnicode.h"
#include "nsUCvLatinDll.h"

static PRUint16 gMappingTable[] = {
#include "8859-7.ut"
};

static PRBool gFastTableInit = PR_FALSE;
static PRUnichar gFastTable [256] ;

//----------------------------------------------------------------------
// Class nsISO88597ToUnicode [implementation]

NS_IMPL_ISUPPORTS(nsISO88597ToUnicode, kIUnicodeDecoderIID);

nsISO88597ToUnicode::nsISO88597ToUnicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsISO88597ToUnicode::~nsISO88597ToUnicode() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

nsresult nsISO88597ToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsISO88597ToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

uMappingTable* nsISO88597ToUnicode::GetMappingTable() 
{
  return (uMappingTable*) &gMappingTable;
}

PRUnichar * nsISO88597ToUnicode::GetFastTable() 
{
  return gFastTable;
}

PRBool nsISO88597ToUnicode::GetFastTableInitState() 
{
  return gFastTableInit;
}

void nsISO88597ToUnicode::SetFastTableInit() 
{
  gFastTableInit = PR_TRUE;
}
