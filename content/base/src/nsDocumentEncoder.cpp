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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsIDocumentEncoder.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h" 
#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsISelection.h"
#include "nsCOMPtr.h"
#include "nsIContentSerializer.h"
#include "nsIUnicodeEncoder.h"
#include "nsIOutputStream.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMComment.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsRange.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsHTMLAtoms.h"
#include "nsITextContent.h"
#include "nsIEnumerator.h"
#include "nsISelectionPrivate.h"
#include "nsIFrameSelection.h"
#include "nsIContentIterator.h"
#include "nsISupportsArray.h"
#include "nsIParserService.h"
#include "nsParserCIID.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);

nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult);
nsresult NS_NewDomSelection(nsISelection **aDomSelection);

enum nsRangeIterationDirection {
  kDirectionOut = -1,
  kDirectionIn = 1
};

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsDocumentEncoder declaration 
#pragma mark -
#endif

class nsDocumentEncoder : public nsIDocumentEncoder
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOCUMENT_ENCODER_IID; return iid; }

  nsDocumentEncoder();
  virtual ~nsDocumentEncoder();

  NS_IMETHOD Init(nsIDocument* aDocument, const nsAReadableString& aMimeType, PRUint32 aFlags);

  /* Interfaces for addref and release and queryinterface */
  NS_DECL_ISUPPORTS

  // Inherited methods from nsIDocumentEncoder
  NS_IMETHOD SetSelection(nsISelection* aSelection);
  NS_IMETHOD SetRange(nsIDOMRange* aRange);
  NS_IMETHOD SetWrapColumn(PRUint32 aWC);
  NS_IMETHOD SetCharset(const nsAReadableString& aCharset);
  NS_IMETHOD GetMimeType(nsAWritableString& aMimeType);
  NS_IMETHOD EncodeToStream(nsIOutputStream* aStream);
  NS_IMETHOD EncodeToString(nsAWritableString& aOutputString);
  NS_IMETHOD EncodeToStringWithContext(nsAWritableString& aEncodedString, 
                                       nsAWritableString& aContextString,
                                       nsAWritableString& aInfoString);
  NS_IMETHOD SetNodeFixup(nsIDocumentEncoderNodeFixup *aFixup);
                                       
protected:
  nsresult SerializeNodeStart(nsIDOMNode* aNode, PRInt32 aStartOffset,
                              PRInt32 aEndOffset, nsAWritableString& aStr);
  nsresult SerializeToStringRecursive(nsIDOMNode* aNode,
                                      nsAWritableString& aStr);
  nsresult SerializeNodeEnd(nsIDOMNode* aNode, nsAWritableString& aStr);
  nsresult SerializeRangeToString(nsIDOMRange *aRange,
                                  nsAWritableString& aOutputString);
  nsresult SerializeRangeNodes(nsIDOMRange* aRange, 
                               nsIDOMNode* aNode, 
                               nsAWritableString& aString,
                               PRInt32 aDepth);
  nsresult SerializeRangeContextStart(const nsVoidArray& aAncestorArray,
                                      nsAWritableString& aString);
  nsresult SerializeRangeContextEnd(const nsVoidArray& aAncestorArray,
                                    nsAWritableString& aString);

  nsresult FlushText(nsAWritableString& aString, PRBool aForce);

  static PRBool IsTag(nsIDOMNode* aNode, nsIAtom* aAtom);
  
  virtual PRBool IncludeInContext(nsIDOMNode *aNode);

  nsCOMPtr<nsIDocument>          mDocument;
  nsCOMPtr<nsISelection>         mSelection;
  nsCOMPtr<nsIDOMRange>          mRange;
  nsCOMPtr<nsIOutputStream>      mStream;
  nsCOMPtr<nsIContentSerializer> mSerializer;
  nsCOMPtr<nsIUnicodeEncoder>    mUnicodeEncoder;
  nsCOMPtr<nsIDOMNode>           mCommonParent;
  nsCOMPtr<nsIDocumentEncoderNodeFixup> mNodeFixup;
  nsCOMPtr<nsICharsetConverterManager2> mCharsetConverterManager;

  nsString          mMimeType;
  nsString          mCharset;
  PRUint32          mFlags;
  PRUint32          mWrapColumn;
  PRUint32          mStartDepth;
  PRUint32          mEndDepth;
  PRInt32           mStartRootIndex;
  PRInt32           mEndRootIndex;
  PRBool            mHaltRangeHint;  
  nsAutoVoidArray   mCommonAncestors;
  nsAutoVoidArray   mStartNodes;
  nsAutoVoidArray   mStartOffsets;
  nsAutoVoidArray   mEndNodes;
  nsAutoVoidArray   mEndOffsets;
};

#ifdef XP_MAC
#pragma mark  nsDocumentEncoder implementation 
#pragma mark -
#endif

NS_IMPL_ADDREF(nsDocumentEncoder)
NS_IMPL_RELEASE(nsDocumentEncoder)

NS_INTERFACE_MAP_BEGIN(nsDocumentEncoder)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentEncoder)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsDocumentEncoder::nsDocumentEncoder()
{
  NS_INIT_REFCNT();

  mMimeType.AssignWithConversion("text/plain");

  mFlags = 0;
  mWrapColumn = 72;
  mStartDepth = 0;
  mEndDepth = 0;
  mHaltRangeHint = PR_FALSE;
}

nsDocumentEncoder::~nsDocumentEncoder()
{
}

static PRBool
IsScriptEnabled(nsIDocument *aDoc)
{
  NS_ENSURE_TRUE(aDoc, PR_TRUE);

  nsCOMPtr<nsIScriptSecurityManager> sm(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(sm, PR_TRUE);

  nsCOMPtr<nsIPrincipal> principal;
  aDoc->GetPrincipal(getter_AddRefs(principal));
  NS_ENSURE_TRUE(principal, PR_TRUE);

  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  aDoc->GetScriptGlobalObject(getter_AddRefs(globalObject));
  NS_ENSURE_TRUE(globalObject, PR_TRUE);

  nsCOMPtr<nsIScriptContext> scriptContext;
  globalObject->GetContext(getter_AddRefs(scriptContext));
  NS_ENSURE_TRUE(scriptContext, PR_TRUE);

  JSContext* cx = (JSContext *) scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, PR_TRUE);

  PRBool enabled = PR_TRUE;
  sm->CanExecuteScripts(cx, principal, &enabled);
  return enabled;
}

NS_IMETHODIMP
nsDocumentEncoder::Init(nsIDocument* aDocument,
                        const nsAReadableString& aMimeType,
                        PRUint32 aFlags)
{
  if (!aDocument)
    return NS_ERROR_INVALID_ARG;

  mDocument = aDocument;

  mMimeType = aMimeType;

  mFlags = aFlags;

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetWrapColumn(PRUint32 aWC)
{
  mWrapColumn = aWC;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetSelection(nsISelection* aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetRange(nsIDOMRange* aRange)
{
  mRange = aRange;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetCharset(const nsAReadableString& aCharset)
{
  mCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::GetMimeType(nsAWritableString& aMimeType)
{
  aMimeType = mMimeType;
  return NS_OK;
}


PRBool
nsDocumentEncoder::IncludeInContext(nsIDOMNode *aNode)
{
  return PR_FALSE;
}

nsresult
nsDocumentEncoder::SerializeNodeStart(nsIDOMNode* aNode, PRInt32 aStartOffset,
                                      PRInt32 aEndOffset,
                                      nsAWritableString& aStr)
{
  PRUint16 type;

  nsCOMPtr<nsIDOMNode> node;
  if (mNodeFixup)
  {
    mNodeFixup->FixupNode(aNode, getter_AddRefs(node));
  }
  if (!node)
  {
    node = do_QueryInterface(aNode);
  }

  node->GetNodeType(&type);
  switch (type) {
    case nsIDOMNode::ELEMENT_NODE:
    {
      nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
      mSerializer->AppendElementStart(element, aStr);
      break;
    }
    case nsIDOMNode::TEXT_NODE:
    {
      nsCOMPtr<nsIDOMText> text = do_QueryInterface(node);
      mSerializer->AppendText(text, aStartOffset, aEndOffset, aStr);
      break;
    }
    case nsIDOMNode::CDATA_SECTION_NODE:
    {
      nsCOMPtr<nsIDOMCDATASection> cdata = do_QueryInterface(node);
      mSerializer->AppendCDATASection(cdata, aStartOffset, aEndOffset, aStr);
      break;
    }
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
    {
      nsCOMPtr<nsIDOMProcessingInstruction> pi = do_QueryInterface(node);
      mSerializer->AppendProcessingInstruction(pi, aStartOffset, aEndOffset,
                                               aStr);
      break;
    }
    case nsIDOMNode::COMMENT_NODE:
    {
      nsCOMPtr<nsIDOMComment> comment = do_QueryInterface(node);
      mSerializer->AppendComment(comment, aStartOffset, aEndOffset, aStr);
      break;
    }
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
    {
      nsCOMPtr<nsIDOMDocumentType> doctype = do_QueryInterface(node);
      mSerializer->AppendDoctype(doctype, aStr);
      break;
    }
  }
  
  return NS_OK;
}

nsresult
nsDocumentEncoder::SerializeNodeEnd(nsIDOMNode* aNode,
                                    nsAWritableString& aStr)
{
  PRUint16 type;

  aNode->GetNodeType(&type);
  switch (type) {
    case nsIDOMNode::ELEMENT_NODE:
    {
      nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
      mSerializer->AppendElementEnd(element, aStr);
      break;
    }
  }

  return NS_OK;
}

nsresult
nsDocumentEncoder::SerializeToStringRecursive(nsIDOMNode* aNode,
                                              nsAWritableString& aStr)
{
  nsresult rv = SerializeNodeStart(aNode, 0, -1, aStr);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasChildren = PR_FALSE;

  aNode->HasChildNodes(&hasChildren);

  if (hasChildren) {
    nsCOMPtr<nsIDOMNodeList> childNodes;
    rv = aNode->GetChildNodes(getter_AddRefs(childNodes));
    NS_ENSURE_TRUE(childNodes, NS_SUCCEEDED(rv) ? NS_ERROR_FAILURE : rv);

    PRInt32 index, count;

    childNodes->GetLength((PRUint32*)&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIDOMNode> child;

      rv = childNodes->Item(index, getter_AddRefs(child));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = SerializeToStringRecursive(child, aStr);
      NS_ENSURE_SUCCESS(rv, rv);     
    }
  }

  rv = SerializeNodeEnd(aNode, aStr);
  NS_ENSURE_SUCCESS(rv, rv);

  return FlushText(aStr, PR_FALSE);
}

PRBool 
nsDocumentEncoder::IsTag(nsIDOMNode* aNode, nsIAtom* aAtom)
{
  if (aNode)
  {
    nsCOMPtr<nsIAtom> atom;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    if (content)
      content->GetTag(*getter_AddRefs(atom));
    if (atom)
    {
      if (atom.get() == aAtom)
      {
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

static nsresult
ConvertAndWrite(nsAReadableString& aString,
                nsIOutputStream* aStream,
                nsIUnicodeEncoder* aEncoder)
{
  NS_ENSURE_ARG_POINTER(aStream);
  NS_ENSURE_ARG_POINTER(aEncoder);
  nsresult rv;
  PRInt32 charLength, startCharLength;
  const nsPromiseFlatString& flat = PromiseFlatString(aString);
  const PRUnichar* unicodeBuf = flat.get();
  PRInt32 unicodeLength = aString.Length();
  PRInt32 startLength = unicodeLength;

  rv = aEncoder->GetMaxLength(unicodeBuf, unicodeLength, &charLength);
  startCharLength = charLength;
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString charXferString;
  charXferString.SetCapacity(charLength);
  char* charXferBuf = (char*)charXferString.get();
  nsresult convert_rv = NS_OK;

  do {
    unicodeLength = startLength;
    charLength = startCharLength;

    convert_rv = aEncoder->Convert(unicodeBuf, &unicodeLength, charXferBuf, &charLength);
    NS_ENSURE_SUCCESS(convert_rv, convert_rv);
    
    PRUint32 written;
    rv = aStream->Write(charXferBuf, charLength, &written);
    NS_ENSURE_SUCCESS(rv, rv);

    // If the converter couldn't convert a chraacer we replace the
    // character with a characre entity.
    if (convert_rv == NS_ERROR_UENC_NOMAPPING) {
      // Finishes the conversion. 
      // The converter has the possibility to write some extra data and flush its final state.
      char finish_buf[32];
      charLength = 32;
      rv = aEncoder->Finish(finish_buf, &charLength);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aStream->Write(finish_buf, charLength, &written);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCAutoString entString("&#");
      entString.AppendInt(unicodeBuf[unicodeLength - 1]);
      entString.Append(';');

      rv = aStream->Write(entString.get(), entString.Length(), &written);
      NS_ENSURE_SUCCESS(rv, rv);

      unicodeBuf += unicodeLength;
      startLength -= unicodeLength;
    }
  } while (convert_rv == NS_ERROR_UENC_NOMAPPING);

  return rv;
}

nsresult
nsDocumentEncoder::FlushText(nsAWritableString& aString, PRBool aForce)
{
  if (!mStream)
    return NS_OK;

  nsresult rv = NS_OK;

  if (aString.Length() > 1024 || aForce) {
    rv = ConvertAndWrite(aString, mStream, mUnicodeEncoder);

    aString.Truncate();
  }

  return rv;
}

#if 0 // This code is really fast at serializing a range, but unfortunately
      // there are problems with it so we don't use it now, maybe later...
static nsresult ChildAt(nsIDOMNode* aNode, PRInt32 aIndex, nsIDOMNode*& aChild)
{
  nsCOMPtr<nsIContent> node(do_QueryInterface(aNode));
  nsCOMPtr<nsIContent> child;

  aChild = nsnull;

  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  node->ChildAt(aIndex, *getter_AddRefs(child));

  if (child)
    child->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)&aChild);

  return NS_OK;
}

static PRInt32 IndexOf(nsIDOMNode* aParent, nsIDOMNode* aChild)
{
  nsCOMPtr<nsIContent> parent(do_QueryInterface(aParent));
  nsCOMPtr<nsIContent> child(do_QueryInterface(aChild));

  if (!parent || !child)
    return -1;

  PRInt32 indx = 0;

  parent->IndexOf(child, indx);

  return indx;
}

static inline PRInt32 GetIndex(nsVoidArray& aIndexArray)
{
  PRInt32 count = aIndexArray.Count();

  if (count) {
    return (PRInt32)aIndexArray.ElementAt(count - 1);
  }

  return 0;
}

static nsresult GetNextNode(nsIDOMNode* aNode, nsVoidArray& aIndexArray,
                            nsIDOMNode*& aNextNode,
                            nsRangeIterationDirection& aDirection)
{
  PRBool hasChildren;

  aNextNode = nsnull;

  aNode->HasChildNodes(&hasChildren);

  if (hasChildren && aDirection == kDirectionIn) {
    ChildAt(aNode, 0, aNextNode);
    NS_ENSURE_TRUE(aNextNode, NS_ERROR_FAILURE);

    aIndexArray.AppendElement((void *)0);

    aDirection = kDirectionIn;
  } else if (aDirection == kDirectionIn) {
    aNextNode = aNode;

    NS_ADDREF(aNextNode);

    aDirection = kDirectionOut;
  } else {
    nsCOMPtr<nsIDOMNode> parent;

    aNode->GetParentNode(getter_AddRefs(parent));
    NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);

    PRInt32 count = aIndexArray.Count();

    if (count) {
      PRInt32 indx = (PRInt32)aIndexArray.ElementAt(count - 1);

      ChildAt(parent, indx + 1, aNextNode);

      if (aNextNode)
        aIndexArray.ReplaceElementAt((void *)(indx + 1), count - 1);
      else
        aIndexArray.RemoveElementAt(count - 1);
    } else {
      PRInt32 indx = IndexOf(parent, aNode);

      if (indx >= 0) {
        ChildAt(parent, indx + 1, aNextNode);

        if (aNextNode)
          aIndexArray.AppendElement((void *)(indx + 1));
      }
    }

    if (aNextNode) {
      aDirection = kDirectionIn;
    } else {
      aDirection = kDirectionOut;

      aNextNode = parent;

      NS_ADDREF(aNextNode);
    }
  }

  return NS_OK;
}
#endif

static PRBool IsTextNode(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  if (nodeType == nsIDOMNode::TEXT_NODE)
    return PR_TRUE;
  return PR_FALSE;
}

static nsresult GetLengthOfDOMNode(nsIDOMNode *aNode, PRUint32 &aCount) 
{
  aCount = 0;
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar = do_QueryInterface(aNode);
  if (nodeAsChar) {
    nodeAsChar->GetLength(&aCount);
  }
  else
  {
    PRBool hasChildNodes;
    aNode->HasChildNodes(&hasChildNodes);
    if (PR_TRUE==hasChildNodes)
    {
      nsCOMPtr<nsIDOMNodeList>nodeList;
      result = aNode->GetChildNodes(getter_AddRefs(nodeList));
      if (NS_SUCCEEDED(result) && nodeList) {
        nodeList->GetLength(&aCount);
      }
    }
  }
  return result;
}

nsresult
nsDocumentEncoder::SerializeRangeNodes(nsIDOMRange* aRange, 
                                       nsIDOMNode* aNode, 
                                       nsAWritableString& aString,
                                       PRInt32 aDepth)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsresult rv=NS_OK;
  
  // get start and end nodes for this recursion level
  nsCOMPtr<nsIContent> startNode, endNode;
  startNode = NS_STATIC_CAST(nsIContent *, mStartNodes[mStartRootIndex - aDepth]);
  endNode   = NS_STATIC_CAST(nsIContent *, mEndNodes[mEndRootIndex - aDepth]);
  
  if ((startNode != content) && (endNode != content))
  {
    // node is completely contained in range.  Serialize the whole subtree
    // rooted by this node.
    rv = SerializeToStringRecursive(aNode, aString);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
  {
    // due to implementation it is impossible for text node to be both start and end of 
    // range.  We would have handled that case without getting here.
    if (IsTextNode(aNode))
    {
      if (startNode == content)
      {
        PRInt32 startOffset;
        aRange->GetStartOffset(&startOffset);
        rv = SerializeNodeStart(aNode, startOffset, -1, aString);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else
      {
        PRInt32 endOffset;
        aRange->GetEndOffset(&endOffset);
        rv = SerializeNodeStart(aNode, 0, endOffset, aString);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    else
    {
      if (aNode != mCommonParent.get())
      {
        if (IncludeInContext(aNode))
        {
          // halt the incrementing of mStartDepth/mEndDepth.  This is
          // so paste client will include this node in paste.
          mHaltRangeHint = PR_TRUE;
        }
        if ((startNode == content) && !mHaltRangeHint) mStartDepth++;
        if ((endNode == content) && !mHaltRangeHint) mEndDepth++;
      
        // serialize the start of this node
        rv = SerializeNodeStart(aNode, 0, -1, aString);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      
      // do some calculations that will tell us which children of this node are in the range.
      nsCOMPtr<nsIContent> child;
      nsCOMPtr<nsIDOMNode> childAsNode;
      PRInt32 startOffset = 0, endOffset = -1;
      if (startNode == content)
        startOffset = NS_PTR_TO_INT32(mStartOffsets[mStartRootIndex - aDepth]);
      if (endNode == content)
        endOffset = NS_PTR_TO_INT32(mEndOffsets[mEndRootIndex - aDepth]) ;
      // generated content will cause offset values of -1 to be returned.  
      PRInt32 j, childCount=0;
      rv = content->ChildCount(childCount);
      NS_ENSURE_SUCCESS(rv, rv);
      if (startOffset == -1) startOffset = 0;
      if (endOffset == -1) endOffset = childCount;
      else
      {
        // if we are at the "tip" of the selection, endOffset is fine.
        // otherwise, we need to add one.  This is because of the semantics
        // of the offset list created by GetAncestorsAndOffsets().  The
        // intermediate points on the list use the endOffset of the 
        // location of the ancestor, rather than just past it.  So we need
        // to add one here in order to include it in the children we serialize.
        nsCOMPtr<nsIDOMNode> endParent;
        aRange->GetEndContainer(getter_AddRefs(endParent));
        if (aNode != endParent.get())
        {
          endOffset++;
        }
      }
      // serialize the children of this node that are in the range
      for (j=startOffset; j<endOffset; j++)
      {
        rv = content->ChildAt(j, *getter_AddRefs(child));
        childAsNode = do_QueryInterface(child);
        NS_ENSURE_SUCCESS(rv, rv);
        if ((j==startOffset) || (j==endOffset-1))
          rv = SerializeRangeNodes(aRange, childAsNode, aString, aDepth+1);
        else
          rv = SerializeToStringRecursive(childAsNode, aString);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // serialize the end of this node
      if (aNode != mCommonParent.get())
      {
        rv = SerializeNodeEnd(aNode, aString);
        NS_ENSURE_SUCCESS(rv, rv); 
      }
    }
  }
  return NS_OK;
}

nsresult
nsDocumentEncoder::SerializeRangeContextStart(const nsVoidArray& aAncestorArray,
                                              nsAWritableString& aString)
{
  PRInt32 i = aAncestorArray.Count();
  nsresult rv = NS_OK;

  while (i) {
    nsIDOMNode *node = (nsIDOMNode *)aAncestorArray.ElementAt(--i);

    if (!node)
      break;

    if (IncludeInContext(node)) {
      rv = SerializeNodeStart(node, 0, -1, aString);

      if (NS_FAILED(rv))
        break;
    }
  }

  return rv;
}

nsresult
nsDocumentEncoder::SerializeRangeContextEnd(const nsVoidArray& aAncestorArray,
                                            nsAWritableString& aString)
{
  PRInt32 i = 0;
  nsresult rv = NS_OK;

  while (1) {
    nsIDOMNode *node = (nsIDOMNode *)aAncestorArray.ElementAt(i++);

    if (!node)
      break;

    if (IncludeInContext(node)) {
      rv = SerializeNodeEnd(node, aString);

      if (NS_FAILED(rv))
        break;
    }
  }

  return rv;
}

nsresult
nsDocumentEncoder::SerializeRangeToString(nsIDOMRange *aRange,
                                          nsAWritableString& aOutputString)
{
  if (!aRange)
    return NS_OK;

  PRBool collapsed;

  aRange->GetCollapsed(&collapsed);

  if (collapsed)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> startParent, endParent;
  PRInt32 startOffset, endOffset;
  
  aRange->GetCommonAncestorContainer(getter_AddRefs(mCommonParent));

  if (!mCommonParent)
    return NS_OK;
  
  aRange->GetStartContainer(getter_AddRefs(startParent));
  NS_ENSURE_TRUE(startParent, NS_ERROR_FAILURE);
  aRange->GetStartOffset(&startOffset);

  aRange->GetEndContainer(getter_AddRefs(endParent));
  NS_ENSURE_TRUE(endParent, NS_ERROR_FAILURE);
  aRange->GetEndOffset(&endOffset);

  mCommonAncestors.Clear();
  mStartNodes.Clear();
  mStartOffsets.Clear();
  mEndNodes.Clear();
  mEndOffsets.Clear();

  nsRange::FillArrayWithAncestors(&mCommonAncestors, mCommonParent);
  nsRange::GetAncestorsAndOffsets(startParent, startOffset, &mStartNodes, &mStartOffsets);
  nsRange::GetAncestorsAndOffsets(endParent, endOffset, &mEndNodes, &mEndOffsets);
  nsCOMPtr<nsIContent> commonContent = do_QueryInterface(mCommonParent);
  mStartRootIndex = mStartNodes.IndexOf(commonContent);
  mEndRootIndex = mEndNodes.IndexOf(commonContent);
  
  nsresult rv = NS_OK;

  rv = SerializeRangeContextStart(mCommonAncestors, aOutputString);
  NS_ENSURE_SUCCESS(rv, rv);

  if ((startParent == endParent) && IsTextNode(startParent))
  {
    rv = SerializeNodeStart(startParent, startOffset, endOffset, aOutputString);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
  {
    rv = SerializeRangeNodes(aRange, mCommonParent, aOutputString, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = SerializeRangeContextEnd(mCommonAncestors, aOutputString);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

NS_IMETHODIMP
nsDocumentEncoder::EncodeToString(nsAWritableString& aOutputString)
{
  if (!mDocument)
    return NS_ERROR_NOT_INITIALIZED;

  aOutputString.Truncate();

  // xxx Also make sure mString is a mime type "text/html" or "text/plain"

  nsCAutoString progId(NS_CONTENTSERIALIZER_CONTRACTID_PREFIX);
  progId.AppendWithConversion(mMimeType);

  mSerializer = do_CreateInstance(NS_STATIC_CAST(const char *, progId));
  NS_ENSURE_TRUE(mSerializer, NS_ERROR_NOT_IMPLEMENTED);

  nsresult rv = NS_OK;

  nsCOMPtr<nsIAtom> charsetAtom;
  if (!mCharset.IsEmpty()) {
    if (!mCharsetConverterManager) {
      mCharsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = mCharsetConverterManager->GetCharsetAtom(mCharset.get(), getter_AddRefs(charsetAtom));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mSerializer->Init(mFlags, mWrapColumn, charsetAtom);

  if (mSelection) {
    nsCOMPtr<nsIDOMRange> range;
    PRInt32 i, count = 0;

    rv = mSelection->GetRangeCount(&count);
    NS_ENSURE_SUCCESS(rv, rv);

    for (i = 0; i < count; i++) {
      mSelection->GetRangeAt(i, getter_AddRefs(range));

      rv = SerializeRangeToString(range, aOutputString);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    mSelection = nsnull;
  } else if (mRange) {
      rv = SerializeRangeToString(mRange, aOutputString);

      mRange = nsnull;
  } else {
    nsCOMPtr<nsIDOMNode> doc(do_QueryInterface(mDocument));

    rv = SerializeToStringRecursive(doc, aOutputString);
  }

  NS_ENSURE_SUCCESS(rv, rv);
  rv = mSerializer->Flush(aOutputString);

  return rv;
}

NS_IMETHODIMP
nsDocumentEncoder::EncodeToStream(nsIOutputStream* aStream)
{
  nsresult rv = NS_OK;

  if (!mDocument)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mCharsetConverterManager) {
    mCharsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIAtom> charsetAtom;
  rv = mCharsetConverterManager->GetCharsetAtom(mCharset.get(), getter_AddRefs(charsetAtom));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mCharsetConverterManager->GetUnicodeEncoder(charsetAtom,
                                                   getter_AddRefs(mUnicodeEncoder));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mMimeType.EqualsIgnoreCase("text/plain")) {
    rv = mUnicodeEncoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, '?');
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // xxx Also make sure mString is a mime type "text/html" or "text/plain"

  mStream = aStream;

  nsAutoString buf;

  rv = EncodeToString(buf);

  // Force a flush of the last chunk of data.
  FlushText(buf, PR_TRUE);

  mStream = nsnull;
  mUnicodeEncoder = nsnull;

  return rv;
}

NS_IMETHODIMP
nsDocumentEncoder::EncodeToStringWithContext(nsAWritableString& aEncodedString, 
                                             nsAWritableString& aContextString,
                                             nsAWritableString& aInfoString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocumentEncoder::SetNodeFixup(nsIDocumentEncoderNodeFixup *aFixup)
{
  mNodeFixup = aFixup;
  return NS_OK;
}


nsresult NS_NewTextEncoder(nsIDocumentEncoder** aResult); // make mac compiler happy

nsresult
NS_NewTextEncoder(nsIDocumentEncoder** aResult)
{
  *aResult = new nsDocumentEncoder;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
 NS_ADDREF(*aResult);
 return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsDocumentEncoder declaration 
#pragma mark -
#endif

class nsHTMLCopyEncoder : public nsDocumentEncoder
{
public:

  nsHTMLCopyEncoder();
  virtual ~nsHTMLCopyEncoder();

  NS_IMETHOD Init(nsIDocument* aDocument, const nsAReadableString& aMimeType, PRUint32 aFlags);

  // overridden methods from nsDocumentEncoder
  NS_IMETHOD SetSelection(nsISelection* aSelection);
  NS_IMETHOD EncodeToStringWithContext(nsAWritableString& aEncodedString, 
                                       nsAWritableString& aContextString,
                                       nsAWritableString& aInfoString);

protected:

  enum Endpoint
  {
    kStart,
    kEnd
  };
  
  nsresult PromoteRange(nsIDOMRange *inRange);
  nsresult GetPromotedPoint(Endpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                                  nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset);
  nsCOMPtr<nsIDOMNode> GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset);
  PRBool IsMozBR(nsIDOMNode* aNode);
  nsresult GetNodeLocation(nsIDOMNode *inChild, nsCOMPtr<nsIDOMNode> *outParent, PRInt32 *outOffset);
  PRBool IsRoot(nsIDOMNode* aNode);
  PRBool IsFirstNode(nsIDOMNode *aNode);
  PRBool IsLastNode(nsIDOMNode *aNode);
  PRBool IsEmptyTextContent(nsIDOMNode* aNode);
  virtual PRBool IncludeInContext(nsIDOMNode *aNode);

  nsCOMPtr<nsIParserService> mParserService;
  PRBool mIsTextWidget;
};

#ifdef XP_MAC
#pragma mark  nsDocumentEncoder implementation 
#pragma mark -
#endif

nsHTMLCopyEncoder::nsHTMLCopyEncoder()
{
  mIsTextWidget = PR_FALSE;
}

nsHTMLCopyEncoder::~nsHTMLCopyEncoder()
{
}

NS_IMETHODIMP
nsHTMLCopyEncoder::Init(nsIDocument* aDocument,
                        const nsAReadableString& aMimetype,
                        PRUint32 aFlags)
{
  if (!aDocument)
    return NS_ERROR_INVALID_ARG;

  mDocument = aDocument;

  mMimeType = NS_LITERAL_STRING("text/html");
  
  // Make all links absolute when copying
  // (see related bugs #57296, #41924, #58646, #32768)
  mFlags = aFlags | OutputAbsoluteLinks;

  if (!IsScriptEnabled(mDocument))
    mFlags |= OutputNoScriptContent;

  nsresult rv;
  mParserService = do_GetService(kParserServiceCID, &rv);
  return rv;
}

NS_IMETHODIMP
nsHTMLCopyEncoder::SetSelection(nsISelection* aSelection)
{
  // check for text widgets: we need to recognize these so that
  // we don't tweak the selection to be outside of the magic
  // div that ender-lite text widgets are embedded in.
  
  if (!aSelection) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMRange> range;
  nsCOMPtr<nsIDOMNode> commonParent;
  PRInt32 count = 0;

  nsresult rv = aSelection->GetRangeCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  // if selection is uninitialized return
  if (!count)
    return NS_ERROR_FAILURE;
  
  // we'll just use the common parent of the first range.  Implicit assumption
  // here that multi-range selections are table cell selections, in which case
  // the common parent is somewhere in the table and we don't really care where.
  rv = aSelection->GetRangeAt(0, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!range)
    return NS_ERROR_NULL_POINTER;
  range->GetCommonAncestorContainer(getter_AddRefs(commonParent));

  nsCOMPtr<nsIContent> tmp, selContent( do_QueryInterface(commonParent) );
  while (selContent)
  {
    // checking for selection inside a plaintext form widget
    nsCOMPtr<nsIAtom> atom;
    selContent->GetTag(*getter_AddRefs(atom));
    if (atom.get() == nsHTMLAtoms::input ||
        atom.get() == nsHTMLAtoms::textarea)
    {
      mIsTextWidget = PR_TRUE;
      break;
    }
    else if (atom.get() == nsHTMLAtoms::body)
    {
      // check for moz prewrap style on body.  If it's there we are 
      // in a plaintext editor.  This is pretty cheezy but I haven't 
      // found a good way to tell if we are in a plaintext editor.
      nsCOMPtr<nsIDOMElement> bodyElem = do_QueryInterface(selContent);
      nsAutoString wsVal;
      rv = bodyElem->GetAttribute(NS_LITERAL_STRING("style"), wsVal);
      if (NS_SUCCEEDED(rv) && (kNotFound != wsVal.Find(NS_LITERAL_STRING("-moz-pre-wrap").get())))
      {
        mIsTextWidget = PR_TRUE;
        break;
      }
    }
    selContent->GetParent(*getter_AddRefs(tmp));
    selContent = tmp;
  }
  
  // also consider ourselves in a text widget if we can't find an html document
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
  if (!htmlDoc) mIsTextWidget = PR_TRUE;
  
  // normalize selection if we are not in a widget
  if (mIsTextWidget) 
  {
    mSelection = aSelection;
    mMimeType = NS_LITERAL_STRING("text/plain");
    return NS_OK;
  }
  
  // there's no Clone() for selection! fix...
  //nsresult rv = aSelection->Clone(getter_AddRefs(mSelection);
  //NS_ENSURE_SUCCESS(rv, rv);
  NS_NewDomSelection(getter_AddRefs(mSelection));
  NS_ENSURE_TRUE(mSelection, NS_ERROR_FAILURE);
  nsCOMPtr<nsISelectionPrivate> privSelection( do_QueryInterface(aSelection) );
  NS_ENSURE_TRUE(privSelection, NS_ERROR_FAILURE);
  
  // get selection range enumerator
  nsCOMPtr<nsIEnumerator> enumerator;
  rv = privSelection->GetEnumerator(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(enumerator, NS_ERROR_FAILURE);

  // loop thru the ranges in the selection
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
  {
    rv = enumerator->CurrentItem(getter_AddRefs(currentItem));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(currentItem, NS_ERROR_FAILURE);
    
    range = do_QueryInterface(currentItem);
    NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMRange> myRange;
    range->CloneRange(getter_AddRefs(myRange));
    NS_ENSURE_TRUE(myRange, NS_ERROR_FAILURE);

    // adjust range to include any ancestors who's children are entirely selected
    rv = PromoteRange(myRange);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = mSelection->AddRange(myRange);
    NS_ENSURE_SUCCESS(rv, rv);

    enumerator->Next();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLCopyEncoder::EncodeToStringWithContext(nsAWritableString& aEncodedString, 
                                             nsAWritableString& aContextString,
                                             nsAWritableString& aInfoString)
{
  nsresult rv = EncodeToString(aEncodedString);
  NS_ENSURE_SUCCESS(rv, rv);

  // do not encode any context info or range hints if we are in a text widget.
  if (mIsTextWidget) return NS_OK;

  // now encode common ancestors into aContextString.  Note that the common ancestors
  // will be for the last range in the selection in the case of multirange selections.
  // encoding ancestors every range in a multirange selection in a way that could be 
  // understood by the paste code would be a lot more work to do.  As a practical matter,
  // selections are single range, and the ones that aren't are table cell selections
  // where all the cells are in the same table.

  // leaf of ancestors might be text node.  If so discard it.
  nsCOMPtr<nsIDOMNode> node;
  node = NS_STATIC_CAST(nsIDOMNode *, mCommonAncestors.ElementAt(0));
  if (node && IsTextNode(node)) 
  {
    mCommonAncestors.RemoveElementAt(0);
    // don't forget to adjust range depth info
    if (mStartDepth) mStartDepth--;
    if (mEndDepth) mEndDepth--;
  }
  
  PRInt32 i = mCommonAncestors.Count();

  node = NS_STATIC_CAST(nsIDOMNode *, mCommonAncestors.ElementAt(--i));

  while (node) 
  {
    SerializeNodeStart(node, 0, -1, aContextString);
    node = NS_STATIC_CAST(nsIDOMNode *, mCommonAncestors.ElementAt(--i));
  }

  i = 0;
  node = NS_STATIC_CAST(nsIDOMNode *, mCommonAncestors.ElementAt(i));

  while (node) 
  {
    SerializeNodeEnd(node, aContextString);
    node = NS_STATIC_CAST(nsIDOMNode *, mCommonAncestors.ElementAt(++i));
  }

  // encode range info : the start and end depth of the selection, where the depth is 
  // distance down in the parent heirarchy.  Later we will need to add leading/trainlig
  // whitespace info to this.
  nsAutoString infoString;
  infoString.AppendInt(mStartDepth);
  infoString.AppendWithConversion(',');
  infoString.AppendInt(mEndDepth);
  aInfoString = infoString;
  
  return NS_OK;
}


PRBool
nsHTMLCopyEncoder::IncludeInContext(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

  if (!content)
    return PR_FALSE;

  nsCOMPtr<nsIAtom> tag;

  content->GetTag(*getter_AddRefs(tag));

  if (tag.get() == nsHTMLAtoms::b        ||
      tag.get() == nsHTMLAtoms::i        ||
      tag.get() == nsHTMLAtoms::u        ||
      tag.get() == nsHTMLAtoms::tt       ||
      tag.get() == nsHTMLAtoms::s        ||
      tag.get() == nsHTMLAtoms::strike   ||
      tag.get() == nsHTMLAtoms::em       ||
      tag.get() == nsHTMLAtoms::strong   ||
      tag.get() == nsHTMLAtoms::dfn      ||
      tag.get() == nsHTMLAtoms::code     ||
      tag.get() == nsHTMLAtoms::cite     ||
      tag.get() == nsHTMLAtoms::variable ||
      tag.get() == nsHTMLAtoms::abbr     ||
      tag.get() == nsHTMLAtoms::font     ||
      tag.get() == nsHTMLAtoms::script   ||
      tag.get() == nsHTMLAtoms::span     ||
      tag.get() == nsHTMLAtoms::pre      ||
      tag.get() == nsHTMLAtoms::h1       ||
      tag.get() == nsHTMLAtoms::h2       ||
      tag.get() == nsHTMLAtoms::h3       ||
      tag.get() == nsHTMLAtoms::h4       ||
      tag.get() == nsHTMLAtoms::h5       ||
      tag.get() == nsHTMLAtoms::h6) {
    return PR_TRUE;
  }

  return PR_FALSE;
}


nsresult 
nsHTMLCopyEncoder::PromoteRange(nsIDOMRange *inRange)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult rv;
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  
  rv = inRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = inRange->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = inRange->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = inRange->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMNode> opStartNode;
  nsCOMPtr<nsIDOMNode> opEndNode;
  PRInt32 opStartOffset, opEndOffset;
  nsCOMPtr<nsIDOMRange> opRange;
  
  rv = GetPromotedPoint( kStart, startNode, startOffset, address_of(opStartNode), &opStartOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GetPromotedPoint( kEnd, endNode, endOffset, address_of(opEndNode), &opEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = inRange->SetStart(opStartNode, opStartOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = inRange->SetEnd(opEndNode, opEndOffset);
  return rv;
} 

nsresult
nsHTMLCopyEncoder::GetPromotedPoint(Endpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                                  nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMNode> parent = aNode;
  PRInt32 offset = aOffset;
  PRBool  bResetPromotion = PR_FALSE;
  
  // defualt values
  *outNode = node;
  *outOffset = offset;

  if (aWhere == kStart)
  {
    // some special casing for text nodes
    if (IsTextNode(aNode))  
    {
      // if not at beginning of text node, we are done
      if (offset >  0) 
      {
        // unless everything before us in just whitespace.  NOTE: we need a more
        // general solution that truly detects all cases of non-significant
        // whitesace with no false alarms.
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(aNode);
        nsAutoString text;
        nodeAsText->SubstringData(0, offset, text);
        text.CompressWhitespace();
        if (text.Length())
          return NS_OK;
        bResetPromotion = PR_TRUE;
      }
      // else
      rv = GetNodeLocation(aNode, address_of(parent), &offset);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
      node = GetChildAt(parent,offset);
    }
    if (!node) node = parent;

    // finding the real start for this point.  look up the tree for as long as we are the 
    // first node in the container, and as long as we haven't hit the body node.
    if (!IsRoot(node))
    {
      rv = GetNodeLocation(node, address_of(parent), &offset);
      NS_ENSURE_SUCCESS(rv, rv);
      if (offset == -1) return NS_OK; // we hit generated content; STOP
      while ((IsFirstNode(node)) && (!IsRoot(parent)))
      {
        if (bResetPromotion)
        {
          nsAutoString tag;
          nsCOMPtr<nsIAtom> atom;
          nsCOMPtr<nsIContent> content = do_QueryInterface(parent);
          if (content)
          {
            PRBool isBlock = PR_FALSE;
            PRInt32 id;
            content->GetTag(*getter_AddRefs(atom));
            atom->ToString(tag);
            mParserService->HTMLStringTagToId(tag, &id);
            mParserService->IsBlock(id, isBlock);
            if (isBlock)
            {
              bResetPromotion = PR_FALSE;
            }
          }   
        }
         
        node = parent;
        rv = GetNodeLocation(node, address_of(parent), &offset);
        NS_ENSURE_SUCCESS(rv, rv);
        if (offset == -1)  // we hit generated content; STOP
        {
          // back up a bit
          parent = node;
          offset = 0;
          break;
        }
      } 
      if (bResetPromotion)
      {
        *outNode = aNode;
        *outOffset = aOffset;
      }
      else
      {
        *outNode = parent;
        *outOffset = offset;
      }
      return rv;
    }
  }
  
  if (aWhere == kEnd)
  {
    // some special casing for text nodes
    if (IsTextNode(aNode))  
    {
      // if not at end of text node, we are done
      PRUint32 len;
      GetLengthOfDOMNode(aNode, len);
      if (offset < (PRInt32)len)
      {
        // unless everything after us in just whitespace.  NOTE: we need a more
        // general solution that truly detects all cases of non-significant
        // whitesace with no false alarms.
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(aNode);
        nsAutoString text;
        nodeAsText->SubstringData(offset, len-offset, text);
        text.CompressWhitespace();
        if (text.Length())
          return NS_OK;
        bResetPromotion = PR_TRUE;
      }
      rv = GetNodeLocation(aNode, address_of(parent), &offset);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
      if (offset) offset--; // we want node _before_ offset
      node = GetChildAt(parent,offset);
    }
    if (!node) node = parent;
    
    // finding the real end for this point.  look up the tree for as long as we are the 
    // last node in the container, and as long as we haven't hit the body node.
    if (!IsRoot(node))
    {
      rv = GetNodeLocation(node, address_of(parent), &offset);
      NS_ENSURE_SUCCESS(rv, rv);
      if (offset == -1) return NS_OK; // we hit generated content; STOP
      while ((IsLastNode(node)) && (!IsRoot(parent)))
      {
        if (bResetPromotion)
        {
          nsAutoString tag;
          nsCOMPtr<nsIAtom> atom;
          nsCOMPtr<nsIContent> content = do_QueryInterface(parent);
          if (content)
          {
            PRBool isBlock = PR_FALSE;
            PRInt32 id;
            content->GetTag(*getter_AddRefs(atom));
            atom->ToString(tag);
            mParserService->HTMLStringTagToId(tag, &id);
            mParserService->IsBlock(id, isBlock);
            if (isBlock)
            {
              bResetPromotion = PR_FALSE;
            }
          }   
        }
          
        node = parent;
        rv = GetNodeLocation(node, address_of(parent), &offset);
        NS_ENSURE_SUCCESS(rv, rv);
        if (offset == -1)  // we hit generated content; STOP
        {
          // back up a bit
          parent = node;
          offset = 0;
          break;
        }
      } 
      if (bResetPromotion)
      {
        *outNode = aNode;
        *outOffset = aOffset;
      }
      else
      {
        *outNode = parent;
        offset++;  // add one since this in an endpoint - want to be AFTER node.
        *outOffset = offset;
      }
      return rv;
    }
  }
  
  return rv;
}

nsCOMPtr<nsIDOMNode> 
nsHTMLCopyEncoder::GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;
  
  if (!aParent) 
    return resultNode;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aParent);
  nsCOMPtr<nsIContent> cChild;
  NS_PRECONDITION(content, "null content in nsHTMLCopyEncoder::GetChildAt");
  
  if (NS_FAILED(content->ChildAt(aOffset, *getter_AddRefs(cChild))))
    return resultNode;
  
  resultNode = do_QueryInterface(cChild);
  return resultNode;
}

PRBool 
nsHTMLCopyEncoder::IsMozBR(nsIDOMNode* aNode)
{
  if (IsTag(aNode, nsHTMLAtoms::br))
  {
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
    if (elem)
    {
      nsAutoString typeAttrName; typeAttrName.AssignWithConversion("type");
      nsAutoString typeAttrVal;
      nsresult rv = elem->GetAttribute(typeAttrName, typeAttrVal);
      typeAttrVal.ToLowerCase();
      if (NS_SUCCEEDED(rv) && (typeAttrVal.EqualsWithConversion("_moz")))
        return PR_TRUE;
    }
    return PR_FALSE;
  }
  return PR_FALSE;
}

nsresult 
nsHTMLCopyEncoder::GetNodeLocation(nsIDOMNode *inChild, nsCOMPtr<nsIDOMNode> *outParent, PRInt32 *outOffset)
{
  NS_ASSERTION((inChild && outParent && outOffset), "bad args");
  nsresult result = NS_ERROR_NULL_POINTER;
  if (inChild && outParent && outOffset)
  {
    result = inChild->GetParentNode(getter_AddRefs(*outParent));
    if ((NS_SUCCEEDED(result)) && (*outParent))
    {
      nsCOMPtr<nsIContent> content = do_QueryInterface(*outParent);
      nsCOMPtr<nsIContent> cChild = do_QueryInterface(inChild);
      if (!cChild || !content) return NS_ERROR_NULL_POINTER;
      result = content->IndexOf(cChild, *outOffset);
    }
  }
  return result;
}

PRBool
nsHTMLCopyEncoder::IsRoot(nsIDOMNode* aNode)
{
  if (aNode)
  {
    if (mIsTextWidget) 
      return (IsTag(aNode, nsHTMLAtoms::div));
    else
      return (IsTag(aNode, nsHTMLAtoms::body) || 
              IsTag(aNode, nsHTMLAtoms::td)   ||
              IsTag(aNode, nsHTMLAtoms::th));
  }
  return PR_FALSE;
}

PRBool
nsHTMLCopyEncoder::IsFirstNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset, j=0;
  nsresult rv = GetNodeLocation(aNode, address_of(parent), &offset);
  if (NS_FAILED(rv)) 
  {
    NS_NOTREACHED("failure in IsFirstNode");
    return PR_FALSE;
  }
  if (offset == 0)  // easy case, we are first dom child
    return PR_TRUE;
  if (!parent)  
    return PR_TRUE;
  
  // need to check if any nodes before us are really visible.
  // Mike wrote something for me along these lines in nsSelectionController,
  // but I don't think it's ready for use yet - revisit.
  // HACK: for now, simply consider all whitespace text nodes to be 
  // invisible formatting nodes.
  nsCOMPtr<nsIDOMNodeList> childList;
  nsCOMPtr<nsIDOMNode> child;

  rv = parent->GetChildNodes(getter_AddRefs(childList));
  if (NS_FAILED(rv) || !childList) 
  {
    NS_NOTREACHED("failure in IsFirstNode");
    return PR_TRUE;
  }
  while (j < offset)
  {
    childList->Item(j, getter_AddRefs(child));
    if (!IsEmptyTextContent(child)) 
      return PR_FALSE;
    j++;
  }
  return PR_TRUE;
}


PRBool
nsHTMLCopyEncoder::IsLastNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset,j;
  PRUint32 numChildren;
  nsresult rv = GetNodeLocation(aNode, address_of(parent), &offset);
  if (NS_FAILED(rv)) 
  {
    NS_NOTREACHED("failure in IsLastNode");
    return PR_FALSE;
  }
  GetLengthOfDOMNode(parent, numChildren); 
  if (offset+1 == (PRInt32)numChildren) // easy case, we are last dom child
    return PR_TRUE;
  if (!parent)
    return PR_TRUE;
  // need to check if any nodes after us are really visible.
  // Mike wrote something for me along these lines in nsSelectionController,
  // but I don't think it's ready for use yet - revisit.
  // HACK: for now, simply consider all whitespace text nodes to be 
  // invisible formatting nodes.
  j = (PRInt32)numChildren-1;
  nsCOMPtr<nsIDOMNodeList>childList;
  nsCOMPtr<nsIDOMNode> child;
  rv = parent->GetChildNodes(getter_AddRefs(childList));
  if (NS_FAILED(rv) || !childList) 
  {
    NS_NOTREACHED("failure in IsLastNode");
    return PR_TRUE;
  }
  while (j > offset)
  {
    childList->Item(j, getter_AddRefs(child));
    j--;
    if (IsMozBR(child))  // we ignore trailing moz BRs.  
      continue;
    if (!IsEmptyTextContent(child)) 
      return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsHTMLCopyEncoder::IsEmptyTextContent(nsIDOMNode* aNode)
{
  PRBool result = PR_FALSE;
  nsCOMPtr<nsITextContent> tc(do_QueryInterface(aNode));
  if (tc) {
    tc->IsOnlyWhitespace(&result);
  }
  return result;
}

nsresult NS_NewHTMLCopyTextEncoder(nsIDocumentEncoder** aResult); // make mac compiler happy

nsresult
NS_NewHTMLCopyTextEncoder(nsIDocumentEncoder** aResult)
{
  *aResult = new nsHTMLCopyEncoder;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
 NS_ADDREF(*aResult);
 return NS_OK;
}
