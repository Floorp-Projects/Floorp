/* -*- Mode: C++ tab-width: 2 indent-tabs-mode: nil c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
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

#include "nsTextEditor.h"
#include "nsIEditorSupport.h"
#include "nsEditorEventListeners.h"
#include "nsIEditProperty.h"

#include "nsIStreamListener.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIDocument.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLContentSinkStream.h"
#include "nsHTMLToTXTSinkStream.h"
#include "nsXIFDTD.h"


#include "nsIDOMDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCharacterData.h"
#include "nsEditorCID.h"
#include "nsISupportsArray.h"
#include "nsIEnumerator.h"
#include "nsIContentIterator.h"
#include "nsIContent.h"
#include "nsLayoutCID.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"


class nsIFrame;

#ifdef XP_UNIX
#include <strstream.h>
#endif

#ifdef XP_MAC
#include <sstream>
#include <string>
#endif

#ifdef XP_PC
#include <strstrea.h>
#endif

#include "CreateElementTxn.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,   NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIEditPropertyIID,     NS_IEDITPROPERTY_IID);

static NS_DEFINE_CID(kEditorCID,      NS_EDITOR_CID);
static NS_DEFINE_IID(kIEditorIID,     NS_IEDITOR_IID);
static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITextEditorIID, NS_ITEXTEDITOR_IID);
static NS_DEFINE_CID(kTextEditorCID,  NS_TEXTEDITOR_CID);

static NS_DEFINE_IID(kIContentIteratorIID, NS_ICONTENTITERTOR_IID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);

nsTextEditor::nsTextEditor()
{
// Done in nsEditor
//  NS_INIT_REFCNT();
}

nsTextEditor::~nsTextEditor()
{
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  nsCOMPtr<nsIDOMDocument> doc;
  Inherited::GetDocument(getter_AddRefs(doc));
  if (doc)
  {
    nsCOMPtr<nsIDOMEventReceiver> erP;
    nsresult result = doc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
    if (NS_SUCCEEDED(result) && erP) 
    {
      if (mKeyListenerP) {
        erP->RemoveEventListener(mKeyListenerP, kIDOMKeyListenerIID);
      }
      if (mMouseListenerP) {
        erP->RemoveEventListener(mMouseListenerP, kIDOMMouseListenerIID);
      }
    }
    else
      NS_NOTREACHED("~nsTextEditor");
  }
}

// Adds appropriate AddRef, Release, and QueryInterface methods for derived class
//NS_IMPL_ISUPPORTS_INHERITED(nsTextEditor, nsEditor, nsITextEditor)

//NS_IMPL_ADDREF_INHERITED(Class, Super)
NS_IMETHODIMP_(nsrefcnt) nsTextEditor::AddRef(void)
{
  return Inherited::AddRef();
}

//NS_IMPL_RELEASE_INHERITED(Class, Super)
NS_IMETHODIMP_(nsrefcnt) nsTextEditor::Release(void)
{
  return Inherited::Release();
}

//NS_IMPL_QUERY_INTERFACE_INHERITED(Class, Super, AdditionalInterface)
NS_IMETHODIMP nsTextEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
 
  if (aIID.Equals(nsITextEditor::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsITextEditor*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return Inherited::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP nsTextEditor::Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull!=aDoc) && (nsnull!=aPresShell))
  {
    // Init the base editor
    result = Inherited::Init(aDoc, aPresShell);
    if (NS_OK != result) 
      return result;

    result = NS_NewEditorKeyListener(getter_AddRefs(mKeyListenerP), this);
    if (NS_OK != result) {
      return result;
    }
    result = NS_NewEditorMouseListener(getter_AddRefs(mMouseListenerP), this);
    if (NS_OK != result) {
      // drop the key listener if we couldn't get a mouse listener.
      mKeyListenerP = do_QueryInterface(0); 
      return result;
    }

    nsCOMPtr<nsIDOMEventReceiver> erP;
    result = aDoc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
    if (NS_OK != result) 
    {
      mKeyListenerP = do_QueryInterface(0);
      mMouseListenerP = do_QueryInterface(0); //dont need these if we cant register them
      return result;
    }
    //cmanske: Shouldn't we check result from this?
    erP->AddEventListener(mKeyListenerP, kIDOMKeyListenerIID);
    //erP->AddEventListener(mMouseListenerP, kIDOMMouseListenerIID);

    result = NS_OK;

		EnableUndo(PR_TRUE);
  }
  return result;
}

// this is a total hack for now.  We don't yet have a way of getting the style properties
// of the current selection, so we can't do anything useful here except show off a little.
NS_IMETHODIMP nsTextEditor::SetTextProperty(nsIAtom *aProperty)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;

  nsresult result=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  result = Inherited::GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    Inherited::BeginTransaction();
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection, &result);
    if ((NS_SUCCEEDED(result)) && enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      result = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(result)) && (nsnull!=currentItem))
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        nsCOMPtr<nsIDOMNode>commonParent;
        result = range->GetCommonParent(getter_AddRefs(commonParent));
        if ((NS_SUCCEEDED(result)) && commonParent)
        {
          PRInt32 startOffset, endOffset;
          range->GetStartOffset(&startOffset);
          range->GetEndOffset(&endOffset);
          nsCOMPtr<nsIDOMNode> startParent;  nsCOMPtr<nsIDOMNode> endParent;
          range->GetStartParent(getter_AddRefs(startParent));
          range->GetEndParent(getter_AddRefs(endParent));
          if (startParent.get()==endParent.get()) 
          { // the range is entirely contained within a single text node
            result = SetTextPropertiesForNode(startParent, commonParent, 
                                              startOffset, endOffset,
                                              aProperty);
          }
          else
          {
            nsCOMPtr<nsIDOMNode> startGrandParent;
            startParent->GetParentNode(getter_AddRefs(startGrandParent));
            nsCOMPtr<nsIDOMNode> endGrandParent;
            endParent->GetParentNode(getter_AddRefs(endGrandParent));
            if (NS_SUCCEEDED(result))
            {
              if (endGrandParent.get()==startGrandParent.get())
              { // the range is between 2 nodes that have a common (immediate) grandparent
                result = SetTextPropertiesForNodesWithSameParent(startParent,startOffset,
                                                                 endParent,  endOffset,
                                                                 commonParent,
                                                                 aProperty);
              }
              else
              { // the range is between 2 nodes that have no simple relationship
                result = SetTextPropertiesForNodeWithDifferentParents(range,
                                                                      startParent,startOffset, 
                                                                      endParent,  endOffset,
                                                                      commonParent,
                                                                      aProperty);
              }
            }
          }
          if (NS_SUCCEEDED(result))
          { // compute a range for the selection
            // don't want to actually do anything with selection, because
            // we are still iterating through it.  Just want to create and remember
            // an nsIDOMRange, and later add the range to the selection after clearing it.
            // XXX: I'm blocked here because nsIDOMSelection doesn't provide a mechanism
            //      for setting a compound selection yet.
          }
        }
      }
    }
    Inherited::EndTransaction();
    if (NS_SUCCEEDED(result))
    { // set the selection
      // XXX: can't do anything until I can create ranges
    }
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::GetTextProperty(nsIAtom *aProperty, PRBool &aAny, PRBool &aAll)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;

  nsresult result=NS_ERROR_NOT_INITIALIZED;
  aAny=PR_FALSE;
  aAll=PR_TRUE;
  nsCOMPtr<nsIDOMSelection>selection;
  result = Inherited::GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection, &result);
    if ((NS_SUCCEEDED(result)) && enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      result = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(result)) && currentItem)
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        nsCOMPtr<nsIContentIterator> iter;
        result = nsRepository::CreateInstance(kCContentIteratorCID, nsnull,
                                              kIContentIteratorIID, 
                                              getter_AddRefs(iter));
        if ((NS_SUCCEEDED(result)) && iter)
        {
          iter->Init(range);
          return 0;
          // loop through the content iterator for each content node
          // for each text node:
          // get the frame for the content, and from it the style context
          // ask the style context about the property
          nsCOMPtr<nsIContent> content;
          result = iter->CurrentNode(getter_AddRefs(content));
          int i=0;
          while (NS_COMFALSE == iter->IsDone())
          {
            nsCOMPtr<nsIDOMCharacterData>text;
            text = do_QueryInterface(content);
            if (text)
            {
              nsCOMPtr<nsIPresShell>presShell;
              Inherited::GetPresShell(getter_AddRefs(presShell));
              NS_ASSERTION(presShell, "bad state, null pres shell");
              if (presShell)
              {
                nsIFrame *frame;
                result = presShell->GetPrimaryFrameFor(content, &frame);
                if ((NS_SUCCEEDED(result)) && frame)
                {
                  nsCOMPtr<nsIStyleContext> sc;
                  result = presShell->GetStyleContextFor(frame, getter_AddRefs(sc));
                  if ((NS_SUCCEEDED(result)) && sc)
                  {
                    PRBool isSet;
                    IsTextStyleSet(sc, aProperty, isSet);
                    if (PR_TRUE==isSet) {
                      aAny = PR_TRUE;
                    }
                    else {
                      aAll = PR_FALSE;
                    }
                  }
                }
              }
            }
            iter->Next();
            result = iter->CurrentNode(getter_AddRefs(content));
          }
        }
      }
    }
  }
  return result;
}

void nsTextEditor::IsTextStyleSet(nsIStyleContext *aSC, 
                                  nsIAtom *aProperty, 
                                  PRBool &aIsSet) const
{
  aIsSet = PR_FALSE;
  if (aSC && aProperty)
  {
    nsStyleFont* font = (nsStyleFont*)aSC->GetStyleData(eStyleStruct_Font);
    if (nsIEditProperty::italic==aProperty)
    {
      aIsSet = PRBool(font->mFont.style & NS_FONT_STYLE_ITALIC);
    }
    else if (nsIEditProperty::bold==aProperty)
    { // XXX: check this logic with Peter
      aIsSet = PRBool(font->mFont.weight > NS_FONT_WEIGHT_NORMAL);
    }
  }
}


NS_IMETHODIMP nsTextEditor::RemoveTextProperty(nsIAtom *aProperty)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::DeleteSelection(nsIEditor::Direction aDir)
{
  return Inherited::DeleteSelection(aDir);
}

NS_IMETHODIMP nsTextEditor::InsertText(const nsString& aStringToInsert)
{
  return Inherited::InsertText(aStringToInsert);
}

NS_IMETHODIMP nsTextEditor::InsertBreak(PRBool aCtrlKey)
{
  // Note: Code that does most of the deletion work was 
  // moved to nsEditor::DeleteSelectionAndCreateNode
  // Only difference is we now do BeginTransaction()/EndTransaction()
  //  even if we fail to get a selection
  nsresult result = Inherited::BeginTransaction();

  nsCOMPtr<nsIDOMNode> newNode;
  nsAutoString tag("BR");
  Inherited::DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));

  // Are we supposed to release newNode?
  result = Inherited::EndTransaction();

  return result;
}

NS_IMETHODIMP nsTextEditor::EnableUndo(PRBool aEnable)
{
  return Inherited::EnableUndo(aEnable);
}

NS_IMETHODIMP nsTextEditor::Undo(PRUint32 aCount)
{
  return Inherited::Undo(aCount);
}

NS_IMETHODIMP nsTextEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  return Inherited::CanUndo(aIsEnabled, aCanUndo);
}

NS_IMETHODIMP nsTextEditor::Redo(PRUint32 aCount)
{
  return Inherited::Redo(aCount);
}

NS_IMETHODIMP nsTextEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  return Inherited::CanRedo(aIsEnabled, aCanRedo);
}

NS_IMETHODIMP nsTextEditor::BeginTransaction()
{
  return Inherited::BeginTransaction();
}

NS_IMETHODIMP nsTextEditor::EndTransaction()
{
  return Inherited::EndTransaction();
}

NS_IMETHODIMP nsTextEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::ScrollUp(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::ScrollDown(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  return Inherited::ScrollIntoView(aScrollToBegin);
}

NS_IMETHODIMP nsTextEditor::Insert(nsIInputStream *aInputStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef XP_MAC 
void WriteFromStringstream(stringstream& aIn, nsIOutputStream* aOut)
{
  if (aOut != nsnull)
  {
    string      theString = aIn.str();
    PRInt32     len = theString.length();
    const char* str = theString.data();
    PRUint32 outCount = 0;

    if (len)
    {
      char * ptr = NS_CONST_CAST(char*,str);
      for (PRInt32 plen = len; plen > 0; plen --, ptr ++)
        if (*ptr == '\n')
          *ptr = '\r';
      aOut->Write(ptr, len, &outCount); 
    }
  }
}
#else
static 
void WriteFromOstrstream(ostrstream& aIn, nsIOutputStream* aOut)
{
  if (aOut != nsnull)
  {
    char* str = aIn.str();
    PRUint32 inCount = aIn.pcount();
    PRUint32 outCount = 0;
    if (str != nsnull)
    {
      aOut->Write(str, inCount, &outCount); 
      // in ostrstreams if you call the str() function
      // then you are responsible for deleting the string
      delete str;
    }
  }
}
#endif

NS_IMETHODIMP nsTextEditor::OutputText(nsIOutputStream *aOutputStream)
{
#ifdef XP_MAC
  stringstream out;
#else
  ostrstream out;
#endif

  nsresult result=NS_ERROR_FAILURE;
  nsIPresShell* shell = nsnull;
  Inherited::GetPresShell(&shell);

  if (nsnull != shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsString buffer;

      doc->CreateXIF(buffer);

      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      nsresult rv = nsRepository::CreateInstance(kCParserCID, 
                                                 nsnull, 
                                                 kCParserIID, 
                                                 (void **)&parser);

      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;

        rv = NS_New_HTMLToTXT_SinkStream(&sink);
	
        if (aOutputStream != nsnull)
          ((nsHTMLContentSinkStream*)sink)->SetOutputStream(out);

        if (NS_OK == rv) {
          parser->SetContentSink(sink);
	  
          nsIDTD* dtd = nsnull;
          rv = NS_NewXIFDTD(&dtd);
          if (NS_OK == rv) {
            parser->RegisterDTD(dtd);
            parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
          }
          #ifdef XP_MAC
          WriteFromStringstream(out,aOutputStream);
          #else
          WriteFromOstrstream(out,aOutputStream);
          #endif

          NS_IF_RELEASE(dtd);
          NS_IF_RELEASE(sink);
        }
        NS_RELEASE(parser);
      }
    }
    NS_RELEASE(shell);
  }
  return result;
}



NS_IMETHODIMP nsTextEditor::OutputHTML(nsIOutputStream *aOutputStream)
{
#ifdef XP_MAC
  stringstream out;
#else
  ostrstream out;
#endif

  
  nsresult result=NS_ERROR_FAILURE;
  nsIPresShell* shell = nsnull;
  Inherited::GetPresShell(&shell);

  if (nsnull != shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsString buffer;

      doc->CreateXIF(buffer);

      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      nsresult rv = nsRepository::CreateInstance(kCParserCID, 
                                                 nsnull, 
                                                 kCParserIID, 
                                                 (void **)&parser);

      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;

        rv = NS_New_HTML_ContentSinkStream(&sink);
	
        if (aOutputStream)
          ((nsHTMLContentSinkStream*)sink)->SetOutputStream(out);

        if (NS_OK == rv) {
          parser->SetContentSink(sink);
	  
          nsIDTD* dtd = nsnull;
          rv = NS_NewXIFDTD(&dtd);
          if (NS_OK == rv) {
            parser->RegisterDTD(dtd);
            parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
          }
          #ifdef XP_MAC
          WriteFromStringstream(out,aOutputStream);
          #else
          WriteFromOstrstream(out,aOutputStream);
          #endif
          NS_IF_RELEASE(dtd);
          NS_IF_RELEASE(sink);
        }
        NS_RELEASE(parser);
      }
    }
    NS_RELEASE(shell);
  }
  return result;
}


NS_IMETHODIMP nsTextEditor::SetTextPropertiesForNode(nsIDOMNode *aNode, 
                                                nsIDOMNode *aParent,
                                                PRInt32 aStartOffset,
                                                PRInt32 aEndOffset,
                                                nsIAtom *aPropName)
{
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar =  do_QueryInterface(aNode);
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;
  PRUint32 count;
  nodeAsChar->GetLength(&count);

  nsCOMPtr<nsIDOMNode>newTextNode;  // this will be the text node we move into the new style node
  if (aStartOffset!=0)
  {
    result = Inherited::SplitNode(aNode, aStartOffset, getter_AddRefs(newTextNode));
  }
  if (NS_SUCCEEDED(result))
  {
    if (aEndOffset!=(PRInt32)count)
    {
      result = Inherited::SplitNode(aNode, aEndOffset-aStartOffset, getter_AddRefs(newTextNode));
    }
    else
    {
      newTextNode = do_QueryInterface(aNode);
    }
    if (NS_SUCCEEDED(result))
    {
      nsAutoString tag;
      result = SetTagFromProperty(tag, aPropName);
      if (NS_SUCCEEDED(result))
      {
        PRInt32 offsetInParent;
        result = nsIEditorSupport::GetChildOffset(aNode, aParent, offsetInParent);
        if (NS_SUCCEEDED(result))
        {
          nsCOMPtr<nsIDOMNode>newStyleNode;
          result = Inherited::CreateNode(tag, aParent, offsetInParent, getter_AddRefs(newStyleNode));
          if (NS_SUCCEEDED(result))
          {
            result = Inherited::DeleteNode(newTextNode);
            if (NS_SUCCEEDED(result)) {
              result = Inherited::InsertNode(newTextNode, newStyleNode, 0);
            }
          }
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP 
nsTextEditor::SetTextPropertiesForNodesWithSameParent(nsIDOMNode *aStartNode,
                                                      PRInt32     aStartOffset,
                                                      nsIDOMNode *aEndNode,
                                                      PRInt32     aEndOffset,
                                                      nsIDOMNode *aParent,
                                                      nsIAtom    *aPropName)
{
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMNode>newLeftTextNode;  // this will be the middle text node
  if (0!=aStartOffset) {
    result = Inherited::SplitNode(aStartNode, aStartOffset, getter_AddRefs(newLeftTextNode));
  }
  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMCharacterData>endNodeAsChar;
    endNodeAsChar = do_QueryInterface(aEndNode);
    if (!endNodeAsChar)
      return NS_ERROR_FAILURE;
    PRUint32 count;
    endNodeAsChar->GetLength(&count);
    nsCOMPtr<nsIDOMNode>newRightTextNode;  // this will be the middle text node
    if ((PRInt32)count!=aEndOffset) {
      result = Inherited::SplitNode(aEndNode, aEndOffset, getter_AddRefs(newRightTextNode));
    }
    else {
      newRightTextNode = do_QueryInterface(aEndNode);
    }
    if (NS_SUCCEEDED(result))
    {
      PRInt32 offsetInParent;
      if (newLeftTextNode) {
        result = nsIEditorSupport::GetChildOffset(newLeftTextNode, aParent, offsetInParent);
      }
      else {
        offsetInParent = -1; // relies on +1 below in call to CreateNode
      }
      if (NS_SUCCEEDED(result))
      {
        nsAutoString tag;
        result = SetTagFromProperty(tag, aPropName);
        if (NS_SUCCEEDED(result))
        { // create the new style node, which will be the new parent for the selected nodes
          nsCOMPtr<nsIDOMNode>newStyleNode;
          result = Inherited::CreateNode(tag, aParent, offsetInParent+1, getter_AddRefs(newStyleNode));
          if (NS_SUCCEEDED(result))
          { // move the right half of the start node into the new style node
            nsCOMPtr<nsIDOMNode>intermediateNode;
            result = aStartNode->GetNextSibling(getter_AddRefs(intermediateNode));
            if (NS_SUCCEEDED(result))
            {
              result = Inherited::DeleteNode(aStartNode);
              if (NS_SUCCEEDED(result)) 
              { 
                PRInt32 childIndex=0;
                result = Inherited::InsertNode(aStartNode, newStyleNode, childIndex);
                childIndex++;
                if (NS_SUCCEEDED(result))
                { // move all the intermediate nodes into the new style node
                  nsCOMPtr<nsIDOMNode>nextSibling;
                  while (intermediateNode.get() != aEndNode)
                  {
                    if (!intermediateNode)
                      result = NS_ERROR_NULL_POINTER;
                    if (NS_FAILED(result)) {
                      break;
                    }
                    // get the next sibling before moving the current child!!!
                    intermediateNode->GetNextSibling(getter_AddRefs(nextSibling));
                    result = Inherited::DeleteNode(intermediateNode);
                    if (NS_SUCCEEDED(result)) {
                      result = Inherited::InsertNode(intermediateNode, newStyleNode, childIndex);
                      childIndex++;
                    }
                    intermediateNode = do_QueryInterface(nextSibling);
                  }
                  if (NS_SUCCEEDED(result))
                  { // move the left half of the end node into the new style node
                    result = Inherited::DeleteNode(newRightTextNode);
                    if (NS_SUCCEEDED(result)) 
                    {
                      result = Inherited::InsertNode(newRightTextNode, newStyleNode, childIndex);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsTextEditor::SetTextPropertiesForNodeWithDifferentParents(nsIDOMRange *aRange,
                                                           nsIDOMNode  *aStartNode,
                                                           PRInt32      aStartOffset,
                                                           nsIDOMNode  *aEndNode,
                                                           PRInt32      aEndOffset,
                                                           nsIDOMNode  *aParent,
                                                           nsIAtom     *aPropName)
{
  nsresult result=NS_OK;
  if (!aRange || !aStartNode || !aEndNode || !aParent || !aPropName)
    return NS_ERROR_NULL_POINTER;
  // create a style node for the text in the start parent
  nsCOMPtr<nsIDOMNode>parent;
  result = aStartNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(result)) {
    return result;
  }
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar = do_QueryInterface(aStartNode);
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;
  PRUint32 count;
  nodeAsChar->GetLength(&count);
  result = SetTextPropertiesForNode(aStartNode, parent, aStartOffset, count, aPropName);

  // create a style node for the text in the end parent
  result = aEndNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(result)) {
    return result;
  }
  nodeAsChar = do_QueryInterface(aEndNode);
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;
  nodeAsChar->GetLength(&count);
  result = SetTextPropertiesForNode(aEndNode, parent, 0, aEndOffset, aPropName);

  // create style nodes for all the content between the start and end nodes
  nsCOMPtr<nsIContentIterator>iter;
  result = nsRepository::CreateInstance(kCContentIteratorCID, nsnull,
                                        kIContentIteratorIID, getter_AddRefs(iter));
  if ((NS_SUCCEEDED(result)) && iter)
  {
    nsCOMPtr<nsIContent>startContent;
    startContent = do_QueryInterface(aStartNode);
    nsCOMPtr<nsIContent>endContent;
    endContent = do_QueryInterface(aEndNode);
    if (startContent && endContent)
    {
      iter->Init(aRange);
      nsCOMPtr<nsIContent> content;
      iter->CurrentNode(getter_AddRefs(content));
      nsAutoString tag;
      result = SetTagFromProperty(tag, aPropName);
      if (NS_FAILED(result)) {
        return result;
      }
      while (NS_COMFALSE == iter->IsDone())
      {
        if ((content.get() != startContent.get()) &&
            (content.get() != endContent.get()))
        {
          nsCOMPtr<nsIDOMCharacterData>charNode;
          charNode = do_QueryInterface(content);
          if (charNode)
          {
            // only want to wrap the text node in a new style node if it doesn't already have that style
            nsCOMPtr<nsIDOMNode>parent;
            charNode->GetParentNode(getter_AddRefs(parent));
            if (!parent) {
              return NS_ERROR_NULL_POINTER;
            }
            nsCOMPtr<nsIContent>parentContent;
            parentContent = do_QueryInterface(parent);
            
            PRInt32 offsetInParent;
            parentContent->IndexOf(content, offsetInParent);

            nsCOMPtr<nsIDOMNode>newStyleNode;
            result = Inherited::CreateNode(tag, parent, offsetInParent, getter_AddRefs(newStyleNode));
            if (NS_SUCCEEDED(result) && newStyleNode) {
              nsCOMPtr<nsIDOMNode>contentNode;
              contentNode = do_QueryInterface(content);
              result = Inherited::DeleteNode(contentNode);
              if (NS_SUCCEEDED(result)) {
                result = Inherited::InsertNode(contentNode, newStyleNode, 0);
              }
            }
          }
        }
        iter->Next();
        result = iter->CurrentNode(getter_AddRefs(content));
      }
    }
  }


  return result;
}



NS_IMETHODIMP 
nsTextEditor::SetTagFromProperty(nsAutoString &aTag, nsIAtom *aPropName) const
{
  if (!aPropName) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsIEditProperty::bold==aPropName) {
    aTag = "b";
  }
  else if (nsIEditProperty::italic==aPropName) {
    aTag = "i";
  }
  else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

