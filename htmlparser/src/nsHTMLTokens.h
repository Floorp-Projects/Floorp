
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
 * This file contains the declarations for all the HTML specific token types that 
 * our DTD's understand. In fact, the same set of token types are used for XML.
 * Currently we have tokens for text, comments, start and end tags, entities, 
 * attributes, style, script and skipped content. Whitespace and newlines also
 * have their own token types, but don't count on them to stay forever.
 * 
 * If you're looking for the html tags, they're in a file called nsHTMLTag.h/cpp.
 *
 * Most of the token types have a similar API. They have methods to get the type
 * of token (GetTokenType); those that represent HTML tags also have a method to
 * get type tag type (GetTypeID). In addition, most have a method that causes the
 * token to help in the parsing process called (Consume). We've also thrown in a 
 * few standard debugging methods as well. 
 */

#ifndef HTMLTOKENS_H
#define HTMLTOKENS_H

#include "nsToken.h"
#include "nsHTMLTags.h"
#include "nsParserError.h"
#include <iostream.h>

class nsScanner;

  /*******************************************************************
   * This enum defines the set of token types that we currently support.
   *******************************************************************/

enum eHTMLTokenTypes {
  eToken_unknown=0,
  eToken_start=1,      eToken_end,     eToken_comment,         eToken_entity,
  eToken_whitespace,   eToken_newline, eToken_text,            eToken_attribute,
  eToken_script,       eToken_style,   eToken_skippedcontent,  eToken_instruction,
  eToken_cdatasection, eToken_error,
  eToken_last //make sure this stays the last token...
};

enum eHTMLCategory {
  eHTMLCategory_unknown=0,
  eHTMLCategory_inline,
  eHTMLCategory_block,
  eHTMLCategory_blockAndInline,
  eHTMLCategory_list,
  eHTMLCategory_table,
  eHTMLCategory_tablepart,
  eHTMLCategory_tablerow,
  eHTMLCategory_tabledata,
  eHTMLCategory_head,
  eHTMLCategory_html,
  eHTMLCategory_body,
  eHTMLCategory_form,
  eHTMLCategory_options,
  eHTMLCategory_frameset,
  eHTMLCategory_text
};


#define eHTMLTags nsHTMLTag

nsresult      ConsumeQuotedString(PRUnichar aChar,nsString& aString,nsScanner& aScanner);
nsresult      ConsumeAttributeText(PRUnichar aChar,nsString& aString,nsScanner& aScanner);
const char*   GetTagName(PRInt32 aTag);
//PRInt32     FindEntityIndex(nsString& aString,PRInt32 aCount=-1);


/**
 *  This declares the basic token type used in the HTML DTD's.
 *  @update  gess 3/25/98
 */
class CHTMLToken : public CToken {
public:

                        CHTMLToken(eHTMLTags aTag);
                        CHTMLToken(const nsString& aString,eHTMLTags aTag=eHTMLTag_unknown);
    virtual void        SetStringValue(const char* name);

protected:
};

/**
 *  This declares start tokens, which always take the form <xxxx>. 
 *	This class also knows how to consume related attributes.
 *  
 *  @update  gess 3/25/98
 */
class CStartToken: public CHTMLToken {
  public:
                        CStartToken(eHTMLTags aTag);
                        CStartToken(nsString& aName,eHTMLTags aTag=eHTMLTag_unknown);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
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


/**
 *  This declares end tokens, which always take the 
 *  form </xxxx>. This class also knows how to consume
 *  related attributes.
 *  
 *  @update  gess 3/25/98
 */
class CEndToken: public CHTMLToken {
  public:
                        CEndToken(eHTMLTags aTag);
                        CEndToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
    virtual PRInt32     GetTypeID(void);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
    virtual void        DebugDumpSource(ostream& out);
};


/**
 *  This declares comment tokens. Comments are usually 
 *  thought of as tokens, but we treat them that way 
 *  here so that the parser can have a consistent view
 *  of all tokens.
 *  
 *  @update  gess 3/25/98
 */
class CCommentToken: public CHTMLToken {
  public:
                        CCommentToken();
                        CCommentToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
            char        mLeadingChar;
};


/**
 *  This class declares entity tokens, which always take
 *  the form &xxxx;. This class also offers a few utility
 *  methods that allow you to easily reduce entities.
 *  
 *  @update  gess 3/25/98
 */
class CEntityToken : public CHTMLToken {
  public:
                        CEntityToken();
                        CEntityToken(const nsString& aString);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
            PRInt32     TranslateToUnicodeStr(nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
    static  PRInt32     ConsumeEntity(PRUnichar aChar,nsString& aString,nsScanner& aScanner);
    static  PRInt32     TranslateToUnicodeStr(PRInt32 aValue,nsString& aString);
//    static  PRInt32     FindEntityIndex(nsString& aString);
//    static  PRInt32     FindEntityIndexMax(const char* aBuffer,PRInt32 aCount=-1);
//    static  PRBool      VerifyEntityTable(void);
//    static  PRInt32     ReduceEntities(nsString& aString);
    virtual  void       DebugDumpSource(ostream& out);
};


/**
 *  Whitespace tokens are used where whitespace can be 
 *  detected as distinct from text. This allows us to 
 *  easily skip leading/trailing whitespace when desired.
 *  
 *  @update  gess 3/25/98
 */
class CWhitespaceToken: public CHTMLToken {
  public:
                        CWhitespaceToken();
                        CWhitespaceToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
};

/**
 *  Text tokens contain the normalized form of html text.
 *  These tokens are guaranteed not to contain entities,
 *  start or end tags, or newlines.
 *  
 *  @update  gess 3/25/98
 */
class CTextToken: public CHTMLToken {
  public:
                        CTextToken();
                        CTextToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
};


/**
 *  CDATASection tokens contain raw unescaped text content delimited by
 *  a ![CDATA[ and ]]. 
 *  XXX Not really a HTML construct - maybe we need a separation
 *  
 *  @update  vidur 11/12/98
 */
class CCDATASectionToken : public CHTMLToken {
public:
                        CCDATASectionToken();
                        CCDATASectionToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);  
};



/**
 *  Attribute tokens are used to contain attribute key/value
 *  pairs whereever they may occur. Typically, they should
 *  occur only in start tokens. However, we may expand that
 *  ability when XML tokens become commonplace.
 *  
 *  @update  gess 3/25/98
 */
class CAttributeToken: public CHTMLToken {
  public:
                          CAttributeToken();
                          CAttributeToken(const nsString& aString);
                          CAttributeToken(const nsString& aKey, const nsString& aString);
    virtual nsresult      Consume(PRUnichar aChar,nsScanner& aScanner);
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


/**
 *  Newline tokens contain, you guessed it, newlines. 
 *  They consume newline (CR/LF) either alone or in pairs.
 *  
 *  @update  gess 3/25/98
 */
class CNewlineToken: public CHTMLToken { 
  public:
                        CNewlineToken();
                        CNewlineToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
    virtual nsString&   GetStringValueXXX(void);
};


/**
 *  Script tokens contain sequences of javascript (or, gulp,
 *  any other script you care to send). We don't tokenize
 *  it here, nor validate it. We just wrap it up, and pass
 *  it along to the html parser, who sends it (later on) 
 *  to the scripting engine.
 *  
 *  @update  gess 3/25/98
 */
class CScriptToken: public CHTMLToken {
  public:
                        CScriptToken();
                        CScriptToken(const nsString& aString);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
  protected:
};


/**
 *  Style tokens contain sequences of css style. We don't 
 *  tokenize it here, nor validate it. We just wrap it up, 
 *  and pass it along to the html parser, who sends it 
 *  (later on) to the style engine.
 *  
 *  @update  gess 3/25/98
 */
class CStyleToken: public CHTMLToken {
  public:
                         CStyleToken();
                         CStyleToken(const nsString& aString);
    virtual const char*  GetClassName(void);
    virtual PRInt32      GetTokenType(void);
  protected:
};


/**
 *  This is a placeholder token, which is being deprecated.
 *  Don't bother paying attention to this.
 *  
 *  @update  gess 3/25/98
 */
class CSkippedContentToken: public CAttributeToken {
  public:
                        CSkippedContentToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
    virtual void        DebugDumpSource(ostream& out);
  protected:
};


/**
 *  Whitespace tokens are used where whitespace can be 
 *  detected as distinct from text. This allows us to 
 *  easily skip leading/trailing whitespace when desired.
 *  
 *  @update  gess 3/25/98
 */
class CInstructionToken: public CHTMLToken {
  public:
                        CInstructionToken();
                        CInstructionToken(const nsString& aString);
    virtual nsresult    Consume(PRUnichar aChar,nsScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
};

class CErrorToken : public CHTMLToken {
public:
  CErrorToken(nsParserError* aError=0);
  ~CErrorToken();
  virtual const char* GetClassName(void);
  virtual PRInt32     GetTokenType(void);
  
  void SetError(nsParserError* aError);  // CErrorToken takes ownership of aError

  // The nsParserError object returned by GetError is still owned by CErrorToken.
  // DO NOT use the delete operator on it.  Should we change this so that a copy
  // of nsParserError is returned which needs to be destroyed by the consumer?
  const nsParserError* GetError(void);   

protected:
  nsParserError* mError;
};

#endif




