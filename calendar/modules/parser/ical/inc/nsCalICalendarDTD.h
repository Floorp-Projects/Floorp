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

#ifndef NS_CALICALENDARDTD__
#define NS_CALICALENDARDTD__

#include "nscalexport.h"
#include "CNavDTD.h"
#include "nscalstrings.h"
#include "nsxpfc.h"
#include "nsIDTD.h"
#include "nsISupports.h"
#include "nsIParser.h"
#include "nsCalICalendarTags.h"
#include "nsCalICalendarTokens.h"
#include "nscalicalendarpars.h"
#include "nsVoidArray.h"
#include "nsDeque.h"
//#include "nsCalICalendarParserCIID.h"
//#include "nsCalICalendarStrings.h" // todo:

class nsIHTMLContentSink;
class nsIDTDDebug;
class nsIParserNode;
class nsCParserNode;
class CITokenHandler;
class nsParser;
class nsDTDContext;
class nsTagStack;
class CScanner;
/*
enum eCalICalendarComponents
{
  eCalICalendarComponents_vcalendar = 0,
  eCalICalendarComponents_vevent,
  eCalICalendarComponents_vtodo,
  eCalICalendarComponents_vjournal,
  eCalICalendarComponents_vfreebusy,
  eCalICalendarComponents_valarm,
  eCalICalendarComponents_vtimezone
};

enum eCalICalendarProperties
{
  eCalICalendarProperties_attach = 0,
  // todo: finish
};
*/

class nsCalICalendarDTD : public nsIDTD {

public:

  NS_DECL_ISUPPORTS

  nsCalICalendarDTD();
  virtual ~nsCalICalendarDTD();

  //nsIDID
  virtual PRBool CanParse(nsString& aContentType, PRInt32 aVersion);
  virtual eAutoDetectResult AutoDetectContentType(nsString& aBuffer,nsString& aType);
  NS_IMETHOD HandleToken(CToken* aToken);
  virtual nsresult CreateNewInstance(nsIDTD** aInstancePtrResult);

  nsresult HandleBeginToken(CToken* aToken);
  nsresult HandleEndToken(CToken* aToken);
  nsresult HandleAttributeToken(CToken* aToken);
  nsresult HandlePropertyValueToken(CToken* aToken);
  

  //more nsIDID
  virtual void SetParser(nsIParser* aParser);
  virtual nsIContentSink* SetContentSink(nsIContentSink* aSink);
  NS_IMETHOD WillBuildModel(nsString& aFilename,PRBool aNotifySink);
  NS_IMETHOD DidBuildModel(PRInt32 anErrorCode,PRBool aNotifySink);
  NS_IMETHOD ConsumeToken(CToken*& aToken);
  NS_IMETHOD WillResumeParse(void);
  NS_IMETHOD WillInterruptParse(void);
  virtual PRBool CanContain(PRInt32 aParent,PRInt32 aChild) const;
  virtual PRBool IsContainer(PRInt32 aTag) const;
  virtual PRBool Verify(nsString& aURLRef);
  virtual nsITokenRecycler* GetTokenRecycler(void);
  
  /*nsresult ConsumeBeginTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
  nsresult ConsumeEndTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
  */
  nsresult ConsumeNewline(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
  nsresult ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
  nsresult ConsumePropertyLine(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
  nsresult ConsumePropertyNameAndAttributes(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
  nsresult ConsumePropertyValue(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);

  nsresult ConsumeAttributes(PRUnichar aChar, CScanner& aScanner,
                             CCalICalendarBeginToken* aToken);
private:
  //NS_IMETHOD_(eCalICalendarTags) ComponentTypeFromObject(const nsIParserNode& aNode); 

protected:
  PRInt32 CollectPropertyValue(nsCParserNode& aNode, PRInt32 aCount);
  PRInt32 CollectAttributes(nsCParserNode& aNode, PRInt32 aCount);
  
  nsParser* mParser;
  nsIContentSink* mSink;
  //CITokenHandler* mTokenHandlers[eToken_last];
  //nsDTDContext*   mContext;
  nsDeque         mTokenDeque;
  nsString        mFilename;
  nsIDTDDebug*    mDTDDebug;
  PRInt32         mLineNumber;
  eParseMode      mParseMode;
};

extern NS_CALICALENDARPARS nsresult NS_NewICAL_DTD(nsIDTD** aInstancePtrResult);

#endif












