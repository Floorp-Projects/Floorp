/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIHTMLFragmentContentSink.h"
#include "nsIParser.h"
#include "nsIParserService.h"
#include "nsParserCIID.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLTokens.h"  
#include "nsGenericHTMLElement.h"
#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMDocumentFragment.h"
#include "nsVoidArray.h"
#include "nsITextContent.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "prmem.h"
#include "nsReadableUtils.h"
#include "nsContentUtils.h"

//
// XXX THIS IS TEMPORARY CODE
// There's a considerable amount of copied code from the
// regular nsHTMLContentSink. All of it will be factored
// at some pointe really soon!
//

static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);

class nsHTMLFragmentContentSink : public nsIHTMLFragmentContentSink {
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
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode=0);

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
  NS_IMETHOD OpenNoscript(const nsIParserNode& aNode);
  NS_IMETHOD CloseNoscript(const nsIParserNode& aNode);
  NS_IMETHOD GetPref(PRInt32 aTag,PRBool& aPref) { return NS_OK; }
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap(const nsIParserNode& aNode);
  NS_IMETHOD FlushPendingNotifications() { return NS_OK; }
  NS_IMETHOD SetDocumentCharset(nsAWritableString& aCharset) { return NS_OK; }
  NS_IMETHOD WillProcessTokens(void) { return NS_OK; }
  NS_IMETHOD DidProcessTokens(void) { return NS_OK; }
  NS_IMETHOD WillProcessAToken(void) { return NS_OK; }
  NS_IMETHOD DidProcessAToken(void) { return NS_OK; }
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode) { return NS_OK; }

  NS_IMETHOD DoFragment(PRBool aFlag);

  // nsIHTMLFragmentContentSink
  NS_IMETHOD GetFragment(nsIDOMDocumentFragment** aFragment);

  nsIContent* GetCurrentContent();
  PRInt32 PushContent(nsIContent *aContent);
  nsIContent* PopContent();

  void GetAttributeValueAt(const nsIParserNode& aNode,
                           PRInt32 aIndex,
                           nsString& aResult);
  nsresult AddAttributes(const nsIParserNode& aNode,
                         nsIContent* aContent);

  nsresult AddText(const nsAReadableString& aString);
  nsresult AddTextToContent(nsIHTMLContent* aContent,const nsString& aText);
  nsresult FlushText();

  void ProcessBaseTag(nsIHTMLContent* aContent);
  void AddBaseTagInfo(nsIHTMLContent* aContent);

  nsresult Init();

  PRBool mHitSentinel;
  PRBool mSeenBody;

  nsIContent* mRoot;
  nsIParser* mParser;
  nsIDOMHTMLFormElement* mCurrentForm;
  nsIHTMLContent* mCurrentMap;

  nsVoidArray* mContentStack;

  PRUnichar* mText;
  PRInt32 mTextLength;
  PRInt32 mTextSize;

  nsString mBaseHREF;
  nsString mBaseTarget;

  nsCOMPtr<nsINodeInfoManager> mNodeInfoManager;
};


nsresult
NS_NewHTMLFragmentContentSink(nsIHTMLFragmentContentSink** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLFragmentContentSink* it;
  NS_NEWXPCOM(it, nsHTMLFragmentContentSink);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init();
  if (NS_FAILED(rv)) {
    delete it;
    return rv;
  }

  return it->QueryInterface(NS_GET_IID(nsIHTMLFragmentContentSink), (void **)aResult);
}

nsHTMLFragmentContentSink::nsHTMLFragmentContentSink()
{
  NS_INIT_REFCNT();
  mHitSentinel = PR_FALSE;
  mSeenBody = PR_TRUE;
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
  // Should probably flush the text buffer here, just to make sure:
  //FlushText();

  NS_IF_RELEASE(mRoot);
  NS_IF_RELEASE(mParser);
  NS_IF_RELEASE(mCurrentForm);
  NS_IF_RELEASE(mCurrentMap);
  if (nsnull != mContentStack) {
    // there shouldn't be anything here except in an error condition
    PRInt32 indx = mContentStack->Count();
    while (0 < indx--) {
      nsIContent* content = (nsIContent*)mContentStack->ElementAt(indx);
      NS_RELEASE(content);
    }
    delete mContentStack;
  }
  if (nsnull != mText) {
    PR_FREEIF(mText);
  }
}

NS_IMPL_ADDREF(nsHTMLFragmentContentSink)
NS_IMPL_RELEASE(nsHTMLFragmentContentSink)

NS_IMETHODIMP 
nsHTMLFragmentContentSink::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                         
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  if (aIID.Equals(NS_GET_IID(nsIHTMLFragmentContentSink))) {
    nsIHTMLFragmentContentSink* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  } 
  if (aIID.Equals(NS_GET_IID(nsIHTMLContentSink))) {
    nsIHTMLContentSink* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  } 
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


nsresult nsHTMLFragmentContentSink::Init()
{
  nsresult rv = NS_NewNodeInfoManager(getter_AddRefs(mNodeInfoManager));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINameSpaceManager> nsmgr;
  rv = NS_NewNameSpaceManager(getter_AddRefs(nsmgr));
  NS_ENSURE_SUCCESS(rv, rv);

  return mNodeInfoManager->Init(nsnull, nsmgr);
}


NS_IMETHODIMP 
nsHTMLFragmentContentSink::WillBuildModel(void)
{
  nsresult result = NS_OK;

  if (nsnull == mRoot) {
    nsIDOMDocumentFragment* frag;

    result = NS_NewDocumentFragment(&frag, nsnull);
    if (NS_SUCCEEDED(result)) {
      result = frag->QueryInterface(NS_GET_IID(nsIContent), (void**)&mRoot);
      NS_RELEASE(frag);
    }
  }
  return result;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
  FlushText();

  // Drop our reference to the parser to get rid of a circular
  // reference.
  NS_IF_RELEASE(mParser);

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
  nsresult result=NS_OK;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  result = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::title, nsnull, 
                                         kNameSpaceID_None,
                                         *getter_AddRefs(nodeInfo));
  if(NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIHTMLContent> content=nsnull;
    result = NS_NewHTMLTitleElement(getter_AddRefs(content), nodeInfo);

    if (NS_SUCCEEDED(result)) {
      nsIContent *parent = GetCurrentContent();
  
      if (nsnull == parent) {
        parent = mRoot;
      }
      
      result=parent->AppendChildTo(content, PR_FALSE, PR_FALSE);
      
      if (NS_SUCCEEDED(result)) {       
        result=AddTextToContent(content,aValue);
      }
    }
  }

  return result;
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
  // XXX Not likely to get a head in the fragment
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseHead(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenBody(const nsIParserNode& aNode)
{
  // Ignore repeated BODY elements. The DTD is just sending them
  // to us for compatibility reasons that don't apply here.
  if (!mSeenBody) {
    mSeenBody = PR_TRUE;
    return OpenContainer(aNode);
  }
  else {
    return NS_OK;
  }
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

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenNoscript(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseNoscript(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

void
nsHTMLFragmentContentSink::ProcessBaseTag(nsIHTMLContent* aContent)
{
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, value)) {
    mBaseHREF = value;
  }
  if (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::target, value)) {
    mBaseTarget = value;
  }
}

void
nsHTMLFragmentContentSink::AddBaseTagInfo(nsIHTMLContent* aContent)
{
  if (aContent) {
    if (mBaseHREF.Length() > 0) {
      aContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::_baseHref, mBaseHREF, PR_FALSE);
    }
    if (mBaseTarget.Length() > 0) {
      aContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::_baseTarget, mBaseTarget, PR_FALSE);
    }
  }
}

static char* kSentinelStr = "endnote";

NS_IMETHODIMP 
nsHTMLFragmentContentSink::OpenContainer(const nsIParserNode& aNode)
{
  nsAutoString tag;
  nsresult result = NS_OK;

  tag.Assign(aNode.GetText());
  if (tag.EqualsIgnoreCase(kSentinelStr)) {
    mHitSentinel = PR_TRUE;
  }
  else if (mHitSentinel) {
    FlushText();
    
    nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
    nsIHTMLContent *content = nsnull;

    nsCOMPtr<nsIParserService> parserService = 
             do_GetService(kParserServiceCID, &result);
    NS_ENSURE_SUCCESS(result, result);

    nsAutoString tmpName;

    if (nodeType == eHTMLTag_userdefined) {
      tmpName.Assign(aNode.GetText());
    } else {
      result = parserService->HTMLIdToStringTag(nodeType, tmpName);
      NS_ENSURE_SUCCESS(result, result);
    }

    nsCOMPtr<nsINodeInfo> nodeInfo;
    result =
      mNodeInfoManager->GetNodeInfo(tmpName, nsnull, kNameSpaceID_None,
                                    *getter_AddRefs(nodeInfo));

    result = NS_CreateHTMLElement(&content, nodeInfo, PR_FALSE);

    if (NS_OK == result) {
      result = AddAttributes(aNode, content);
      if (NS_OK == result) {
        nsIContent *parent = GetCurrentContent();
        
        if (nsnull == parent) {
          parent = mRoot;
        }
        
        parent->AppendChildTo(content, PR_FALSE, PR_FALSE);
        PushContent(content);
      }
    }

    if (nodeType == eHTMLTag_table
        || nodeType == eHTMLTag_thead
        || nodeType == eHTMLTag_tbody
        || nodeType == eHTMLTag_tfoot
        || nodeType == eHTMLTag_tr
        || nodeType == eHTMLTag_td
        || nodeType == eHTMLTag_th)
      // XXX if navigator_quirks_mode (only body in html supports background)
      AddBaseTagInfo(content);     
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::CloseContainer(const nsIParserNode& aNode)
{
  if (mHitSentinel && (nsnull != GetCurrentContent())) {
    nsIContent* content;
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

  switch (aNode.GetTokenType()) {
    case eToken_start:
      {
        FlushText();
        
        // Create new leaf content object
        nsCOMPtr<nsIHTMLContent> content;
        nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());

        nsCOMPtr<nsIParserService> parserService = 
                 do_GetService(kParserServiceCID, &result);

        NS_ENSURE_SUCCESS(result, result);

        nsAutoString tmpName;

        if (nodeType == eHTMLTag_userdefined) {
          tmpName.Assign(aNode.GetText());
        } else {
          result = parserService->HTMLIdToStringTag(nodeType, tmpName);
          NS_ENSURE_SUCCESS(result, result);
        }

        nsCOMPtr<nsINodeInfo> nodeInfo;
        result = mNodeInfoManager->GetNodeInfo(tmpName, nsnull, kNameSpaceID_None,
                                               *getter_AddRefs(nodeInfo));

        if(NS_SUCCEEDED(result)) {
          result = NS_CreateHTMLElement(getter_AddRefs(content), nodeInfo, PR_FALSE);

          if (NS_OK == result) {
            result = AddAttributes(aNode, content);
            if (NS_OK == result) {
              nsIContent *parent = GetCurrentContent();
            
              if (nsnull == parent) {
                parent = mRoot;
              }
            
              parent->AppendChildTo(content, PR_FALSE, PR_FALSE);
            }
          }

          if(nodeType == eHTMLTag_plaintext || 
             nodeType == eHTMLTag_script    ||
             nodeType == eHTMLTag_style     ||
             nodeType == eHTMLTag_textarea  ||
             nodeType == eHTMLTag_xmp) {
            
            // Create a text node holding the content
            result=AddTextToContent(content,aNode.GetSkippedContent());
          }
          else if (nodeType == eHTMLTag_img || nodeType == eHTMLTag_frame
              || nodeType == eHTMLTag_input)    // elements with 'SRC='
              AddBaseTagInfo(content);
          else if (nodeType == eHTMLTag_base)
              ProcessBaseTag(content);
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
    result = comment->QueryInterface(NS_GET_IID(nsIDOMComment), 
                                     (void **)&domComment);
    if (NS_OK == result) {
      domComment->AppendData(aNode.GetText());
      NS_RELEASE(domComment);
      
      nsIContent *parent = GetCurrentContent();
      
      if (nsnull == parent) {
        parent = mRoot;
      }
      
      parent->AppendChildTo(comment, PR_FALSE, PR_FALSE);
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

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */

NS_IMETHODIMP
nsHTMLFragmentContentSink::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::DoFragment(PRBool aFlag)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFragmentContentSink::GetFragment(nsIDOMDocumentFragment** aFragment)
{
  if (nsnull != mRoot) {
    return mRoot->QueryInterface(NS_GET_IID(nsIDOMDocumentFragment), (void**)aFragment);
  }
  else {
    *aFragment = nsnull;
    return NS_OK;
  }
}

nsIContent* 
nsHTMLFragmentContentSink::GetCurrentContent()
{
  if (nsnull != mContentStack) {
    PRInt32 indx = mContentStack->Count() - 1;
    return (nsIContent *)mContentStack->ElementAt(indx);
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
    PRInt32 indx = mContentStack->Count() - 1;
    content = (nsIContent *)mContentStack->ElementAt(indx);
    mContentStack->RemoveElementAt(indx);
  }
  return content;
}

#define NS_ACCUMULATION_BUFFER_SIZE 4096

nsresult
nsHTMLFragmentContentSink::AddText(const nsAReadableString& aString)
{
  PRInt32 addLen = aString.Length();
  if (0 == addLen) {
    return NS_OK;
  }

  // Create buffer when we first need it
  if (0 == mTextSize) {
    mText = (PRUnichar *) PR_MALLOC(sizeof(PRUnichar) * NS_ACCUMULATION_BUFFER_SIZE);
    if (nsnull == mText) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTextSize = NS_ACCUMULATION_BUFFER_SIZE;
  }

  // Copy data from string into our buffer; flush buffer when it fills up
  PRInt32 offset = 0;
  PRBool  isLastCharCR = PR_FALSE;
  while (0 != addLen) {
    PRInt32 amount = mTextSize - mTextLength;
    if (amount > addLen) {
      amount = addLen;
    }
    if (0 == amount) {
      nsresult rv = FlushText();
      if (NS_OK != rv) {
        return rv;
      }
    }
    mTextLength +=
      nsContentUtils::CopyNewlineNormalizedUnicodeTo(aString, 
                                                     offset,
                                                     &mText[mTextLength], 
                                                     amount,
                                                     isLastCharCR);
    offset += amount;
    addLen -= amount;
  }

  return NS_OK;
}

nsresult
nsHTMLFragmentContentSink::AddTextToContent(nsIHTMLContent* aContent,const nsString& aText) {
  NS_ASSERTION(aContent !=nsnull, "can't add text w/o a content");

  nsresult result=NS_OK;
  
  if(aContent) {
    if (aText.Length() > 0) {
      nsCOMPtr<nsIContent> text;
      result = NS_NewTextNode(getter_AddRefs(text));
      if (NS_SUCCEEDED(result)) {
        nsCOMPtr<nsIDOMText> tc=do_QueryInterface(text,&result);
        if (NS_SUCCEEDED(result)) {
          tc->SetData(aText);
        }
        result=aContent->AppendChildTo(text, PR_FALSE, PR_FALSE);
      }
    }
  }
  return result;
}

nsresult
nsHTMLFragmentContentSink::FlushText()
{
  nsresult rv = NS_OK;
  if (0 != mTextLength) {
    nsIContent* content;
    rv = NS_NewTextNode(&content);
    if (NS_OK == rv) {
      
      // Set the text in the text node
      nsITextContent* text = nsnull;
      content->QueryInterface(NS_GET_IID(nsITextContent), (void**) &text);
      text->SetText(mText, mTextLength, PR_FALSE);
      NS_RELEASE(text);

      // Add text to its parent
      nsIContent *parent = GetCurrentContent();
      
      if (nsnull == parent) {
        parent = mRoot;
      }
      
      parent->AppendChildTo(content, PR_FALSE, PR_FALSE);

      NS_RELEASE(content);
    }

    mTextLength = 0;
  }

  return rv;
}

// XXX Code copied from nsHTMLContentSink. It should be shared.
void
nsHTMLFragmentContentSink::GetAttributeValueAt(const nsIParserNode& aNode,
                                               PRInt32 aIndex,
                                               nsString& aResult)
{
  // Copy value
  const nsString& value = aNode.GetValueAt(aIndex);
  aResult.Truncate();
  aResult.Append(value);

  // Strip quotes if present
  if (aResult.Length() > 0) {
    PRUnichar first = aResult.First();
    if ((first == '\"') || (first == '\'')) {
      if (aResult.Last() == first) {
        aResult.Cut(0, 1);
        PRInt32 pos = aResult.Length() - 1;
        if (pos >= 0) {
          aResult.Cut(pos, 1);
        }
      } else {
        // Mismatched quotes - leave them in
      }
    }
  }
}

// XXX Code copied from nsHTMLContentSink. It should be shared.
nsresult
nsHTMLFragmentContentSink::AddAttributes(const nsIParserNode& aNode,
                                         nsIContent* aContent)
{
  // Add tag attributes to the content attributes
  nsAutoString k, v;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsAReadableString& key = aNode.GetKeyAt(i);
    k.Truncate();
    k.Append(key);
    k.ToLowerCase();

    nsIAtom*  keyAtom = NS_NewAtom(k);
    
    if (NS_CONTENT_ATTR_NOT_THERE == 
        aContent->GetAttr(kNameSpaceID_HTML, keyAtom, v)) {
      // Get value and remove mandatory quotes
      GetAttributeValueAt(aNode, i, v);

      // Add attribute to content
      aContent->SetAttr(kNameSpaceID_HTML, keyAtom, v, PR_FALSE);
    }
    NS_RELEASE(keyAtom);
  }
  return NS_OK;
}
