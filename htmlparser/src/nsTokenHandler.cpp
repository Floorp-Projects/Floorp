/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */


#include "nsTokenHandler.h"
#include "nsDebug.h"
#include "nsIDTD.h"
#include "nsToken.h"
#include "nsIParser.h"

MOZ_DECL_CTOR_COUNTER(CTokenHandler)

/**
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
CTokenHandler::CTokenHandler(dispatchFP aFP,PRInt32 aType){

  MOZ_COUNT_CTOR(CTokenHandler);

  mType=aType;
  mFP=aFP;
}


/**
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
CTokenHandler::~CTokenHandler(){
  MOZ_COUNT_DTOR(CTokenHandler);
}


/**
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
PRInt32 CTokenHandler::GetTokenType(void){
  return mType;
}


/**
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
nsresult CTokenHandler::operator()(CToken* aToken,nsIDTD* aDTD){
  nsresult result=NS_OK;
  if((0!=aDTD) && (0!=mFP)) {
     result=(*mFP)(aToken,aDTD);
  }
  return result;
}


