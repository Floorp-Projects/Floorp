/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIHTMLFragmentContentSink.h"
#include "nsIParser.h"
#include "nsIParserService.h"
#include "nsIHTMLContent.h"
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
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"
#include "nsEscape.h"

//
// XXX THIS IS TEMPORARY CODE
// There's a considerable amount of copied code from the
// regular nsHTMLContentSink. All of it will be factored
// at some pointe really soon!
//

class nsHTMLFragmentContentSink : public nsIHTMLFragmentContentSink {
public:
  nsHTMLFragmentContentSink();
  virtual ~nsHTMLFragmentContentSink();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(void);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);
  virtual void FlushContent(PRBool aNotify) { }
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset) { return NS_OK; }

  // nsIHTMLContentSink
  NS_IMETHOD BeginContext(PRInt32 aID);
  NS_IMETHOD EndContext(PRInt32 aID);
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML();
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead();
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody();
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm();
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset();
  NS_IMETHOD IsEnabled(PRInt32 aTag, PRBool* aReturn) {
    *aReturn = PR_TRUE;
    return NS_OK;
  }
  NS_IMETHOD_(PRBool) IsFormOnStack() { return PR_FALSE; }
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap();
  NS_IMETHOD WillProcessTokens(void) { return NS_OK; }
  NS_IMETHOD DidProcessTokens(void) { return NS_OK; }
  NS_IMETHOD WillProcessAToken(void) { return NS_OK; }
  NS_IMETHOD DidProcessAToken(void) { return NS_OK; }
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode) { return NS_OK; }
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD AddHeadContent(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);

  // nsIHTMLFragmentContentSink
  NS_IMETHOD GetFragment(nsIDOMDocumentFragment** aFragment);
  NS_IMETHOD SetTargetDocument(nsIDocument* aDocument);

  nsIContent* GetCurrentContent();
  PRInt32 PushContent(nsIContent *aContent);
  nsIContent* PopContent();

  nsresult AddAttributes(const nsIParserNode& aNode,
                         nsIContent* aContent);

  nsresult AddText(const nsAString& aString);
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

  nsCOMPtr<nsIDocument> mTargetDocument;
  nsCOMPtr<nsINodeInfoManager> mNodeInfoManager;
};

class nsHTMLFragmentContentSink2 : public nsHTMLFragmentContentSink
{
public:
  nsHTMLFragmentContentSink2() { mHitSentinel = PR_TRUE; mSeenBody = PR_FALSE;}
  virtual ~nsHTMLFragmentContentSink2() {}
};

nsresult
NS_NewHTMLFragmentContentSink2(nsIHTMLFragmentContentSink** aResult)
{
  NS_PRECONDITION(aResult, "Null out ptr");

  *aResult = new nsHTMLFragmentContentSink2();
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult
NS_NewHTMLFragmentContentSink(nsIHTMLFragmentContentSink** aResult)
{
  NS_PRECONDITION(aResult, "Null out ptr");

  *aResult = new nsHTMLFragmentContentSink();
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aResult);

  return NS_OK;
}

nsHTMLFragmentContentSink::nsHTMLFragmentContentSink()
{
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

  PR_FREEIF(mText);
}

NS_IMPL_ADDREF(nsHTMLFragmentContentSink)
NS_IMPL_RELEASE(nsHTMLFragmentContentSink)

NS_INTERFACE_MAP_BEGIN(nsHTMLFragmentContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLFragmentContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIContentSink)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIHTMLFragmentContentSink)
NS_INTERFACE_MAP_END


NS_IMETHODIMP 
nsHTMLFragmentContentSink::WillBuildModel(void)
{
  if (mRoot) {
    return NS_OK;
  }

  NS_ASSERTION(mTargetDocument, "Need a document!");

  nsCOMPtr<nsIDOMDocumentFragment> frag;
  nsresult rv = NS_NewDocumentFragment(getter_AddRefs(frag), mTargetDocument);
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(frag, &mRoot);
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::DidBuildModel(void)
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
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_NOT_INITIALIZED);

  nsresult result=NS_OK;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  result = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::title, nsnull,
                                         kNameSpaceID_None,
                                         getter_AddRefs(nodeInfo));
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
nsHTMLFragmentContentSink::CloseHTML()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::OpenHead(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::CloseHead()
{
  return CloseContainer(eHTMLTag_head);
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
nsHTMLFragmentContentSink::CloseBody()
{
  return CloseContainer(eHTMLTag_body);
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::OpenForm(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::CloseForm()
{
  return CloseContainer(eHTMLTag_form);
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::OpenFrameset(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::CloseFrameset()
{
  return CloseContainer(eHTMLTag_frameset);
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::OpenMap(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::CloseMap()
{
  return CloseContainer(eHTMLTag_map);
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
    if (!mBaseHREF.IsEmpty()) {
      aContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::_baseHref, mBaseHREF, PR_FALSE);
    }
    if (!mBaseTarget.IsEmpty()) {
      aContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::_baseTarget, mBaseTarget, PR_FALSE);
    }
  }
}

static const char kSentinelStr[] = "endnote";

NS_IMETHODIMP
nsHTMLFragmentContentSink::OpenContainer(const nsIParserNode& aNode)
{
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_NOT_INITIALIZED);

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

    nsCOMPtr<nsINodeInfo> nodeInfo;

    if (nodeType == eHTMLTag_userdefined) {
      result =
        mNodeInfoManager->GetNodeInfo(aNode.GetText(), nsnull,
                                      kNameSpaceID_None,
                                      getter_AddRefs(nodeInfo));
    } else {
      nsIParserService* parserService =
        nsContentUtils::GetParserServiceWeakRef();
      if (!parserService)
        return NS_ERROR_OUT_OF_MEMORY;

      const PRUnichar *name = nsnull;

      parserService->HTMLIdToStringTag(nodeType, &name);
      NS_ASSERTION(name, "This should not happen!");

      result =
        mNodeInfoManager->GetNodeInfo(nsDependentString(name), nsnull,
                                      kNameSpaceID_None,
                                      getter_AddRefs(nodeInfo));
    }

    NS_ENSURE_SUCCESS(result, result);

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
nsHTMLFragmentContentSink::CloseContainer(const nsHTMLTag aTag)
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
nsHTMLFragmentContentSink::AddHeadContent(const nsIParserNode& aNode)
{
  return AddLeaf(aNode);
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::AddLeaf(const nsIParserNode& aNode)
{
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_NOT_INITIALIZED);

  nsresult result = NS_OK;

  switch (aNode.GetTokenType()) {
    case eToken_start:
      {
        FlushText();

        // Create new leaf content object
        nsCOMPtr<nsIHTMLContent> content;
        nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());

        nsIParserService* parserService =
          nsContentUtils::GetParserServiceWeakRef();
        if (!parserService)
          return NS_ERROR_OUT_OF_MEMORY;

        nsCOMPtr<nsINodeInfo> nodeInfo;

        if (nodeType == eHTMLTag_userdefined) {
          result =
            mNodeInfoManager->GetNodeInfo(aNode.GetText(), nsnull,
                                          kNameSpaceID_None,
                                          getter_AddRefs(nodeInfo));
        } else {
          const PRUnichar *name = nsnull;
          result = parserService->HTMLIdToStringTag(nodeType, &name);
          NS_ASSERTION(name, "This should not happen!");

          result =
            mNodeInfoManager->GetNodeInfo(nsDependentString(name), nsnull,
                                          kNameSpaceID_None,
                                          getter_AddRefs(nodeInfo));
        }

        NS_ENSURE_SUCCESS(result, result);

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
            nsCOMPtr<nsIDTD> dtd;
            mParser->GetDTD(getter_AddRefs(dtd));
            NS_ENSURE_TRUE(dtd, NS_ERROR_FAILURE);

            nsAutoString skippedContent;
            PRInt32 lineNo = 0;

            dtd->CollectSkippedContent(nodeType, skippedContent, lineNo);
            result=AddTextToContent(content, skippedContent);
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
nsHTMLFragmentContentSink::AddComment(const nsIParserNode& aNode)
{
  nsIContent *comment;
  nsIDOMComment *domComment;
  nsresult result = NS_OK;

  FlushText();

  result = NS_NewCommentNode(&comment);
  if (NS_SUCCEEDED(result)) {
    result = CallQueryInterface(comment, &domComment);
    if (NS_SUCCEEDED(result)) {
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
nsHTMLFragmentContentSink::AddDocTypeDecl(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::GetFragment(nsIDOMDocumentFragment** aFragment)
{
  if (mRoot) {
    return CallQueryInterface(mRoot, aFragment);
  }

  *aFragment = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFragmentContentSink::SetTargetDocument(nsIDocument* aTargetDocument)
{
  NS_ENSURE_ARG_POINTER(aTargetDocument);

  mTargetDocument = aTargetDocument;
  mNodeInfoManager = aTargetDocument->GetNodeInfoManager();
  if (mNodeInfoManager) {
    return NS_OK;
  }

  nsresult rv = NS_NewNodeInfoManager(getter_AddRefs(mNodeInfoManager));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mNodeInfoManager->Init(aTargetDocument);
  if (NS_FAILED(rv)) {
    mNodeInfoManager = nsnull;
  }

  return rv;
}

nsIContent*
nsHTMLFragmentContentSink::GetCurrentContent()
{
  if (nsnull != mContentStack) {
    PRInt32 indx = mContentStack->Count() - 1;
    if (indx >= 0)
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
    if (indx >= 0) {
      content = (nsIContent *)mContentStack->ElementAt(indx);
      mContentStack->RemoveElementAt(indx);
    }
  }
  return content;
}

#define NS_ACCUMULATION_BUFFER_SIZE 4096

nsresult
nsHTMLFragmentContentSink::AddText(const nsAString& aString)
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
    if (!aText.IsEmpty()) {
      nsCOMPtr<nsITextContent> text;
      result = NS_NewTextNode(getter_AddRefs(text));
      if (NS_SUCCEEDED(result)) {
        text->SetText(aText, PR_TRUE);

        result = aContent->AppendChildTo(text, PR_FALSE, PR_FALSE);
      }
    }
  }
  return result;
}

nsresult
nsHTMLFragmentContentSink::FlushText()
{
  if (0 == mTextLength) {
    return NS_OK;
  }

  nsCOMPtr<nsITextContent> content;
  nsresult rv = NS_NewTextNode(getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the text in the text node
  content->SetText(mText, mTextLength, PR_FALSE);

  // Add text to its parent
  nsIContent *parent = GetCurrentContent();

  if (!parent) {
    parent = mRoot;
  }

  rv = parent->AppendChildTo(content, PR_FALSE, PR_FALSE);

  mTextLength = 0;

  return rv;
}

// XXX Code copied from nsHTMLContentSink. It should be shared.
nsresult
nsHTMLFragmentContentSink::AddAttributes(const nsIParserNode& aNode,
                                         nsIContent* aContent)
{
  // Add tag attributes to the content attributes

  PRInt32 ac = aNode.GetAttributeCount();

  if (ac == 0) {
    // No attributes, nothing to do. Do an early return to avoid
    // constructing the nsAutoString object for nothing.

    return NS_OK;
  }

  nsCAutoString k;
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());

  // The attributes are on the parser node in the order they came in in the
  // source.  What we want to happen if a single attribute is set multiple
  // times on an element is that the first time should "win".  That is, <input
  // value="foo" value="bar"> should show "foo".  So we loop over the
  // attributes backwards; this ensures that the first attribute in the set
  // wins.  This does mean that we do some extra work in the case when the same
  // attribute is set multiple times, but we save a HasAttr call in the much
  // more common case of reasonable HTML.

  for (PRInt32 i = ac - 1; i >= 0; i--) {
    // Get lower-cased key
    const nsAString& key = aNode.GetKeyAt(i);
    // Copy up-front to avoid shared-buffer overhead (and convert to UTF-8
    // at the same time since that's what the atom table uses).
    CopyUTF16toUTF8(key, k);
    ToLowerCase(k);

    nsCOMPtr<nsIAtom> keyAtom = do_GetAtom(k);

    // Get value and remove mandatory quotes
    static const char* kWhitespace = "\n\r\t\b";
    const nsAString& v =
      nsContentUtils::TrimCharsInSet(kWhitespace, aNode.GetValueAt(i));

    if (nodeType == eHTMLTag_a && keyAtom == nsHTMLAtoms::name) {
      NS_ConvertUCS2toUTF8 cname(v);
      NS_ConvertUTF8toUCS2 uv(nsUnescape(cname.BeginWriting()));

      // Add attribute to content
      aContent->SetAttr(kNameSpaceID_None, keyAtom, uv, PR_FALSE);
    } else {
      // Add attribute to content
      aContent->SetAttr(kNameSpaceID_None, keyAtom, v, PR_FALSE);
    }
  }

  return NS_OK;
}
