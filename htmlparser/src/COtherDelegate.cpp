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

#include <ctype.h>
#include "COtherDelegate.h"
#include "nsScanner.h"
#include "nsParserTypes.h"
#include "COtherDTD.h"



// Note: We already handle the following special case conditions:
//       1) If you see </>, simply treat it as a bad tag.
//       2) If you see </ ...>, treat it like a comment.
//       3) If you see <> or <_ (< space) simply treat it as text.
//       4) If you see <~ (< followed by non-alpha), treat it as text.

static char gIdentChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";


/**
 *  Default constructor
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
COtherDelegate::COtherDelegate() :
  ITokenizerDelegate(), mTokenDeque() {
}

/**
 *  Default constructor
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
COtherDelegate::COtherDelegate(COtherDelegate& aDelegate) : 
  ITokenizerDelegate(), mTokenDeque() {
}


/**
 * 
 * @update  gess4/11/98
 * @param 
 * @return
 */
eParseMode COtherDelegate::GetParseMode() const {
  return eParseMode_unknown;
}

/**
 * Cause delegate to create and return a new DTD.
 *
 * @update  gess4/22/98
 * @return  new DTD or null
 */
nsIDTD* COtherDelegate::GetDTD(void) const{
  return new COtherDTD();
}

/**
 *  This method is called just after a "<" has been consumed 
 *  and we know we're at the start of some kind of tagged 
 *  element. We don't know yet if it's a tag or a comment.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CToken* COtherDelegate::ConsumeTag(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode) {
  CToken* result=0;
  nsAutoString empty("");
  anErrorCode=anErrorCode=aScanner.GetChar(aChar);

  switch(aChar) {
    case kForwardSlash:
      PRUnichar ch; 
      anErrorCode=aScanner.Peek(ch);
      if(nsString::IsAlpha(ch))
        result=new CEndToken(empty);
      else result=new CCommentToken(empty); //Special case: </ ...> is treated as a comment
      break;
    case kExclamation:
      result=new CCommentToken(empty);
      break;
    default:
      if(nsString::IsAlpha(aChar))
        return ConsumeStartTag(aChar,aScanner,anErrorCode);
      else if(kNotFound!=aChar) {
        nsAutoString temp("<");
        return ConsumeText(temp,aScanner,anErrorCode);
      }
  } //switch

  if(result!=0) {
    anErrorCode= result->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
    if(anErrorCode) {
      result=0;
      delete result;
    }
  }
  return result;
}

/**
 *  This method is called just after we've consumed a start
 *  tag, and we now have to consume its attributes.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar: last char read
 *  @param   aScanner: see nsScanner.h
 *  @return  
 */
void COtherDelegate::ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode) {
  PRBool done=PR_FALSE;
  nsAutoString as("");
  anErrorCode=kNoError;
  while((!done) && (anErrorCode==kNoError)) {
     CToken* result = new CAttributeToken(as);
      if(result){
        anErrorCode= result->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
         mTokenDeque.Push(result);
      }
    aScanner.Peek(aChar);
      if(aChar==kGreaterThan) { //you just ate the '>'
        aScanner.GetChar(aChar); //skip the '>'
        done=PR_TRUE;
      }
  }
  return;
}

/**
 *  This is a special case method. It's job is to consume 
 *  all of the given tag up to an including the end tag.
 *
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null
 */
CToken* COtherDelegate::ConsumeContentToEndTag(const nsString& aString,PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode){
  
  //In the case that we just read the given tag, we should go and
  //consume all the input until we find a matching end tag.

  nsAutoString endTag("</");
  endTag.Append(aString);
  endTag.Append(">");
  CSkippedContentToken* sc=new CSkippedContentToken(endTag);
  anErrorCode= sc->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
  return sc;
}

/**
 *  This method is called just after a "<" has been consumed 
 *  and we know we're at the start of a tag.  
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
CToken* COtherDelegate::ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode) {
  CStartToken* result=new CStartToken(nsAutoString(""));
  if(result) {
    anErrorCode= result->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
    if(result->IsAttributed()) {
      ConsumeAttributes(aChar,aScanner,anErrorCode);
    }
    //now that that's over with, we have one more problem to solve.
    //In the case that we just read a <SCRIPT> or <STYLE> tags, we should go and
    //consume all the content itself.
    nsString& str=result->GetText();
    if(str.EqualsIgnoreCase("SCRIPT") ||
       str.EqualsIgnoreCase("STYLE") ||
       str.EqualsIgnoreCase("TITLE") ||
       str.EqualsIgnoreCase("TEXTAREA")) {
      CToken* sc=ConsumeContentToEndTag(str,aChar,aScanner,anErrorCode);
      
      if(sc){
          //now we strip the ending sequence from our new SkippedContent token...
        PRInt32 slen=str.Length()+3;
        nsString& skippedText=sc->GetText();
      
        skippedText.Cut(skippedText.Length()-slen,slen);
        mTokenDeque.Push(sc);
    
        //In the case that we just read a given tag, we should go and
        //consume all the tag content itself (and throw it all away).

        CEndToken* endtoken=new CEndToken(str);
        mTokenDeque.Push(endtoken);
      } //if
    } //if
  }
  return result;
}

/**
 *  This method is called just after a "&" has been consumed 
 *  and we know we're at the start of an entity.  
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
CToken* COtherDelegate::ConsumeEntity(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode) {
   CToken*    result = 0;
   PRUnichar  ch;
   anErrorCode=aScanner.GetChar(ch);
   if(nsString::IsAlpha(ch)) { //handle common enity references &xxx; or &#000.
     result = new CEntityToken(nsAutoString(""));
     anErrorCode= result->Consume(ch,aScanner);  //tell new token to finish consuming text...    
   }
   else if(kHashsign==ch) {
     result = new CEntityToken(nsAutoString(""));
     anErrorCode=result->Consume(ch,aScanner);
   }
   else {
     //oops, we're actually looking at plain text...
     nsAutoString temp("&");
     return ConsumeText(temp,aScanner,anErrorCode);
   }
   return result;
}

/**
 *  This method is called just after whitespace has been 
 *  consumed and we know we're at the start a whitespace run.  
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
CToken* COtherDelegate::ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode) {
  CToken* result = new CWhitespaceToken(nsAutoString(""));
  anErrorCode=result->Consume(aChar,aScanner);
  return result;
}

/**
 *  This method is called just after a "<!" has been consumed 
 *  and we know we're at the start of a comment.  
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
CToken* COtherDelegate::ConsumeComment(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode){
  CToken* result= new CCommentToken(nsAutoString(""));
  anErrorCode=result->Consume(aChar,aScanner);
  return result;
}

/**
 *  This method is called just after a known text char has
 *  been consumed and we should read a text run.
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
CToken* COtherDelegate::ConsumeText(const nsString& aString,CScanner& aScanner,PRInt32& anErrorCode){
  CToken* result=new CTextToken(aString);
  if(result) {
    PRUnichar ch;
     anErrorCode=result->Consume(ch,aScanner);
  }
  return result;
}

/**
 *  This method is called just after a newline has been consumed. 
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
CToken* COtherDelegate::ConsumeNewline(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode){
  CToken* result=new CNewlineToken(nsAutoString(""));
  if(result) {
     anErrorCode=result->Consume(aChar,aScanner);
  }
  return result;
}

/**
 *  This method repeatedly called by the tokenizer. 
 *  Each time, we determine the kind of token were about to 
 *  read, and then we call the appropriate method to handle
 *  that token type.
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
CToken* COtherDelegate::GetToken(CScanner& aScanner,PRInt32& anErrorCode){
  CToken*   result=0;
  PRUnichar aChar;

  if(mTokenDeque.GetSize()>0) {
    return (CToken*)mTokenDeque.Pop();
  }

   while(!aScanner.Eof()) {
     anErrorCode=aScanner.GetChar(aChar);
    switch(aChar) {
      case kAmpersand:
        return ConsumeEntity(aChar,aScanner,anErrorCode);
      case kLessThan:
        return ConsumeTag(aChar,aScanner,anErrorCode);
      case kCR: case kLF:
        return ConsumeNewline(aChar,aScanner,anErrorCode);
      case kNotFound:
        break;
      default:
        if(nsString::IsSpace(aChar))
            return ConsumeWhitespace(aChar,aScanner,anErrorCode);
        else
        {
          nsAutoString temp(aChar);
          return ConsumeText(temp,aScanner,anErrorCode);
        }
    } //switch
    if(anErrorCode==kEOF)
      anErrorCode=0;
   } //while
  return result;
}

/**
 * 
 * @update  gess4/11/98
 * @param 
 * @return
 */
CToken* COtherDelegate::CreateTokenOfType(eHTMLTokenTypes aType) {
  return 0;
}

/**
 *  This method is by the tokenizer, once for each new token
 *  we've constructed. This method determines whether or not
 *  the new token (argument) should be accepted as a valid 
 *  token. If so, the token is added to the deque of tokens
 *  contained within the tokenzier. If no, the token is 
 *  ignored.
 *  
 *  @update gess 3/25/98
 *  @param  aToken: token to be tested for acceptance
 *  @return TRUE if token should be accepted. 
 */
PRBool COtherDelegate::WillAddToken(CToken& /*aToken*/) {
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This method is called by the parser, just before a stream
 *  is parsed. This method is called so that the delegate 
 *  can do any "pre-parsing" initialization.
 *  
 *  @update gess 3/25/98
 *  @return TRUE if preinitialization completed successfully
 */
PRBool COtherDelegate::WillTokenize() {
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This method is called by the parser, just after a stream
 *  was parsed. This method is called so that the delegate 
 *  can do any "post-parsing" cleanup.
 *  
 *  @update gess 3/25/98
 *  @return TRUE if preinitialization completed successfully
 */
PRBool COtherDelegate::DidTokenize() {
  PRBool result=PR_TRUE;
   return result;
}


/**
 *  This is the selftest method for the delegate class.
 *  Unfortunately, there's not much you can do with this
 *  class alone, so we do the selftesting as part of the
 *  parser class.
 *  
 *  @update gess 3/25/98
 */
void COtherDelegate::SelfTest(void) {
#ifdef _DEBUG

#endif
}




