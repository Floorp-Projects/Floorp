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
#include "CNavDelegate.h"
#include "nsScanner.h"
#include "nsParserTypes.h"
#include "CNavDTD.h"


// Note: We already handle the following special case conditions:
//       1) If you see </>, simply treat it as a bad tag.
//       2) If you see </ ...>, treat it like a comment.
//       3) If you see <> or <_ (< space) simply treat it as text.
//       4) If you see <[!a..z] (< followed by non-alpha), treat it as text.

static char gIdentChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

static void TokenFreeProc(void * pToken)
{
   if (pToken!=NULL) {
      CToken * pCToken = (CToken*)pToken;
      delete pCToken;
   }
}

/**
 *  Default constructor
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
CNavDelegate::CNavDelegate() :
  ITokenizerDelegate(), mTokenDeque(PR_TRUE,TokenFreeProc) {
}

/**
 *  Default constructor
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
CNavDelegate::CNavDelegate(CNavDelegate& aDelegate) : 
  ITokenizerDelegate(), mTokenDeque() {
}


/**
 * 
 * @update  gess4/11/98
 * @param 
 * @return
 */
eParseMode CNavDelegate::GetParseMode(void) const {
  return eParseMode_unknown;
}


/**
 * Cause delegate to create and return a new DTD.
 *
 * @update  gess4/22/98
 * @return  new DTD or null
 */
nsIDTD* CNavDelegate::GetDTD(void) const{
  return new CNavDTD();
}

/**
 *  This method is called just after a "<" has been consumed 
 *  and we know we're at the start of some kind of tagged 
 *  element. We don't know yet if it's a tag or a comment.
 *  
 *  @update  gess 5/12/98
 *  @param   aChar is the last char read
 *  @param   aScanner is represents our input source
 *  @param   aToken is the out arg holding our new token
 *  @return  error code (may return kInterrupted).
 */
PRInt32 CNavDelegate::ConsumeTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {

  nsAutoString empty("");
  PRInt32 result=aScanner.GetChar(aChar);

  if(kNoError==result) {

    switch(aChar) {
      case kForwardSlash:
        PRUnichar ch; 
        result=aScanner.Peek(ch);
        if(kNoError==result) {
          if(nsString::IsAlpha(ch))
            aToken=new CEndToken(empty);
          else aToken=new CCommentToken(empty); //Special case: </ ...> is treated as a comment
        }//if
        break;
      case kExclamation:
        aToken=new CCommentToken(empty);
        break;
      default:
        if(nsString::IsAlpha(aChar))
          return ConsumeStartTag(aChar,aScanner,aToken);
        else if(kEOF!=aChar) {
          nsAutoString temp("<");
          return ConsumeText(temp,aScanner,aToken);
        }
    } //switch

    if((0!=aToken) && (kNoError==result)) {
      result= aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
      if(result) {
        delete aToken;
        aToken=0;
      }
    } //if
  } //if
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
PRInt32 CNavDelegate::ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  PRBool done=PR_FALSE;
  PRInt32 result=kNoError;
  nsAutoString as("");
  PRInt16 theAttrCount=0;

  while((!done) && (result==kNoError)) {
    CToken* theToken= new CAttributeToken(as);
    if(theToken){
      result=theToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
      if(kNoError==result){
        theAttrCount++;
        mTokenDeque.Push(theToken);
      }//if
      else delete theToken; //we can't keep it...
    }//if
    
    if(kNoError==result){
      result=aScanner.Peek(aChar);
      if(aChar==kGreaterThan) { //you just ate the '>'
        aScanner.GetChar(aChar); //skip the '>'
        done=PR_TRUE;
      }//if
    }//if
  }//while

  aToken->SetAttributeCount(theAttrCount);
  return result;
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
PRInt32 CNavDelegate::ConsumeContentToEndTag(const nsString& aString,PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  
  //In the case that we just read the given tag, we should go and
  //consume all the input until we find a matching end tag.

  nsAutoString endTag("</");
  endTag.Append(aString);
  endTag.Append(">");
  aToken=new CSkippedContentToken(endTag);
  return aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
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
PRInt32 CNavDelegate::ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  PRInt32 theDequeSize=mTokenDeque.GetSize();
  PRInt32 result=kNoError;

  aToken=new CStartToken(nsAutoString(""));

  if(aToken) {
    result= aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
    if(kNoError==result) {
      if(((CStartToken*)aToken)->IsAttributed()) {
        result=ConsumeAttributes(aChar,aScanner,aToken);
      }
      //now that that's over with, we have one more problem to solve.
      //In the case that we just read a <SCRIPT> or <STYLE> tags, we should go and
      //consume all the content itself.
      if(kNoError==result) {
        nsString& str=aToken->GetText();
        CToken*   skippedToken=0;
        if(str.EqualsIgnoreCase("SCRIPT") ||
           str.EqualsIgnoreCase("STYLE") ||
           str.EqualsIgnoreCase("TITLE") ||
           str.EqualsIgnoreCase("TEXTAREA")) {
          result=ConsumeContentToEndTag(str,aChar,aScanner,skippedToken);
    
          if((kNoError==result) && skippedToken){
              //now we strip the ending sequence from our new SkippedContent token...
            PRInt32 slen=str.Length()+3;
            nsString& skippedText=skippedToken->GetText();
    
            skippedText.Cut(skippedText.Length()-slen,slen);
            mTokenDeque.Push(skippedToken);
  
            //In the case that we just read a given tag, we should go and
            //consume all the tag content itself (and throw it all away).

            CEndToken* endtoken=new CEndToken(str);
            mTokenDeque.Push(endtoken);
          } //if
        } //if
      } //if

      //EEEEECCCCKKKK!!! 
      //This code is confusing, so pay attention.
      //If you're here, it's because we were in the midst of consuming a start
      //tag but ran out of data (not in the stream, but in this *part* of the stream.
      //For simplicity, we have to unwind our input. Therefore, we pop and discard
      //any new tokens we've cued this round. Later we can get smarter about this.
      if(kNoError!=result) {
        while(mTokenDeque.GetSize()>theDequeSize) {
          delete mTokenDeque.PopBack();
        }
      }


    } //if
  } //if
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
PRInt32 CNavDelegate::ConsumeEntity(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
   PRUnichar  ch;
   PRInt32 result=aScanner.GetChar(ch);

   if(kNoError==result) {
     if(nsString::IsAlpha(ch)) { //handle common enity references &xxx; or &#000.
       aToken = new CEntityToken(nsAutoString(""));
       result = aToken->Consume(ch,aScanner);  //tell new token to finish consuming text...    
     }
     else if(kHashsign==ch) {
       aToken = new CEntityToken(nsAutoString(""));
       result=aToken->Consume(0,aScanner);
     }
     else {
       //oops, we're actually looking at plain text...
       nsAutoString temp("&");
       temp.Append(ch);
       result=ConsumeText(temp,aScanner,aToken);
     }
   }//if
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
PRInt32 CNavDelegate::ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  aToken = new CWhitespaceToken(nsAutoString(""));
  PRInt32 result=kNoError;
  if(aToken) {
     result=aToken->Consume(aChar,aScanner);
  }
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
PRInt32 CNavDelegate::ConsumeComment(PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  aToken = new CCommentToken(nsAutoString(""));
  PRInt32 result=kNoError;
  if(aToken) {
     result=aToken->Consume(aChar,aScanner);
  }
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
PRInt32 CNavDelegate::ConsumeText(const nsString& aString,CScanner& aScanner,CToken*& aToken){

  PRInt32 result=kNoError;
  if(aToken=new CTextToken(aString)) {
    PRUnichar ch=0;
    result=aToken->Consume(ch,aScanner);
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
PRInt32 CNavDelegate::ConsumeNewline(PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  aToken=new CNewlineToken(nsAutoString(""));
  PRInt32 result=kNoError;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner);
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
PRInt32 CNavDelegate::GetToken(CScanner& aScanner,CToken*& aToken){
  
  aToken=0;
  if(mTokenDeque.GetSize()>0) {
    aToken=(CToken*)mTokenDeque.Pop();
    return kNoError;
  }

  PRInt32 result=kNoError;
  if(kNoError==result){
    
    PRUnichar aChar;
    result=aScanner.GetChar(aChar);
    switch(result) {
      case kEOF:
        break;

      case kInterrupted:
        aScanner.RewindToMark();
        break; 

      case kNoError:
      default:
        switch(aChar) {
          case kLessThan:
            return ConsumeTag(aChar,aScanner,aToken);

          case kAmpersand:
            return ConsumeEntity(aChar,aScanner,aToken);
          
          case kCR: case kLF:
            return ConsumeNewline(aChar,aScanner,aToken);
          
          case kNotFound:
            break;
          
          default:
            if(!nsString::IsSpace(aChar)) {
              nsAutoString temp(aChar);
              return ConsumeText(temp,aScanner,aToken);
            }
            else return ConsumeWhitespace(aChar,aScanner,aToken);
            break;
        } //switch
        break; 
    } //switch
    if(kNoError==result)
      result=aScanner.Eof();
  } //while
  return result;
}

/**
 * 
 * @update  gess4/11/98
 * @param 
 * @return
 */
CToken* CNavDelegate::CreateTokenOfType(eHTMLTokenTypes aType) {
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
PRBool CNavDelegate::WillAddToken(CToken& /*aToken*/) {
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
PRBool CNavDelegate::WillTokenize(PRBool aIncremental) {
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
PRBool CNavDelegate::DidTokenize(PRBool aIncremental) {
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
void CNavDelegate::SelfTest(void) {
#ifdef _DEBUG

#endif
}




