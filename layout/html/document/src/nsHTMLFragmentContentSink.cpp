/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#include "nsCOMPtr.h"
#include "nsIHTMLContentSink.h"
#include "nsIParser.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsHTMLTokens.h"  
#include "nsHTMLEntities.h" 
#include "nsHTMLParts.h"

//
// XXX THIS IS TEMPORARY CODE
// There's a considerable amount of copied code from the
// regular nsHTMLContentSink. All of it will be factored
// at some pointe really soon!
//

class nsHTMLFragmentContentSink : public nsIHTMLContentSink {
public:
  nsHTMLFragmentContentSink();
  virtual ~nsHTMLFragmentContentSink();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);  
  
  // nsIHTMLContentSink
  NS_IMETHOD BeginContext(PRInt32 aID);
  NS_IMETHOD EndContext(PRInt32 aID);
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML(const nsIParserNode& aNode);
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead(const nsIParserNode& aNode);
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody(const nsIParserNode& aNode);
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm(const nsIParserNode& aNode);
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset(const nsIParserNode& aNode);
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap(const nsIParserNode& aNode);
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD DoFragment(PRBool aFlag);


  nsIContent* GetCurrentContent();
  PRInt32 PushContent(nsIContent *aContent);
  nsIContent* PopContent();

  nsresult AddAttributes(const nsIParserNode& aNode,
                         nsIContent* aContent,
                         PRBool aIsHTML);

  nsresult AddText(const nsString& aString);
  nsresult FlushText(PRBool aCreateTextNode=PR_TRUE,
                     PRBool* aDidFlush=nsnull);


  PRBool mHitSentinel;

  nsIContent* mRoot;
  nsIParser* mParser;
  nsIDOMHTMLFormElement* mCurrentForm;
  nsIHTMLContent* mCurrentMap;

  nsVoidArray* mContentStack;

  PRUnichar* mText;
  PRInt32 mTextLength;
  PRInt32 mTextSize;
};


nsHTMLFragmentContentSink::nsHTMLFragmentContentSink()
{
  mHitSentinel = PR_FALSE;
  mRoot = nsnull;
  mParser = nsnull;
  mCurrentForm = nsnull;
  mCurrentMap = nsnull;
  mContentStack = nsnull;
  mText = nsnull;
  mTextLength = 0;
  mTextSize = 0;
}

nsHTMLFragmentContentSink::~nsHTMLFragmentContentSink()
{
  NS_IF_RELEASE(mRoot);
  NS_IF_RELEASE(mParser);
  NS_IF_RELEASE(mCurrentForm);
  NS_IF_RELEASE(mCurrentMap);
  if (nsnull != mContentStack) {
    // there shouldn't be anything here except in an error condition
    PRInt32 index = mContentStack->Count();
    while (0 < index--) {
      nsIContent* content = (nsIContent*)mContentStack->ElementAt(index);
      NS_RELEASE(content);
    }
    delete mContentStack;
  }
  if (nsnull != mText) {
    PR_FREEIF(mText);
  }
}

NS_IMPL_ISUPPORTS(nsHTMLFragmentContentSink, kIHTMLContentSinkIID)


NS_IMETHODIMP 
nsHTMLFragmentContentSink::WillBuildModel(void)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::WillInterrupt(void)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::WillResume(void)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::SetParser(nsIParser* aParser)
{
  NS_IF_RELEASE(mParser);
  mParser = aParser;
  NS_IF_ADDREF(mParser);
  
  return NS_OK;
}
  
NS_IMETHODIMP 
nsHTMLFragmentContentSink::BeginContext(PRInt32 aID)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::EndContext(PRInt32 aID)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::SetTitle(const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenHTML(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseHTML(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenHead(const nsIParserNode& aNode)
{
  // XXX Not likely to get a head in the paste
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseHead(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenBody(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseBody(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenForm(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseForm(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenFrameset(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseFrameset(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenMap(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseMap(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

static char* kSentinelStr = "endnote";

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenContainer(const nsIParserNode& aNode)
{
  nsAutoString tag;

  tag = aNode.GetText();
  if (tag.EqualsIgnoreCase(kSentinelStr)) {
    mHitSentinel = PR_TRUE;
  }
  else if (mHitSentinel) {
    FlushText();
    
    nsIHTMLContent *content = nsnull;
    result = NS_CreateHTMLElement(&content, tag);

    if (NS_OK == result) {
      result = AddAttributes(aNode, content);
      if (NS_OK == result) {
        nsIContent *parent = GetCurrentContent();
	
	if (nsnull == parent) {
	  parent = mRoot;
	}

        parent->AppendChildTo(content, PR_FALSE);
        PushContent(content);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseContainer(const nsIParserNode& aNode)
{
  if (mHitSentinel && (nsnull != GetCurrentContent()) {
    nsIContent content;
    FlushText();
    content = PopContent();
    NS_RELEASE(content);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::AddLeaf(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());

  switch (nodeType) {
    case eToken_start:
      {
	FlushText();
	
	// Create new leaf content object
	nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
	nsIHTMLContent* content;
	result = NS_CreateHTMLElement(&content, tag);
	
	if (NS_OK == result) {
	  result = AddAttributes(aNode, content);
	  if (NS_OK == result) {
	    nsIContent *parent = GetCurrentContent();
	    
	    if (nsnull == parent) {
	      parent = mRoot;
	    }
	    
	    parent->AppendChildTo(content, PR_FALSE);
	  }
	  NS_RELEASE(content);
	}
      }
      break;
    case eToken_text:
    case eToken_whitespace:
    case eToken_newline:
      result = AddText(aNode.GetText());
      break;

    case eToken_entity:
      {
	nsAutoString tmp;
	PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp);
	if (unicode < 0) {
	  result = AddText(aNode.GetText());
	}
	else {
	  result = AddText(tmp);
	}
      }
      break;
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::NotifyError(const nsParserError* aError)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::AddComment(const nsIParserNode& aNode)
{
  nsIContent *comment;
  nsIDOMComment *domComment;
  nsresult result = NS_OK;

  FlushText();
  
  result = NS_NewCommentNode(&comment);
  if (NS_OK == result) {
    result = comment->QueryInterface(kIDOMCommentIID, 
                                     (void **)&domComment);
    if (NS_OK == result) {
      domComment->AppendData(aNode.GetText());
      NS_RELEASE(domComment);
      
      nsIContent *parent = GetCurrentContent();
      
      if (nsnull == parent) {
	parent = mRoot;
      }
      
      parent->AppendChildTo(comment, PR_FALSE);
    }
    NS_RELEASE(comment);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::DoFragment(PRBool aFlag)
{
  return NS_OK;
}


nsIContent* 
nsHTMLFragmentContentSink::GetCurrentContent()
{
  if (nsnull != mContentStack) {
    PRInt32 index = mContentStack->Count() - 1;
    return (nsIContent *)mContentStack->ElementAt(index);
  }
  return nsnull;
}

PRInt32 
nsHTMLFragmentContentSink::PushContent(nsIContent *aContent)
{
  if (nsnull == mContentStack) {
    mContentStack = new nsVoidArray();
  }
  
  mContentStack->AppendElement((void *)aContent);
  return mContentStack->Count();
}
 
nsIContent*
nsHTMLFragmentContentSink::PopContent()
{
  nsIContent* content = nsnull;
  if (nsnull != mContentStack) {
    PRInt32 index = mContentStack->Count() - 1;
    content = (nsIContent *)mContentStack->ElementAt(index);
    mContentStack->RemoveElementAt(index);
  }
  return content;
}
