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

#ifndef NS_CALICALENDARPARSERNODE__
#define NS_CALICALENDARPARSERNODE__

#include "nsIParserNode.h"
#include "nsCalICalendarTokens.h"
#include "nsString.h"
#include "nsParserCIID.h"
#include "nsCalParserCIID.h"


class nsCalICalendarCParserNode :  public nsIParserNode {
  enum {eMaxAttr=20};
  
  public:

    NS_DECL_ISUPPORTS

    /**
     * Default constructor
     * @update	gess5/11/98
     * @param   aToken is the token this node "refers" to
     */
    nsCalICalendarCParserNode(CCalICalendarToken* aToken=nsnull,PRInt32 aLineNumber=1);

    /**
     * Destructor
     * @update	gess5/11/98
     */
    virtual ~nsCalICalendarCParserNode();

    /**
     * Init
     * @update	gess5/11/98
     */
    virtual nsresult Init(CCalICalendarToken* aToken=nsnull,PRInt32 aLineNumber=1);

    /**
     * Retrieve the name of the node
     * @update	gess5/11/98
     * @return  string containing node name
     */
    virtual const nsString& GetName() const;

    /**
     * Retrieve the text from the given node
     * @update	gess5/11/98
     * @return  string containing node text
     */
    virtual const nsString& GetText() const;

    /**
     * Retrieve skipped context from node
     * @update	gess5/11/98
     * @return  string containing skipped content
     */
    virtual const nsString& GetSkippedContent() const;

    /**
     * Retrieve the type of the parser node.
     * @update	gess5/11/98
     * @return  node type.
     */
    virtual PRInt32 GetNodeType()  const;

    /**
     * Retrieve token type of parser node
     * @update	gess5/11/98
     * @return  token type
     */
    virtual PRInt32 GetTokenType()  const;

    //***************************************
    //methods for accessing key/value pairs
    //***************************************

    /**
     * Retrieve the number of attributes in this node.
     * @update	gess5/11/98
     * @return  count of attributes (may be 0)
     */
    virtual PRInt32 GetAttributeCount(PRBool askToken=PR_FALSE) const;

    /**
     * Retrieve the key (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the key you want
     * @return  string containing key.
     */
    virtual const nsString& GetKeyAt(PRInt32 anIndex) const;

    /**
     * Retrieve the value (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the value you want
     * @return  string containing value.
     */
    virtual const nsString& GetValueAt(PRInt32 anIndex) const;

    /**
     * NOTE: When the node is an entity, this will translate the entity
     *       to it's unicode value, and store it in aString.
     * @update	gess5/11/98
     * @param   aString will contain the resulting unicode string value
     * @return  int (unicode char or unicode index from table)
     */
  
    virtual PRInt32 TranslateToUnicodeStr(nsString& aString) const;
    
    /**
     * 
     * @update	gess5/11/98
     * @param 
     * @return
     */
    virtual void AddAttribute(CCalICalendarToken* aToken);

    /**
     * 
     * @update	gess5/11/98
     * @param 
     * @return
     */
    virtual void SetSkippedContent(CCalICalendarToken* aToken);

    /**
     * This getter retrieves the line number from the input source where
     * the token occured. Lines are interpreted as occuring between \n characters.
     * @update	gess7/24/98
     * @return  int containing the line number the token was found on
     */
    virtual PRInt32 GetSourceLineNumber(void) const;
              
  protected:
    PRInt32   mAttributeCount;    
    PRInt32   mLineNumber;
    CCalICalendarToken*   mToken;
    CCalICalendarToken*   mAttributes[eMaxAttr]; // XXX Ack! This needs to be dynamic! 
    CCalICalendarToken*   mSkippedContent;
    // nsAutoString  mName;

    static const nsAutoString  mEmptyString;

};

#endif


