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


#include "nsTokenHandler.h"
#include "nsHTMLParser.h"
#include "nsHTMLTokens.h"
#include "nsDebug.h"

static const char* kNullParserGiven = "Error: Null parser given as argument";

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CTokenHandler::CTokenHandler(eHTMLTokenTypes aType) {
  mType=aType;
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CTokenHandler::~CTokenHandler(){
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
eHTMLTokenTypes CTokenHandler::GetTokenType(void){
  return mType;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  PRBool result=PR_FALSE;
  if(aParser){
    result=PR_TRUE;
  }
  return result;
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CStartTokenHandler::CStartTokenHandler() : CTokenHandler(eToken_start) {
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CStartTokenHandler::~CStartTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CStartTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleStartToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CStartTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CEndTokenHandler::CEndTokenHandler(): CTokenHandler(eToken_end) {
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CEndTokenHandler::~CEndTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CEndTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleEndToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CEndTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CCommentTokenHandler::CCommentTokenHandler() : CTokenHandler(eToken_comment) {
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CCommentTokenHandler::~CCommentTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CCommentTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleCommentToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CCommentTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CEntityTokenHandler::CEntityTokenHandler() : CTokenHandler(eToken_entity) {
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CEntityTokenHandler::~CEntityTokenHandler() {
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CEntityTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleEntityToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CEntityTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CWhitespaceTokenHandler::CWhitespaceTokenHandler() : CTokenHandler(eToken_whitespace) {
}

  
/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CWhitespaceTokenHandler::~CWhitespaceTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CWhitespaceTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleSimpleContentToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CWhitespaceTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CNewlineTokenHandler::CNewlineTokenHandler() : CTokenHandler(eToken_newline) {
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CNewlineTokenHandler::~CNewlineTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CNewlineTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleSimpleContentToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CNewlineTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CTextTokenHandler::CTextTokenHandler() : CTokenHandler(eToken_text) {
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CTextTokenHandler::~CTextTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CTextTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleSimpleContentToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CTextTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CAttributeTokenHandler::CAttributeTokenHandler() : CTokenHandler(eToken_attribute) {
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CAttributeTokenHandler::~CAttributeTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CAttributeTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleAttributeToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CAttributeTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CScriptTokenHandler::CScriptTokenHandler() : CTokenHandler(eToken_script) {
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CScriptTokenHandler::~CScriptTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CScriptTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleScriptToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CScriptTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CStyleTokenHandler::CStyleTokenHandler() : CTokenHandler(eToken_style) {
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CStyleTokenHandler::~CStyleTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CStyleTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleStyleToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CStyleTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CSkippedContentTokenHandler::CSkippedContentTokenHandler() : CTokenHandler(eToken_skippedcontent) {
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CSkippedContentTokenHandler::~CSkippedContentTokenHandler(){
}
                          

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CSkippedContentTokenHandler::operator()(CToken* aToken,nsHTMLParser* aParser){
  NS_ASSERTION(0!=aParser,kNullParserGiven);
  if(aParser){
    return aParser->HandleSkippedContentToken(aToken);
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool CSkippedContentTokenHandler::CanHandle(eHTMLTokenTypes aType){
  PRBool result=PR_FALSE;
  return result;
}


