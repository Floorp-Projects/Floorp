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

#ifndef CALICALENDARTOKENS_H_
#define CALICALENDARTOKENS_H_

#include "nsToken.h"
#include "nsCalICalendarTags.h"
#include <iostream.h>

const PRUint32 kSemiColon = ';';
const PRUint32 kColon = ':';
const PRUint32 kNewline = '\n';
const PRUint32 kCarriageReturn = '\r';
class CScanner;
class CToken;

enum eCalICalendarTokenTypes {
  eCalICalendarToken_unknown = 0,
  eCalICalendarToken_begin=1,
  eCalICalendarToken_end,
  eCalICalendarToken_whitespace,
  eCalICalendarToken_newline,
  eCalICalendarToken_text,
  eCalICalendarToken_attribute,
  eCalICalendarToken_propertyvalue,
  eCalICalendarToken_propertyname,
  eCalICalendarToken_identifier,
  eCalICalendarToken_last // make sure this stays the last token...
};

#define eCalICalendarTags nsCalICalendarTag

PRInt32       CalICalendarConsumeQuotedString(PRUnichar aChar,nsString& aString,CScanner& aScanner);
PRInt32       CalICalendarConsumeAttributeText(PRUnichar aChar,nsString& aString,CScanner& aScanner);
const char *  CalICalendarGetTagName(PRInt32 aTag);

class CCalICalendarToken: public CToken {
public:

               CCalICalendarToken(eCalICalendarTags aTag);
               CCalICalendarToken(const nsString& aString,eCalICalendarTags aTag=eCalICalendarTag_unknown);
  virtual void SetStringValue(const char* name);

protected:
};

class CCalICalendarIdentifierToken: public CCalICalendarToken {
public:
                      CCalICalendarIdentifierToken(eCalICalendarTags aTag);
                      CCalICalendarIdentifierToken(const nsString& aString,eCalICalendarTags aTag=eCalICalendarTag_unknown);

  virtual nsresult    Consume(PRUnichar aChar,CScanner& aScanner);
  virtual PRInt32     GetTypeID(void);
  virtual const char* GetClassName(void);
  virtual PRInt32     GetTokenType(void);
  virtual void        DebugDumpSource(ostream& out);
};

class CCalICalendarPropertyValueToken: public CCalICalendarToken {
public:
                      CCalICalendarPropertyValueToken();
                      CCalICalendarPropertyValueToken(eCalICalendarTags aTag);
                      CCalICalendarPropertyValueToken(const nsString& aString,eCalICalendarTags aTag=eCalICalendarTag_unknown);

  virtual nsresult    Consume(PRUnichar aChar,CScanner& aScanner);
  virtual PRInt32     GetTypeID(void);
  virtual const char* GetClassName(void);
  virtual PRInt32     GetTokenType(void);
  virtual void        DebugDumpSource(ostream& out);
};

class CCalICalendarBeginToken: public CCalICalendarToken {
public:

                      CCalICalendarBeginToken(eCalICalendarTags aTag);
                      CCalICalendarBeginToken(const nsString& aName, eCalICalendarTags aTag=eCalICalendarTag_unknown);
  virtual nsresult    Consume(PRUnichar aChar,CScanner& aScanner);
  virtual PRInt32     GetTypeID(void);
  virtual const char* GetClassName(void);  
  virtual PRInt32     GetTokenType(void);

          PRBool      IsAttributed(void);
          void        SetAttributed(PRBool aValue);
          PRBool      IsEmpty(void);
          void        SetEmpty(PRBool aValue);
  virtual void        DebugDumpSource(ostream& out);
  virtual void        Reinitialize(PRInt32 aTag, const nsString& aString);
  
protected:
          PRBool      mAttributed;      
          PRBool      mEmpty;      
};

class CCalICalendarEndToken: public CCalICalendarToken {
public:
                      CCalICalendarEndToken(eCalICalendarTags aTag);
                      CCalICalendarEndToken(const nsString& aString, eCalICalendarTags aTag=eCalICalendarTag_unknown);
  virtual nsresult    Consume(PRUnichar aChar,CScanner& aScanner);
  virtual PRInt32     GetTypeID(void);
  virtual const char* GetClassName(void);
  virtual PRInt32     GetTokenType(void);
  virtual void        DebugDumpSource(ostream& out);
};

class CCalICalendarWhitespaceToken: public CCalICalendarToken {
public:
                        CCalICalendarWhitespaceToken();
                        CCalICalendarWhitespaceToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
};

class CCalICalendarTextToken: public CCalICalendarToken {
public:
                        CCalICalendarTextToken();
                        CCalICalendarTextToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
};

class CCalICalendarAttributeToken: public CCalICalendarToken {
  public:
                          CCalICalendarAttributeToken();
                          CCalICalendarAttributeToken(const nsString& aString);
                          CCalICalendarAttributeToken(const nsString& aKey, const nsString& aString);
    virtual nsresult      Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char*   GetClassName(void);
    virtual PRInt32       GetTokenType(void);
    virtual nsString&     GetKey(void) {return mTextKey;}
    virtual void          DebugDumpToken(ostream& out);
    virtual void          DebugDumpSource(ostream& out);
            PRBool        mLastAttribute;
    virtual void          Reinitialize(PRInt32 aTag, const nsString& aString);

  protected:
             nsString mTextKey;
};

class CCalICalendarNewlineToken: public CCalICalendarToken { 
  public:
                        CCalICalendarNewlineToken();
                        CCalICalendarNewlineToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
    virtual nsString&   GetStringValueXXX(void);
};

#endif







