/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under theg
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include "nsScanner.h"
#include "nsToken.h"
#include "nsCalICalendarTokens.h"
#include "nsParserTypes.h"
#include "prtypes.h"
#include "nsDebug.h"
#include "nsCalICalendarTags.h"
//#include "nsCalICalendarEntities.h" // todo: if necessary
#include "nsCRT.h"

static nsString     gIdentChars("-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz");
static nsString     gNewLine("\n\r");
static nsAutoString gDigits("0123456789");
static nsAutoString gWhitespace("\b\t ");
static nsAutoString gDelimeters("\n\r:;");
static const char*  gUserdefined = "userdefined";
static const char*  gEmpty = "";

const PRInt32 kMAXNAMELEN=10;

CCalICalendarToken::CCalICalendarToken(const nsString& aName, eCalICalendarTags aTag) : CToken(aName) {
  mTypeID=aTag;
}

CCalICalendarToken::CCalICalendarToken(eCalICalendarTags aTag) : CToken(aTag) {
}

void CCalICalendarToken::SetStringValue(const char* name){
  if(name) {
    mTextValue=name;
    mTypeID = NS_CalICalendarTagToEnum(name);
  }
}

CCalICalendarIdentifierToken::CCalICalendarIdentifierToken(eCalICalendarTags aTag)
  : CCalICalendarToken(aTag) {
}

CCalICalendarIdentifierToken::CCalICalendarIdentifierToken(const nsString& aString,
                                                           eCalICalendarTags aTag)
  : CCalICalendarToken(aString, aTag) {
}


nsresult CCalICalendarIdentifierToken::Consume(PRUnichar aChar, CScanner& aScanner)
{
  mTextValue=aChar;

  nsresult result=aScanner.ReadUntil(mTextValue,gIdentChars,PR_FALSE,PR_FALSE);
  if (NS_OK==result) {
    mTextValue.StripChars("\r");
  }
  return result;
}


PRInt32 CCalICalendarIdentifierToken::GetTypeID(){
  if(eCalICalendarTag_unknown==mTypeID) {
    nsAutoString tmp(mTextValue);
    tmp.ToUpperCase();
    char cbuf[20];
    tmp.ToCString(cbuf, sizeof(cbuf));
    mTypeID = NS_CalICalendarTagToEnum(cbuf);
  }
  return mTypeID;
}

const char* CCalICalendarIdentifierToken::GetClassName(void) {
  return "identifier";
}

PRInt32 CCalICalendarIdentifierToken::GetTokenType(void) {
  return eCalICalendarToken_identifier;
}

void CCalICalendarIdentifierToken::DebugDumpSource(ostream& out) {
  char buffer[200];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  out << "<" << buffer;
}

CCalICalendarPropertyValueToken::CCalICalendarPropertyValueToken()
  : CCalICalendarToken(eCalICalendarTag_unknown) {
}

CCalICalendarPropertyValueToken::CCalICalendarPropertyValueToken(eCalICalendarTags aTag)
  : CCalICalendarToken(aTag) {
}

CCalICalendarPropertyValueToken::CCalICalendarPropertyValueToken(const nsString& aString,
                                                           eCalICalendarTags aTag)
  : CCalICalendarToken(aString, aTag) {
}


nsresult CCalICalendarPropertyValueToken::Consume(PRUnichar aChar, CScanner& aScanner)
{
  //mTextValue=aChar;
  PRUnichar nextChar;
  PRBool done = PR_FALSE;
  nsString cString;

  nsresult result=aScanner.ReadUntil(mTextValue,gNewLine,PR_TRUE,PR_FALSE);
  
  /* Consume the next line if starts with a space */
  while (!done)
  {
  
    /* Consume the '\r\n'.  Also handles UNIX where '\r' is line end */
    aScanner.GetChar(nextChar); // eat next '\r';

    // if next char is '\n' eat it also
    result = aScanner.Peek(nextChar);
    if (kNewline == nextChar)
      aScanner.GetChar(nextChar);

    // handle multi-line property values
    result = aScanner.Peek(nextChar);
    if (kSpace==nextChar)
    {
      aScanner.GetChar(nextChar); // eat space
      cString = "";
      result = aScanner.ReadUntil(cString,gNewLine,PR_TRUE,PR_FALSE);
      if (NS_OK == result)
      {
        mTextValue.Append(cString);
      }
      else {
        // XXX: todo: handle error here.
      }
    }
    else
    {
      done = PR_TRUE;
    }
  }
  if (NS_OK==result) {
    mTextValue.StripChars("\r");
  }
  return result;
}


PRInt32 CCalICalendarPropertyValueToken::GetTypeID(){
  if(eCalICalendarTag_unknown==mTypeID) {
    nsAutoString tmp(mTextValue);
    tmp.ToUpperCase();
    char cbuf[20];
    tmp.ToCString(cbuf, sizeof(cbuf));
    mTypeID = NS_CalICalendarTagToEnum(cbuf);
  }
  return mTypeID;
}

const char* CCalICalendarPropertyValueToken::GetClassName(void) {
  return "propertyValue";
}

PRInt32 CCalICalendarPropertyValueToken::GetTokenType(void) {
  return eCalICalendarToken_propertyvalue;
}

void CCalICalendarPropertyValueToken::DebugDumpSource(ostream& out) {
  char buffer[200];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  out << "<" << buffer;
}

CCalICalendarBeginToken::CCalICalendarBeginToken(eCalICalendarTags aTag) :
  CCalICalendarToken(aTag) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
}

CCalICalendarBeginToken::CCalICalendarBeginToken(nsString& aString,
                                                 eCalICalendarTags aTag) :
  CCalICalendarToken(aString,aTag) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
}

void CCalICalendarBeginToken::Reinitialize(PRInt32 aTag, const nsString& aString){
  CToken::Reinitialize(aTag,aString);
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
}


PRInt32 CCalICalendarBeginToken::GetTypeID(){
  if(eCalICalendarTag_unknown==mTypeID) {
    nsAutoString tmp(mTextValue);
    tmp.ToUpperCase();
    char cbuf[20];
    tmp.ToCString(cbuf, sizeof(cbuf));
    mTypeID = NS_CalICalendarTagToEnum(cbuf);
  }
  return mTypeID;
}

const char* CCalICalendarBeginToken::GetClassName(void) {
  return "begin";
}

PRInt32 CCalICalendarBeginToken::GetTokenType(void) {
  return eCalICalendarToken_begin;
}

void CCalICalendarBeginToken::SetAttributed(PRBool aValue) {
  mAttributed=aValue;
}

PRBool CCalICalendarBeginToken::IsAttributed(void) {
  return mAttributed;
}

void CCalICalendarBeginToken::SetEmpty(PRBool aValue) {
  mEmpty=aValue;
}

PRBool CCalICalendarBeginToken::IsEmpty(void) {
  return mEmpty;
}

//todo: this needs to be modified more
nsresult CCalICalendarBeginToken::Consume(PRUnichar aChar, CScanner& aScanner) {

   //if you're here, we've need to consume the BEGIN: char, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '\r\n'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the "\r\n"
  /*
  nsresult result=NS_OK;
  aScanner.GetChar(aChar);
  mTextValue="BEGIN:";
  if(kMinus==aChar) {
    mTextValue+="-";
    */
    
  mTextValue=aChar;
  nsresult result=aScanner.ReadWhile(mTextValue,gIdentChars,PR_TRUE,PR_FALSE);
  char buffer[300];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  mTypeID = NS_CalICalendarTagToEnum(buffer);

   //Good. Now, let's skip whitespace after the identifier,
   //and see if the next char is ">". If so, we have a complete
   //tag without attributes.
  if(NS_OK==result) {  
    result=aScanner.SkipWhitespace();
    if(NS_OK==result) {
      result=aScanner.GetChar(aChar);
      if(NS_OK==result) {
        if(kSemiColon==aChar) { //look for ';' 
          //push that char back, since we apparently have attributes...
          aScanner.PutBack(aChar);
          mAttributed=PR_TRUE;
        } //if
      } //if
    }//if
  }

  // skip "BEGIN" or "END", reset mTextValue to VCALENDAR,VEVENT,etc..
  if (mTypeID == eCalICalendarTag_begin || mTypeID == eCalICalendarTag_end)
  {
    result = aScanner.SkipWhitespace();
    if (NS_OK == result)
    { 
      mTextValue="";
      result = aScanner.ReadWhile(mTextValue,gIdentChars,PR_TRUE,PR_FALSE);
    }
  }
  
  return result;
};

void CCalICalendarBeginToken::DebugDumpSource(ostream& out) {
  char buffer[200];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  out << "BEGIN:" << buffer;
  if(!mAttributed)
    out << "\r\n";
}

CCalICalendarEndToken::CCalICalendarEndToken(eCalICalendarTags aTag) : CCalICalendarToken(aTag) {
}

CCalICalendarEndToken::CCalICalendarEndToken(const nsString& aName) : CCalICalendarToken(aName) {
}

// todo: revisit
nsresult CCalICalendarEndToken::Consume(PRUnichar aChar, CScanner& aScanner) {

  //if you're here, we've already Consumed the "END:" chars, and are
  //ready to Consume the rest of the open tag identifier.
  //Stop consuming as soon as you see a space or a '\r\n'.
  //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  mTextValue="";
  nsresult result=aScanner.ReadUntil(mTextValue,kGreaterThan,PR_FALSE);

  char buffer[300];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  mTypeID= NS_CalICalendarTagToEnum(buffer);

  if(NS_OK==result)
    result=aScanner.GetChar(aChar); //eat the closing '>;
  return result;
}

PRInt32 CCalICalendarEndToken::GetTypeID(){
  if (eCalICalendarTag_unknown==mTypeID) {
    nsAutoString tmp(mTextValue);
    tmp.ToUpperCase();
    char cbuf[200];
    tmp.ToCString(cbuf, sizeof(cbuf));
    mTypeID = NS_CalICalendarTagToEnum(cbuf);
  }
  return mTypeID;
}

const char* CCalICalendarEndToken::GetClassName(void) {
  return "/end";
}

PRInt32 CCalICalendarEndToken::GetTokenType(void) {
  return eCalICalendarToken_end;
}

void CCalICalendarEndToken::DebugDumpSource(ostream& out) {
  char buffer[200];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  out << "END:" << buffer << "\r\n";
}

CCalICalendarTextToken::CCalICalendarTextToken() :
  CCalICalendarToken(eCalICalendarTag_text) {
}

CCalICalendarTextToken::CCalICalendarTextToken(const nsString& aName) :
  CCalICalendarToken(aName) {
    mTypeID=eCalICalendarTag_text;
}

const char*  CCalICalendarTextToken::GetClassName(void) {
  return "text";
}

PRInt32 CCalICalendarTextToken::GetTokenType(void) {
  return eCalICalendarToken_text;
}

//todo: revisit
nsresult CCalICalendarTextToken::Consume(PRUnichar aChar, CScanner& aScanner) {
  static    nsAutoString terminals("&<\r");
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;

  while((NS_OK==result) && (!done)) {
    result=aScanner.ReadUntil(mTextValue,terminals,PR_FALSE,PR_FALSE);
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);
      if(kCR==aChar) {
        result=aScanner.GetChar(aChar); //strip off the \r
        result=aScanner.Peek(aChar);    //then see what's next.
        switch(aChar) {
          case kCR:
            result=aScanner.GetChar(aChar); //strip off the \r
            mTextValue.Append("\n\n");
            break;
          case kNewLine:
             //which means we saw \r\n, which becomes \n
            result=aScanner.GetChar(aChar); //strip off the \n
                //now fall through on purpose...
          default:
            mTextValue.Append("\n");
            break;
        }
      }
      else done=PR_TRUE;
    }
  }
  return result;
}

CCalICalendarNewlineToken::CCalICalendarNewlineToken() : CCalICalendarToken(eCalICalendarTag_newline) {
}

CCalICalendarNewlineToken::CCalICalendarNewlineToken(const nsString& aName) : CCalICalendarToken(aName) {
  mTypeID=eCalICalendarTag_newline;
}

const char*  CCalICalendarNewlineToken::GetClassName(void) {
  return "CRLF";
}

PRInt32 CCalICalendarNewlineToken::GetTokenType(void) {
  return eCalICalendarToken_newline;
}

nsString& CCalICalendarNewlineToken::GetStringValueXXX(void) {
  static nsAutoString theStr("\n");
  return theStr;
}

nsresult CCalICalendarNewlineToken::Consume(PRUnichar aChar, CScanner& aScanner) {
  mTextValue=aChar;

    //we already read the \r or \n, let's see what's next!
  PRUnichar nextChar;
  nsresult result=aScanner.Peek(nextChar);

  if(NS_OK==result) {
    switch(aChar) {
      case kNewLine:
        if(kCR==nextChar) {
          result=aScanner.GetChar(nextChar);
          mTextValue+=nextChar;
        }
        break;
      case kCR:
        if(kNewLine==nextChar) {
          result=aScanner.GetChar(nextChar);
          mTextValue+=nextChar;
        }
        break;
      default:
        break;
    }
  }
  return result;
}

CCalICalendarAttributeToken::CCalICalendarAttributeToken() : CCalICalendarToken(eCalICalendarTag_unknown) {
}

CCalICalendarAttributeToken::CCalICalendarAttributeToken(const nsString& aName) :
  CCalICalendarToken(aName),  
  mTextKey() {
  mLastAttribute=PR_FALSE;
}

CCalICalendarAttributeToken::CCalICalendarAttributeToken(const nsString& aKey,
                                                         const nsString& aName) :
  CCalICalendarToken(aName) {
  mTextKey = aKey;
  mLastAttribute=PR_FALSE;
}

void CCalICalendarAttributeToken::Reinitialize(PRInt32 aTag, const nsString& aString){
  CCalICalendarToken::Reinitialize(aTag,aString);
  mTextKey.Truncate();
  mLastAttribute=PR_FALSE;
}

const char*  CCalICalendarAttributeToken::GetClassName(void) {
  return "attr";
}

PRInt32 CCalICalendarAttributeToken::GetTokenType(void) {
  return eCalICalendarToken_attribute;
}

void CCalICalendarAttributeToken::DebugDumpToken(ostream& out) {
  char buffer[200];
  mTextKey.ToCString(buffer,sizeof(buffer)-1);
  out << "[" << GetClassName() << "] " << buffer << "=";
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  out << buffer << ": " << mTypeID << endl;
}

PRInt32 CalICalendarConsumeQuotedString(PRUnichar aChar,nsString& aString,CScanner& aScanner){
  PRInt32 result=kNotFound;
  switch(aChar) {
    case kQuote:
      result=aScanner.ReadUntil(aString,kQuote,PR_TRUE);
      break;
    case kApostrophe:
      result=aScanner.ReadUntil(aString,kApostrophe,PR_TRUE);
      break;
    default:
      break;
  }
  PRUnichar ch=aString.Last();
  if(ch!=aChar)
    aString+=aChar;
  return result;
}

// todo: revisit
PRInt32 CalICalendarConsumeAttributeValueText(PRUnichar,nsString& aString,CScanner& aScanner){

  PRInt32 result=kNotFound;
  static nsAutoString terminals(":;");

  result=aScanner.ReadUntil(aString,terminals,PR_FALSE,PR_FALSE);
  return result;
}

PRInt32 ConsumeAttributeValueText(PRUnichar,nsString& aString,CScanner& aScanner) {
  PRInt32 result=kNotFound;
  static nsAutoString terminals(":;");
  result=aScanner.ReadUntil(aString,terminals,PR_FALSE,PR_FALSE);
  return result;
}

// todo: revisit
nsresult CCalICalendarAttributeToken::Consume(PRUnichar aChar, CScanner& aScanner) {
  
  aScanner.SkipWhitespace();             //skip leading whitespace                                      
  nsresult result=aScanner.Peek(aChar);
  if(NS_OK==result) {
    static nsAutoString terminals("\b\t\n\r \"=");
    result=aScanner.ReadUntil(mTextKey,terminals,PR_TRUE,PR_FALSE);
    
      //now it's time to Consume the (optional) value...
    if(NS_OK == (result=aScanner.SkipWhitespace())) { 
      //Skip ahead until you find an equal sign or a '>'...
        if(NS_OK == (result=aScanner.Peek(aChar))) {  
          if(kEqual==aChar){
            result=aScanner.GetChar(aChar);  //skip the equal sign...
            if(NS_OK==result) {
              result=aScanner.SkipWhitespace();     //now skip any intervening whitespace
              if(NS_OK==result) {
                result=aScanner.GetChar(aChar);  //and grab the next char.    
                if(NS_OK==result) {
                  if((kQuote==aChar) || (kApostrophe==aChar)) {
                    mTextValue=aChar;
                    result=CalICalendarConsumeQuotedString(aChar,mTextValue,aScanner);
                  }
                  else {      
                    mTextValue=aChar;       //it's an alphanum attribute...
                    result=ConsumeAttributeValueText(aChar,mTextValue,aScanner);
                  } 
                }//if
                if(NS_OK==result)
                  result=aScanner.SkipWhitespace();     
              }//if
            }//if
          }//if
        }//if
//      }if
    }
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);
      mLastAttribute= PRBool((kColon==aChar) || (kEOF==result));
    }
  }
  return result;
}

void CCalICalendarAttributeToken::DebugDumpSource(ostream& out) {
  char buffer[200];
  mTextKey.ToCString(buffer,sizeof(buffer)-1);
  out << " " << buffer;
  if(mTextValue.Length()){
    mTextValue.ToCString(buffer,sizeof(buffer)-1);
    out << "=" << buffer;
  }
  if(mLastAttribute)
    out<<">";
}

CCalICalendarWhitespaceToken::CCalICalendarWhitespaceToken() :
  CCalICalendarToken(eCalICalendarTag_whitespace) {
}

CCalICalendarWhitespaceToken::CCalICalendarWhitespaceToken(const nsString& aName) :
  CCalICalendarToken(aName) {
  mTypeID=eCalICalendarTag_whitespace;
}

const char*  CCalICalendarWhitespaceToken::GetClassName(void) {
  return "ws";
}

PRInt32 CCalICalendarWhitespaceToken::GetTokenType(void) {
  return eCalICalendarToken_whitespace;
}

nsresult CCalICalendarWhitespaceToken::Consume(PRUnichar aChar, CScanner& aScanner) {
  
  mTextValue=aChar;

  nsresult result=aScanner.ReadWhile(mTextValue,gWhitespace,PR_FALSE,PR_FALSE);
  if(NS_OK==result) {
    mTextValue.StripChars("\r");
  }
  return result;
}

const char* CalICalendarGetTagName(PRInt32 aTag) {
  const char* result = NS_CalICalendarEnumToTag((nsCalICalendarTag) aTag);
  if (0 == result) {
    if(aTag>=eCalICalendarTag_userdefined)
      result = gUserdefined;
    else result= gEmpty;
  }
  return result;
}

