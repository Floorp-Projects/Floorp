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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mats Palmgren <matpal@gmail.com>
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

/*
 * Object that can be used to serialize selections, ranges, or nodes
 * to strings in a gazillion different ways.
 */
 
#include "nsIDocumentEncoder.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h" 
#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
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
#include "nsIRange.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsICharsetConverterManager.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIEnumerator.h"
#include "nsIParserService.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsITransferable.h" // for kUnicodeMime
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsIFrame.h"
#include "nsStringBuffer.h"
#include "mozilla/dom/Element.h"

nsresult NS_NewDomSelection(nsISelection **aDomSelection);

enum nsRangeIterationDirection {
  kDirectionOut = -1,
  kDirectionIn = 1
};

class nsDocumentEncoder : public nsIDocumentEncoder
{
public:
  nsDocumentEncoder();
  virtual ~nsDocumentEncoder();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDocumentEncoder)
  NS_DECL_NSIDOCUMENTENCODER

protected:
  void Initialize(bool aClearCachedSerializer = true);
  nsresult SerializeNodeStart(nsINode* aNode, PRInt32 aStartOffset,
                              PRInt32 aEndOffset, nsAString& aStr,
                              nsINode* aOriginalNode = nsnull);
  nsresult SerializeToStringRecursive(nsINode* aNode,
                                      nsAString& aStr,
                                      bool aDontSerializeRoot);
  nsresult SerializeNodeEnd(nsINode* aNode, nsAString& aStr);
  // This serializes the content of aNode.
  nsresult SerializeToStringIterative(nsINode* aNode,
                                      nsAString& aStr);
  nsresult SerializeRangeToString(nsIRange *aRange,
                                  nsAString& aOutputString);
  nsresult SerializeRangeNodes(nsIRange* aRange, 
                               nsINode* aNode, 
                               nsAString& aString,
                               PRInt32 aDepth);
  nsresult SerializeRangeContextStart(const nsTArray<nsINode*>& aAncestorArray,
                                      nsAString& aString);
  nsresult SerializeRangeContextEnd(const nsTArray<nsINode*>& aAncestorArray,
                                    nsAString& aString);

  nsresult FlushText(nsAString& aString, bool aForce);

  bool IsVisibleNode(nsINode* aNode)
  {
    NS_PRECONDITION(aNode, "");

    if (mFlags & SkipInvisibleContent) {
      nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
      if (content) {
        nsIFrame* frame = content->GetPrimaryFrame();
        if (!frame) {
          if (aNode->IsNodeOfType(nsINode::eTEXT)) {
            // We have already checked that our parent is visible.
            return PR_TRUE;
          }
          return PR_FALSE;
        }
        bool isVisible = frame->GetStyleVisibility()->IsVisible();
        if (!isVisible && aNode->IsNodeOfType(nsINode::eTEXT))
          return PR_FALSE;
      }
    }
    return PR_TRUE;
  }

  static bool IsTag(nsIContent* aContent, nsIAtom* aAtom);
  
  virtual bool IncludeInContext(nsINode *aNode);

  nsCOMPtr<nsIDocument>          mDocument;
  nsCOMPtr<nsISelection>         mSelection;
  nsCOMPtr<nsIRange>             mRange;
  nsCOMPtr<nsINode>              mNode;
  nsCOMPtr<nsIOutputStream>      mStream;
  nsCOMPtr<nsIContentSerializer> mSerializer;
  nsCOMPtr<nsIUnicodeEncoder>    mUnicodeEncoder;
  nsCOMPtr<nsINode>              mCommonParent;
  nsCOMPtr<nsIDocumentEncoderNodeFixup> mNodeFixup;
  nsCOMPtr<nsICharsetConverterManager> mCharsetConverterManager;

  nsString          mMimeType;
  nsCString         mCharset;
  PRUint32          mFlags;
  PRUint32          mWrapColumn;
  PRUint32          mStartDepth;
  PRUint32          mEndDepth;
  PRInt32           mStartRootIndex;
  PRInt32           mEndRootIndex;
  nsAutoTArray<nsINode*, 8>    mCommonAncestors;
  nsAutoTArray<nsIContent*, 8> mStartNodes;
  nsAutoTArray<PRInt32, 8>     mStartOffsets;
  nsAutoTArray<nsIContent*, 8> mEndNodes;
  nsAutoTArray<PRInt32, 8>     mEndOffsets;
  bool              mHaltRangeHint;  
  bool              mIsCopying;  // Set to true only while copying
  bool              mNodeIsContainer;
  nsStringBuffer*   mCachedBuffer;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDocumentEncoder)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDocumentEncoder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDocumentEncoder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDocumentEncoder)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentEncoder)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDocumentEncoder)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSelection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCommonParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDocumentEncoder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSelection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCommonParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsDocumentEncoder::nsDocumentEncoder() : mCachedBuffer(nsnull)
{
  Initialize();
  mMimeType.AssignLiteral("text/plain");

}

void nsDocumentEncoder::Initialize(bool aClearCachedSerializer)
{
  mFlags = 0;
  mWrapColumn = 72;
  mStartDepth = 0;
  mEndDepth = 0;
  mStartRootIndex = 0;
  mEndRootIndex = 0;
  mHaltRangeHint = PR_FALSE;
  mNodeIsContainer = PR_FALSE;
  if (aClearCachedSerializer) {
    mSerializer = nsnull;
  }
}

nsDocumentEncoder::~nsDocumentEncoder()
{
  if (mCachedBuffer) {
    mCachedBuffer->Release();
  }
}

NS_IMETHODIMP
nsDocumentEncoder::Init(nsIDOMDocument* aDocument,
                        const nsAString& aMimeType,
                        PRUint32 aFlags)
{
  if (!aDocument)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  return NativeInit(doc, aMimeType, aFlags);
}

NS_IMETHODIMP
nsDocumentEncoder::NativeInit(nsIDocument* aDocument,
                              const nsAString& aMimeType,
                              PRUint32 aFlags)
{
  if (!aDocument)
    return NS_ERROR_INVALID_ARG;

  Initialize(!mMimeType.Equals(aMimeType));

  mDocument = aDocument;

  mMimeType = aMimeType;

  mFlags = aFlags;
  mIsCopying = PR_FALSE;

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
  mRange = do_QueryInterface(aRange);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetNode(nsIDOMNode* aNode)
{
  mNodeIsContainer = PR_FALSE;
  mNode = do_QueryInterface(aNode);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetContainerNode(nsIDOMNode *aContainer)
{
  mNodeIsContainer = PR_TRUE;
  mNode = do_QueryInterface(aContainer);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetNativeContainerNode(nsINode* aContainer)
{
  mNodeIsContainer = PR_TRUE;
  mNode = aContainer;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetCharset(const nsACString& aCharset)
{
  mCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::GetMimeType(nsAString& aMimeType)
{
  aMimeType = mMimeType;
  return NS_OK;
}


bool
nsDocumentEncoder::IncludeInContext(nsINode *aNode)
{
  return PR_FALSE;
}

nsresult
nsDocumentEncoder::SerializeNodeStart(nsINode* aNode,
                                      PRInt32 aStartOffset,
                                      PRInt32 aEndOffset,
                                      nsAString& aStr,
                                      nsINode* aOriginalNode)
{
  if (!IsVisibleNode(aNode))
    return NS_OK;
  
  nsINode* node = nsnull;
  nsCOMPtr<nsINode> fixedNodeKungfuDeathGrip;

  // Caller didn't do fixup, so we'll do it ourselves
  if (!aOriginalNode) {
    aOriginalNode = aNode;
    if (mNodeFixup) { 
      bool dummy;
      nsCOMPtr<nsIDOMNode> domNodeIn = do_QueryInterface(aNode);
      nsCOMPtr<nsIDOMNode> domNodeOut;
      mNodeFixup->FixupNode(domNodeIn, &dummy, getter_AddRefs(domNodeOut));
      fixedNodeKungfuDeathGrip = do_QueryInterface(domNodeOut);
      node = fixedNodeKungfuDeathGrip;
    }
  }

  // Either there was no fixed-up node,
  // or the caller did fixup themselves and aNode is already fixed
  if (!node)
    node = aNode;
  
  if (node->IsElement()) {
    mozilla::dom::Element* originalElement =
      aOriginalNode && aOriginalNode->IsElement() ?
        aOriginalNode->AsElement() : nsnull;
    mSerializer->AppendElementStart(node->AsElement(),
                                    originalElement, aStr);
    return NS_OK;
  }

  switch (node->NodeType()) {
    case nsIDOMNode::TEXT_NODE:
    {
      mSerializer->AppendText(static_cast<nsIContent*>(node),
                              aStartOffset, aEndOffset, aStr);
      break;
    }
    case nsIDOMNode::CDATA_SECTION_NODE:
    {
      mSerializer->AppendCDATASection(static_cast<nsIContent*>(node),
                                      aStartOffset, aEndOffset, aStr);
      break;
    }
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
    {
      mSerializer->AppendProcessingInstruction(static_cast<nsIContent*>(node),
                                               aStartOffset, aEndOffset, aStr);
      break;
    }
    case nsIDOMNode::COMMENT_NODE:
    {
      mSerializer->AppendComment(static_cast<nsIContent*>(node),
                                 aStartOffset, aEndOffset, aStr);
      break;
    }
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
    {
      mSerializer->AppendDoctype(static_cast<nsIContent*>(node), aStr);
      break;
    }
  }

  return NS_OK;
}

nsresult
nsDocumentEncoder::SerializeNodeEnd(nsINode* aNode,
                                    nsAString& aStr)
{
  if (!IsVisibleNode(aNode))
    return NS_OK;

  if (aNode->IsElement()) {
    mSerializer->AppendElementEnd(aNode->AsElement(), aStr);
  }
  return NS_OK;
}

nsresult
nsDocumentEncoder::SerializeToStringRecursive(nsINode* aNode,
                                              nsAString& aStr,
                                              bool aDontSerializeRoot)
{
  if (!IsVisibleNode(aNode))
    return NS_OK;

  nsresult rv = NS_OK;
  bool serializeClonedChildren = false;
  nsINode* maybeFixedNode = nsnull;

  // Keep the node from FixupNode alive.
  nsCOMPtr<nsINode> fixedNodeKungfuDeathGrip;
  if (mNodeFixup) {
    nsCOMPtr<nsIDOMNode> domNodeIn = do_QueryInterface(aNode);
    nsCOMPtr<nsIDOMNode> domNodeOut;
    mNodeFixup->FixupNode(domNodeIn, &serializeClonedChildren, getter_AddRefs(domNodeOut));
    fixedNodeKungfuDeathGrip = do_QueryInterface(domNodeOut);
    maybeFixedNode = fixedNodeKungfuDeathGrip;
  }

  if (!maybeFixedNode)
    maybeFixedNode = aNode;

  if (mFlags & SkipInvisibleContent) {
    if (aNode->IsNodeOfType(nsINode::eCONTENT)) {
      nsIFrame* frame = static_cast<nsIContent*>(aNode)->GetPrimaryFrame();
      if (frame) {
        bool isSelectable;
        frame->IsSelectable(&isSelectable, nsnull);
        if (!isSelectable){
          aDontSerializeRoot = PR_TRUE;
        }
      }
    }
  }

  if (!aDontSerializeRoot) {
    rv = SerializeNodeStart(maybeFixedNode, 0, -1, aStr, aNode);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsINode* node = serializeClonedChildren ? maybeFixedNode : aNode;

  for (nsINode* child = node->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    rv = SerializeToStringRecursive(child, aStr, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aDontSerializeRoot) {
    rv = SerializeNodeEnd(node, aStr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return FlushText(aStr, PR_FALSE);
}

nsresult
nsDocumentEncoder::SerializeToStringIterative(nsINode* aNode,
                                              nsAString& aStr)
{
  nsresult rv;

  nsINode* node = aNode->GetFirstChild();
  while (node) {
    nsINode* current = node;
    rv = SerializeNodeStart(current, 0, -1, aStr, current);
    NS_ENSURE_SUCCESS(rv, rv);
    node = current->GetFirstChild();
    while (!node && current && current != aNode) {
      rv = SerializeNodeEnd(current, aStr);
      NS_ENSURE_SUCCESS(rv, rv);
      // Check if we have siblings.
      node = current->GetNextSibling();
      if (!node) {
        // Perhaps parent node has siblings.
        current = current->GetNodeParent();
      }
    }
  }

  return NS_OK;
}

bool 
nsDocumentEncoder::IsTag(nsIContent* aContent, nsIAtom* aAtom)
{
  return aContent && aContent->Tag() == aAtom;
}

static nsresult
ConvertAndWrite(const nsAString& aString,
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
  if (!EnsureStringLength(charXferString, charLength))
    return NS_ERROR_OUT_OF_MEMORY;

  char* charXferBuf = charXferString.BeginWriting();
  nsresult convert_rv = NS_OK;

  do {
    unicodeLength = startLength;
    charLength = startCharLength;

    convert_rv = aEncoder->Convert(unicodeBuf, &unicodeLength, charXferBuf, &charLength);
    NS_ENSURE_SUCCESS(convert_rv, convert_rv);

    // Make sure charXferBuf is null-terminated before we call
    // Write().

    charXferBuf[charLength] = '\0';

    PRUint32 written;
    rv = aStream->Write(charXferBuf, charLength, &written);
    NS_ENSURE_SUCCESS(rv, rv);

    // If the converter couldn't convert a chraacer we replace the
    // character with a characre entity.
    if (convert_rv == NS_ERROR_UENC_NOMAPPING) {
      // Finishes the conversion. 
      // The converter has the possibility to write some extra data and flush its final state.
      char finish_buf[33];
      charLength = sizeof(finish_buf) - 1;
      rv = aEncoder->Finish(finish_buf, &charLength);
      NS_ENSURE_SUCCESS(rv, rv);

      // Make sure finish_buf is null-terminated before we call
      // Write().

      finish_buf[charLength] = '\0';

      rv = aStream->Write(finish_buf, charLength, &written);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCAutoString entString("&#");
      if (NS_IS_HIGH_SURROGATE(unicodeBuf[unicodeLength - 1]) && 
          unicodeLength < startLength && NS_IS_LOW_SURROGATE(unicodeBuf[unicodeLength]))  {
        entString.AppendInt(SURROGATE_TO_UCS4(unicodeBuf[unicodeLength - 1],
                                              unicodeBuf[unicodeLength]));
        unicodeLength += 1;
      }
      else
        entString.AppendInt(unicodeBuf[unicodeLength - 1]);
      entString.Append(';');

      // Since entString is an nsCAutoString we know entString.get()
      // returns a null-terminated string, so no need for extra
      // null-termination before calling Write() here.

      rv = aStream->Write(entString.get(), entString.Length(), &written);
      NS_ENSURE_SUCCESS(rv, rv);

      unicodeBuf += unicodeLength;
      startLength -= unicodeLength;
    }
  } while (convert_rv == NS_ERROR_UENC_NOMAPPING);

  return rv;
}

nsresult
nsDocumentEncoder::FlushText(nsAString& aString, bool aForce)
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
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

  aChild = nsnull;

  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsIContent *child = content->GetChildAt(aIndex);

  if (child)
    return CallQueryInterface(child, &aChild);

  return NS_OK;
}

static PRInt32 IndexOf(nsIDOMNode* aParent, nsIDOMNode* aChild)
{
  nsCOMPtr<nsIContent> parent(do_QueryInterface(aParent));
  nsCOMPtr<nsIContent> child(do_QueryInterface(aChild));

  if (!parent)
    return -1;

  return parent->IndexOf(child);
}

static inline PRInt32 GetIndex(nsTArray<PRInt32>& aIndexArray)
{
  PRInt32 count = aIndexArray.Length();

  if (count) {
    return aIndexArray.ElementAt(count - 1);
  }

  return 0;
}

static nsresult GetNextNode(nsIDOMNode* aNode, nsTArray<PRInt32>& aIndexArray,
                            nsIDOMNode*& aNextNode,
                            nsRangeIterationDirection& aDirection)
{
  bool hasChildren;

  aNextNode = nsnull;

  aNode->HasChildNodes(&hasChildren);

  if (hasChildren && aDirection == kDirectionIn) {
    ChildAt(aNode, 0, aNextNode);
    NS_ENSURE_TRUE(aNextNode, NS_ERROR_FAILURE);

    aIndexArray.AppendElement(0);

    aDirection = kDirectionIn;
  } else if (aDirection == kDirectionIn) {
    aNextNode = aNode;

    NS_ADDREF(aNextNode);

    aDirection = kDirectionOut;
  } else {
    nsCOMPtr<nsIDOMNode> parent;

    aNode->GetParentNode(getter_AddRefs(parent));
    NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);

    PRInt32 count = aIndexArray.Length();

    if (count) {
      PRInt32 indx = aIndexArray.ElementAt(count - 1);

      ChildAt(parent, indx + 1, aNextNode);

      if (aNextNode)
        aIndexArray.ElementAt(count - 1) = indx + 1;
      else
        aIndexArray.RemoveElementAt(count - 1);
    } else {
      PRInt32 indx = IndexOf(parent, aNode);

      if (indx >= 0) {
        ChildAt(parent, indx + 1, aNextNode);

        if (aNextNode)
          aIndexArray.AppendElement(indx + 1);
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

static bool IsTextNode(nsINode *aNode)
{
  return aNode && aNode->IsNodeOfType(nsINode::eTEXT);
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
    bool hasChildNodes;
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
nsDocumentEncoder::SerializeRangeNodes(nsIRange* aRange,
                                       nsINode* aNode,
                                       nsAString& aString,
                                       PRInt32 aDepth)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  if (!IsVisibleNode(aNode))
    return NS_OK;

  nsresult rv = NS_OK;

  // get start and end nodes for this recursion level
  nsCOMPtr<nsIContent> startNode, endNode;
  {
    PRInt32 start = mStartRootIndex - aDepth;
    if (start >= 0 && (PRUint32)start <= mStartNodes.Length())
      startNode = mStartNodes[start];

    PRInt32 end = mEndRootIndex - aDepth;
    if (end >= 0 && (PRUint32)end <= mEndNodes.Length())
      endNode = mEndNodes[end];
  }

  if (startNode != content && endNode != content)
  {
    // node is completely contained in range.  Serialize the whole subtree
    // rooted by this node.
    rv = SerializeToStringRecursive(aNode, aString, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
  {
    // due to implementation it is impossible for text node to be both start and end of 
    // range.  We would have handled that case without getting here.
    //XXXsmaug What does this all mean?
    if (IsTextNode(aNode))
    {
      if (startNode == content)
      {
        PRInt32 startOffset = aRange->StartOffset();
        rv = SerializeNodeStart(aNode, startOffset, -1, aString);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else
      {
        PRInt32 endOffset = aRange->EndOffset();
        rv = SerializeNodeStart(aNode, 0, endOffset, aString);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    else
    {
      if (aNode != mCommonParent)
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
      
      // do some calculations that will tell us which children of this
      // node are in the range.
      nsIContent* childAsNode = nsnull;
      PRInt32 startOffset = 0, endOffset = -1;
      if (startNode == content && mStartRootIndex >= aDepth)
        startOffset = mStartOffsets[mStartRootIndex - aDepth];
      if (endNode == content && mEndRootIndex >= aDepth)
        endOffset = mEndOffsets[mEndRootIndex - aDepth];
      // generated content will cause offset values of -1 to be returned.  
      PRInt32 j;
      PRUint32 childCount = content->GetChildCount();

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
        if (aNode != aRange->GetEndParent())
        {
          endOffset++;
        }
      }
      // serialize the children of this node that are in the range
      for (j=startOffset; j<endOffset; j++)
      {
        childAsNode = content->GetChildAt(j);

        if ((j==startOffset) || (j==endOffset-1))
          rv = SerializeRangeNodes(aRange, childAsNode, aString, aDepth+1);
        else
          rv = SerializeToStringRecursive(childAsNode, aString, PR_FALSE);

        NS_ENSURE_SUCCESS(rv, rv);
      }

      // serialize the end of this node
      if (aNode != mCommonParent)
      {
        rv = SerializeNodeEnd(aNode, aString);
        NS_ENSURE_SUCCESS(rv, rv); 
      }
    }
  }
  return NS_OK;
}

nsresult
nsDocumentEncoder::SerializeRangeContextStart(const nsTArray<nsINode*>& aAncestorArray,
                                              nsAString& aString)
{
  PRInt32 i = aAncestorArray.Length();
  nsresult rv = NS_OK;

  while (i > 0) {
    nsINode *node = aAncestorArray.ElementAt(--i);

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
nsDocumentEncoder::SerializeRangeContextEnd(const nsTArray<nsINode*>& aAncestorArray,
                                            nsAString& aString)
{
  PRInt32 i = 0;
  PRInt32 count = aAncestorArray.Length();
  nsresult rv = NS_OK;

  while (i < count) {
    nsINode *node = aAncestorArray.ElementAt(i++);

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
nsDocumentEncoder::SerializeRangeToString(nsIRange *aRange,
                                          nsAString& aOutputString)
{
  if (!aRange || aRange->Collapsed())
    return NS_OK;

  mCommonParent = aRange->GetCommonAncestor();

  if (!mCommonParent)
    return NS_OK;
  
  nsINode* startParent = aRange->GetStartParent();
  NS_ENSURE_TRUE(startParent, NS_ERROR_FAILURE);
  PRInt32 startOffset = aRange->StartOffset();

  nsINode* endParent = aRange->GetEndParent();
  NS_ENSURE_TRUE(endParent, NS_ERROR_FAILURE);
  PRInt32 endOffset = aRange->EndOffset();

  mCommonAncestors.Clear();
  mStartNodes.Clear();
  mStartOffsets.Clear();
  mEndNodes.Clear();
  mEndOffsets.Clear();

  nsContentUtils::GetAncestors(mCommonParent, mCommonAncestors);
  nsCOMPtr<nsIDOMNode> sp = do_QueryInterface(startParent);
  nsContentUtils::GetAncestorsAndOffsets(sp, startOffset,
                                         &mStartNodes, &mStartOffsets);
  nsCOMPtr<nsIDOMNode> ep = do_QueryInterface(endParent);
  nsContentUtils::GetAncestorsAndOffsets(ep, endOffset,
                                         &mEndNodes, &mEndOffsets);

  nsCOMPtr<nsIContent> commonContent = do_QueryInterface(mCommonParent);
  mStartRootIndex = mStartNodes.IndexOf(commonContent);
  mEndRootIndex = mEndNodes.IndexOf(commonContent);
  
  nsresult rv = NS_OK;

  rv = SerializeRangeContextStart(mCommonAncestors, aOutputString);
  NS_ENSURE_SUCCESS(rv, rv);

  if ((startParent == endParent) && IsTextNode(startParent))
  {
    if (mFlags & SkipInvisibleContent) {
      // Check that the parent is visible if we don't a frame.
      // IsVisibleNode() will do it when there's a frame.
      nsCOMPtr<nsIContent> content = do_QueryInterface(startParent);
      if (content && !content->GetPrimaryFrame()) {
        nsIContent* parent = content->GetParent();
        if (!parent || !IsVisibleNode(parent))
          return NS_OK;
      }
    }
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
nsDocumentEncoder::EncodeToString(nsAString& aOutputString)
{
  if (!mDocument)
    return NS_ERROR_NOT_INITIALIZED;

  aOutputString.Truncate();

  nsString output;
  static const size_t bufferSize = 2048;
  if (!mCachedBuffer) {
    mCachedBuffer = nsStringBuffer::Alloc(bufferSize);
  }
  NS_ASSERTION(!mCachedBuffer->IsReadonly(),
               "DocumentEncoder shouldn't keep reference to non-readonly buffer!");
  static_cast<PRUnichar*>(mCachedBuffer->Data())[0] = PRUnichar(0);
  mCachedBuffer->ToString(0, output, PR_TRUE);
  // output owns the buffer now!
  mCachedBuffer = nsnull;
  

  if (!mSerializer) {
    nsCAutoString progId(NS_CONTENTSERIALIZER_CONTRACTID_PREFIX);
    AppendUTF16toUTF8(mMimeType, progId);

    mSerializer = do_CreateInstance(progId.get());
    NS_ENSURE_TRUE(mSerializer, NS_ERROR_NOT_IMPLEMENTED);
  }

  nsresult rv = NS_OK;

  nsCOMPtr<nsIAtom> charsetAtom;
  if (!mCharset.IsEmpty()) {
    if (!mCharsetConverterManager) {
      mCharsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  
  bool rewriteEncodingDeclaration = !(mSelection || mRange || mNode) && !(mFlags & OutputDontRewriteEncodingDeclaration);
  mSerializer->Init(mFlags, mWrapColumn, mCharset.get(), mIsCopying, rewriteEncodingDeclaration);

  if (mSelection) {
    nsCOMPtr<nsIDOMRange> range;
    PRInt32 i, count = 0;

    rv = mSelection->GetRangeCount(&count);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> node, prevNode;
    for (i = 0; i < count; i++) {
      mSelection->GetRangeAt(i, getter_AddRefs(range));

      // Bug 236546: newlines not added when copying table cells into clipboard
      // Each selected cell shows up as a range containing a row with a single cell
      // get the row, compare it to previous row and emit </tr><tr> as needed
      range->GetStartContainer(getter_AddRefs(node));
      NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);
      if (node != prevNode) {
        if (prevNode) {
          nsCOMPtr<nsINode> p = do_QueryInterface(prevNode);
          rv = SerializeNodeEnd(p, output);
          NS_ENSURE_SUCCESS(rv, rv);
          prevNode = nsnull;
        }
        nsCOMPtr<nsIContent> content = do_QueryInterface(node);
        if (content && content->Tag() == nsGkAtoms::tr) {
          nsCOMPtr<nsINode> n = do_QueryInterface(node);
          rv = SerializeNodeStart(n, 0, -1, output);
          NS_ENSURE_SUCCESS(rv, rv);
          prevNode = node;
        }
      }

      nsCOMPtr<nsIRange> r = do_QueryInterface(range);
      rv = SerializeRangeToString(r, output);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (prevNode) {
      nsCOMPtr<nsINode> p = do_QueryInterface(prevNode);
      rv = SerializeNodeEnd(p, output);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    mSelection = nsnull;
  } else if (mRange) {
      rv = SerializeRangeToString(mRange, output);

      mRange = nsnull;
  } else if (mNode) {
    if (!mNodeFixup && !(mFlags & SkipInvisibleContent) && !mStream &&
        mNodeIsContainer) {
      rv = SerializeToStringIterative(mNode, output);
    } else {
      rv = SerializeToStringRecursive(mNode, output, mNodeIsContainer);
    }
    mNode = nsnull;
  } else {
    rv = mSerializer->AppendDocumentStart(mDocument, output);

    if (NS_SUCCEEDED(rv)) {
      rv = SerializeToStringRecursive(mDocument, output, PR_FALSE);
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);
  rv = mSerializer->Flush(output);
 
  mCachedBuffer = nsStringBuffer::FromString(output);
  // We have to be careful how we set aOutputString, because we don't
  // want it to end up sharing mCachedBuffer if we plan to reuse it.
  bool setOutput = false;
  // Try to cache the buffer.
  if (mCachedBuffer) {
    if (mCachedBuffer->StorageSize() == bufferSize &&
        !mCachedBuffer->IsReadonly()) {
      mCachedBuffer->AddRef();
    } else {
      if (NS_SUCCEEDED(rv)) {
        mCachedBuffer->ToString(output.Length(), aOutputString);
        setOutput = PR_TRUE;
      }
      mCachedBuffer = nsnull;
    }
  }

  if (!setOutput && NS_SUCCEEDED(rv)) {
    aOutputString.Append(output.get(), output.Length());
  }

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

  rv = mCharsetConverterManager->GetUnicodeEncoder(mCharset.get(),
                                                   getter_AddRefs(mUnicodeEncoder));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mMimeType.LowerCaseEqualsLiteral("text/plain")) {
    rv = mUnicodeEncoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, '?');
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
nsDocumentEncoder::EncodeToStringWithContext(nsAString& aContextString,
                                             nsAString& aInfoString,
                                             nsAString& aEncodedString)
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

class nsHTMLCopyEncoder : public nsDocumentEncoder
{
public:

  nsHTMLCopyEncoder();
  virtual ~nsHTMLCopyEncoder();

  NS_IMETHOD Init(nsIDOMDocument* aDocument, const nsAString& aMimeType, PRUint32 aFlags);

  // overridden methods from nsDocumentEncoder
  NS_IMETHOD SetSelection(nsISelection* aSelection);
  NS_IMETHOD EncodeToStringWithContext(nsAString& aContextString,
                                       nsAString& aInfoString,
                                       nsAString& aEncodedString);

protected:

  enum Endpoint
  {
    kStart,
    kEnd
  };
  
  nsresult PromoteRange(nsIDOMRange *inRange);
  nsresult PromoteAncestorChain(nsCOMPtr<nsIDOMNode> *ioNode, 
                                PRInt32 *ioStartOffset, 
                                PRInt32 *ioEndOffset);
  nsresult GetPromotedPoint(Endpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                            nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset, nsIDOMNode *aCommon);
  nsCOMPtr<nsIDOMNode> GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset);
  bool IsMozBR(nsIDOMNode* aNode);
  nsresult GetNodeLocation(nsIDOMNode *inChild, nsCOMPtr<nsIDOMNode> *outParent, PRInt32 *outOffset);
  bool IsRoot(nsIDOMNode* aNode);
  bool IsFirstNode(nsIDOMNode *aNode);
  bool IsLastNode(nsIDOMNode *aNode);
  bool IsEmptyTextContent(nsIDOMNode* aNode);
  virtual bool IncludeInContext(nsINode *aNode);

  bool mIsTextWidget;
};

nsHTMLCopyEncoder::nsHTMLCopyEncoder()
{
  mIsTextWidget = PR_FALSE;
}

nsHTMLCopyEncoder::~nsHTMLCopyEncoder()
{
}

NS_IMETHODIMP
nsHTMLCopyEncoder::Init(nsIDOMDocument* aDocument,
                        const nsAString& aMimeType,
                        PRUint32 aFlags)
{
  if (!aDocument)
    return NS_ERROR_INVALID_ARG;

  mIsTextWidget = PR_FALSE;
  Initialize();

  mIsCopying = PR_TRUE;
  mDocument = do_QueryInterface(aDocument);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  mMimeType.AssignLiteral("text/html");

  // Make all links absolute when copying
  // (see related bugs #57296, #41924, #58646, #32768)
  mFlags = aFlags | OutputAbsoluteLinks;

  if (!mDocument->IsScriptEnabled())
    mFlags |= OutputNoScriptContent;

  return NS_OK;
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

  for (nsCOMPtr<nsIContent> selContent(do_QueryInterface(commonParent));
       selContent;
       selContent = selContent->GetParent())
  {
    // checking for selection inside a plaintext form widget
    nsIAtom *atom = selContent->Tag();
    if (atom == nsGkAtoms::input ||
        atom == nsGkAtoms::textarea)
    {
      mIsTextWidget = PR_TRUE;
      break;
    }
    else if (atom == nsGkAtoms::body)
    {
      // check for moz prewrap style on body.  If it's there we are 
      // in a plaintext editor.  This is pretty cheezy but I haven't 
      // found a good way to tell if we are in a plaintext editor.
      nsCOMPtr<nsIDOMElement> bodyElem = do_QueryInterface(selContent);
      nsAutoString wsVal;
      rv = bodyElem->GetAttribute(NS_LITERAL_STRING("style"), wsVal);
      if (NS_SUCCEEDED(rv) && (kNotFound != wsVal.Find(NS_LITERAL_STRING("pre-wrap"))))
      {
        mIsTextWidget = PR_TRUE;
        break;
      }
    }
  }
  
  // also consider ourselves in a text widget if we can't find an html document
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
  if (!(htmlDoc && mDocument->IsHTML()))
    mIsTextWidget = PR_TRUE;
  
  // normalize selection if we are not in a widget
  if (mIsTextWidget) 
  {
    mSelection = aSelection;
    mMimeType.AssignLiteral("text/plain");
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
nsHTMLCopyEncoder::EncodeToStringWithContext(nsAString& aContextString,
                                             nsAString& aInfoString,
                                             nsAString& aEncodedString)
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
  PRInt32 count = mCommonAncestors.Length();
  PRInt32 i;
  nsCOMPtr<nsINode> node;
  if (count > 0)
    node = mCommonAncestors.ElementAt(0);

  if (node && IsTextNode(node)) 
  {
    mCommonAncestors.RemoveElementAt(0);
    // don't forget to adjust range depth info
    if (mStartDepth) mStartDepth--;
    if (mEndDepth) mEndDepth--;
    // and the count
    count--;
  }
  
  i = count;
  while (i > 0)
  {
    node = mCommonAncestors.ElementAt(--i);
    SerializeNodeStart(node, 0, -1, aContextString);
  }
  //i = 0; guaranteed by above
  while (i < count)
  {
    node = mCommonAncestors.ElementAt(i++);
    SerializeNodeEnd(node, aContextString);
  }

  // encode range info : the start and end depth of the selection, where the depth is 
  // distance down in the parent hierarchy.  Later we will need to add leading/trailing
  // whitespace info to this.
  nsAutoString infoString;
  infoString.AppendInt(mStartDepth);
  infoString.Append(PRUnichar(','));
  infoString.AppendInt(mEndDepth);
  aInfoString = infoString;
  
  return NS_OK;
}


bool
nsHTMLCopyEncoder::IncludeInContext(nsINode *aNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

  if (!content)
    return PR_FALSE;

  nsIAtom *tag = content->Tag();

  return (tag == nsGkAtoms::b        ||
          tag == nsGkAtoms::i        ||
          tag == nsGkAtoms::u        ||
          tag == nsGkAtoms::a        ||
          tag == nsGkAtoms::tt       ||
          tag == nsGkAtoms::s        ||
          tag == nsGkAtoms::big      ||
          tag == nsGkAtoms::small    ||
          tag == nsGkAtoms::strike   ||
          tag == nsGkAtoms::em       ||
          tag == nsGkAtoms::strong   ||
          tag == nsGkAtoms::dfn      ||
          tag == nsGkAtoms::code     ||
          tag == nsGkAtoms::cite     ||
          tag == nsGkAtoms::var      ||
          tag == nsGkAtoms::abbr     ||
          tag == nsGkAtoms::font     ||
          tag == nsGkAtoms::script   ||
          tag == nsGkAtoms::span     ||
          tag == nsGkAtoms::pre      ||
          tag == nsGkAtoms::h1       ||
          tag == nsGkAtoms::h2       ||
          tag == nsGkAtoms::h3       ||
          tag == nsGkAtoms::h4       ||
          tag == nsGkAtoms::h5       ||
          tag == nsGkAtoms::h6);
}


nsresult 
nsHTMLCopyEncoder::PromoteRange(nsIDOMRange *inRange)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult rv;
  nsCOMPtr<nsIDOMNode> startNode, endNode, common;
  PRInt32 startOffset, endOffset;
  
  rv = inRange->GetCommonAncestorContainer(getter_AddRefs(common));
  NS_ENSURE_SUCCESS(rv, rv);
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
  
  // examine range endpoints.  
  rv = GetPromotedPoint( kStart, startNode, startOffset, address_of(opStartNode), &opStartOffset, common);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GetPromotedPoint( kEnd, endNode, endOffset, address_of(opEndNode), &opEndOffset, common);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // if both range endpoints are at the common ancestor, check for possible inclusion of ancestors
  if ( (opStartNode == common) && (opEndNode == common) )
  {
    rv = PromoteAncestorChain(address_of(opStartNode), &opStartOffset, &opEndOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    opEndNode = opStartNode;
  }
  
  // set the range to the new values
  rv = inRange->SetStart(opStartNode, opStartOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = inRange->SetEnd(opEndNode, opEndOffset);
  return rv;
} 


// PromoteAncestorChain will promote a range represented by [{*ioNode,*ioStartOffset} , {*ioNode,*ioEndOffset}]
// The promotion is different from that found in getPromotedPoint: it will only promote one endpoint if it can
// promote the other.  Thus, instead of having a startnode/endNode, there is just the one ioNode.
nsresult
nsHTMLCopyEncoder::PromoteAncestorChain(nsCOMPtr<nsIDOMNode> *ioNode, 
                                        PRInt32 *ioStartOffset, 
                                        PRInt32 *ioEndOffset) 
{
  if (!ioNode || !ioStartOffset || !ioEndOffset) return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  bool done = false;

  nsCOMPtr<nsIDOMNode> frontNode, endNode, parent;
  PRInt32 frontOffset, endOffset;

  //save the editable state of the ioNode, so we don't promote an ancestor if it has different editable state
  nsCOMPtr<nsINode> node = do_QueryInterface(*ioNode);
  bool isEditable = node->IsEditable();
  
  // loop for as long as we can promote both endpoints
  while (!done)
  {
    rv = (*ioNode)->GetParentNode(getter_AddRefs(parent));
    if ((NS_FAILED(rv)) || !parent)
      done = PR_TRUE;
    else
    {
      // passing parent as last param to GetPromotedPoint() allows it to promote only one level
      // up the hierarchy.
      rv = GetPromotedPoint( kStart, *ioNode, *ioStartOffset, address_of(frontNode), &frontOffset, parent);
      NS_ENSURE_SUCCESS(rv, rv);
      // then we make the same attempt with the endpoint
      rv = GetPromotedPoint( kEnd, *ioNode, *ioEndOffset, address_of(endNode), &endOffset, parent);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsINode> frontINode = do_QueryInterface(frontNode);
      // if both endpoints were promoted one level and isEditable is the same as the original node, 
      // keep looping - otherwise we are done.
      if ( (frontNode != parent) || (endNode != parent) || (frontINode->IsEditable() != isEditable) )
        done = PR_TRUE;
      else
      {
        *ioNode = frontNode;  
        *ioStartOffset = frontOffset;
        *ioEndOffset = endOffset;
      }
    }
  }
  return rv;
}

nsresult
nsHTMLCopyEncoder::GetPromotedPoint(Endpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                                  nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset, nsIDOMNode *common)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMNode> parent = aNode;
  PRInt32 offset = aOffset;
  bool    bResetPromotion = false;
  
  // default values
  *outNode = node;
  *outOffset = offset;

  if (common == node) 
    return NS_OK;
    
  if (aWhere == kStart)
  {
    // some special casing for text nodes
    nsCOMPtr<nsINode> t = do_QueryInterface(aNode);
    if (IsTextNode(t))
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
        if (!text.IsEmpty())
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
    if (!IsRoot(node) && (parent != common))
    {
      rv = GetNodeLocation(node, address_of(parent), &offset);
      NS_ENSURE_SUCCESS(rv, rv);
      if (offset == -1) return NS_OK; // we hit generated content; STOP
      nsIParserService *parserService = nsContentUtils::GetParserService();
      if (!parserService)
        return NS_ERROR_OUT_OF_MEMORY;
      while ((IsFirstNode(node)) && (!IsRoot(parent)) && (parent != common))
      {
        if (bResetPromotion)
        {
          nsCOMPtr<nsIContent> content = do_QueryInterface(parent);
          if (content)
          {
            bool isBlock = false;
            parserService->IsBlock(parserService->HTMLAtomTagToId(content->Tag()), isBlock);
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
    nsCOMPtr<nsINode> n = do_QueryInterface(aNode);
    if (IsTextNode(n))
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
        if (!text.IsEmpty())
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
    if (!IsRoot(node) && (parent != common))
    {
      rv = GetNodeLocation(node, address_of(parent), &offset);
      NS_ENSURE_SUCCESS(rv, rv);
      if (offset == -1) return NS_OK; // we hit generated content; STOP
      nsIParserService *parserService = nsContentUtils::GetParserService();
      if (!parserService)
        return NS_ERROR_OUT_OF_MEMORY;
      while ((IsLastNode(node)) && (!IsRoot(parent)) && (parent != common))
      {
        if (bResetPromotion)
        {
          nsCOMPtr<nsIContent> content = do_QueryInterface(parent);
          if (content)
          {
            bool isBlock = false;
            parserService->IsBlock(parserService->HTMLAtomTagToId(content->Tag()), isBlock);
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
  NS_PRECONDITION(content, "null content in nsHTMLCopyEncoder::GetChildAt");

  resultNode = do_QueryInterface(content->GetChildAt(aOffset));

  return resultNode;
}

bool 
nsHTMLCopyEncoder::IsMozBR(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (IsTag(content, nsGkAtoms::br))
  {
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
    if (elem)
    {
      nsAutoString typeAttrName(NS_LITERAL_STRING("type"));
      nsAutoString typeAttrVal;
      nsresult rv = elem->GetAttribute(typeAttrName, typeAttrVal);
      ToLowerCase(typeAttrVal);
      if (NS_SUCCEEDED(rv) && (typeAttrVal.EqualsLiteral("_moz")))
        return PR_TRUE;
    }
    return PR_FALSE;
  }
  return PR_FALSE;
}

nsresult 
nsHTMLCopyEncoder::GetNodeLocation(nsIDOMNode *inChild,
                                   nsCOMPtr<nsIDOMNode> *outParent,
                                   PRInt32 *outOffset)
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
      if (!cChild || !content)
        return NS_ERROR_NULL_POINTER;

      *outOffset = content->IndexOf(cChild);
    }
  }
  return result;
}

bool
nsHTMLCopyEncoder::IsRoot(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content)
  {
    if (mIsTextWidget) 
      return (IsTag(content, nsGkAtoms::div));

    return (IsTag(content, nsGkAtoms::body) ||
            IsTag(content, nsGkAtoms::td)   ||
            IsTag(content, nsGkAtoms::th));
  }
  return PR_FALSE;
}

bool
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


bool
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

bool
nsHTMLCopyEncoder::IsEmptyTextContent(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> cont = do_QueryInterface(aNode);
  return cont && cont->TextIsOnlyWhitespace();
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
