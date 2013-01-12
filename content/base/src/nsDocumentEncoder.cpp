/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsRange.h"
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
#include "nsIEditorDocShell.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIDocShell.h"

using namespace mozilla;
using namespace mozilla::dom;

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
  nsresult SerializeNodeStart(nsINode* aNode, int32_t aStartOffset,
                              int32_t aEndOffset, nsAString& aStr,
                              nsINode* aOriginalNode = nullptr);
  nsresult SerializeToStringRecursive(nsINode* aNode,
                                      nsAString& aStr,
                                      bool aDontSerializeRoot);
  nsresult SerializeNodeEnd(nsINode* aNode, nsAString& aStr);
  // This serializes the content of aNode.
  nsresult SerializeToStringIterative(nsINode* aNode,
                                      nsAString& aStr);
  nsresult SerializeRangeToString(nsRange *aRange,
                                  nsAString& aOutputString);
  nsresult SerializeRangeNodes(nsRange* aRange, 
                               nsINode* aNode, 
                               nsAString& aString,
                               int32_t aDepth);
  nsresult SerializeRangeContextStart(const nsTArray<nsINode*>& aAncestorArray,
                                      nsAString& aString);
  nsresult SerializeRangeContextEnd(const nsTArray<nsINode*>& aAncestorArray,
                                    nsAString& aString);
  virtual int32_t
  GetImmediateContextCount(const nsTArray<nsINode*>& aAncestorArray)
  {
    return -1;
  }

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
            return true;
          }
          return false;
        }
        bool isVisible = frame->GetStyleVisibility()->IsVisible();
        if (!isVisible && aNode->IsNodeOfType(nsINode::eTEXT))
          return false;
      }
    }
    return true;
  }

  static bool IsTag(nsIContent* aContent, nsIAtom* aAtom);
  
  virtual bool IncludeInContext(nsINode *aNode);

  nsCOMPtr<nsIDocument>          mDocument;
  nsCOMPtr<nsISelection>         mSelection;
  nsRefPtr<nsRange>              mRange;
  nsCOMPtr<nsINode>              mNode;
  nsCOMPtr<nsIOutputStream>      mStream;
  nsCOMPtr<nsIContentSerializer> mSerializer;
  nsCOMPtr<nsIUnicodeEncoder>    mUnicodeEncoder;
  nsCOMPtr<nsINode>              mCommonParent;
  nsCOMPtr<nsIDocumentEncoderNodeFixup> mNodeFixup;
  nsCOMPtr<nsICharsetConverterManager> mCharsetConverterManager;

  nsString          mMimeType;
  nsCString         mCharset;
  uint32_t          mFlags;
  uint32_t          mWrapColumn;
  uint32_t          mStartDepth;
  uint32_t          mEndDepth;
  int32_t           mStartRootIndex;
  int32_t           mEndRootIndex;
  nsAutoTArray<nsINode*, 8>    mCommonAncestors;
  nsAutoTArray<nsIContent*, 8> mStartNodes;
  nsAutoTArray<int32_t, 8>     mStartOffsets;
  nsAutoTArray<nsIContent*, 8> mEndNodes;
  nsAutoTArray<int32_t, 8>     mEndOffsets;
  bool              mHaltRangeHint;  
  // Used when context has already been serialized for
  // table cell selections (where parent is <tr>)
  bool              mDisableContextSerialize;
  bool              mIsCopying;  // Set to true only while copying
  bool              mNodeIsContainer;
  nsStringBuffer*   mCachedBuffer;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDocumentEncoder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDocumentEncoder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDocumentEncoder)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentEncoder)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDocumentEncoder)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCommonParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDocumentEncoder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCommonParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsDocumentEncoder::nsDocumentEncoder() : mCachedBuffer(nullptr)
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
  mHaltRangeHint = false;
  mDisableContextSerialize = false;
  mNodeIsContainer = false;
  if (aClearCachedSerializer) {
    mSerializer = nullptr;
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
                        uint32_t aFlags)
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
                              uint32_t aFlags)
{
  if (!aDocument)
    return NS_ERROR_INVALID_ARG;

  Initialize(!mMimeType.Equals(aMimeType));

  mDocument = aDocument;

  mMimeType = aMimeType;

  mFlags = aFlags;
  mIsCopying = false;

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetWrapColumn(uint32_t aWC)
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
  mRange = static_cast<nsRange*>(aRange);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetNode(nsIDOMNode* aNode)
{
  mNodeIsContainer = false;
  mNode = do_QueryInterface(aNode);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetNativeNode(nsINode* aNode)
{
  mNodeIsContainer = false;
  mNode = aNode;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetContainerNode(nsIDOMNode *aContainer)
{
  mNodeIsContainer = true;
  mNode = do_QueryInterface(aContainer);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentEncoder::SetNativeContainerNode(nsINode* aContainer)
{
  mNodeIsContainer = true;
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
  return false;
}

static
bool
IsInvisibleBreak(nsINode *aNode) {
  // xxxehsan: we should probably figure out a way to determine
  // if a BR node is visible without using the editor.
  Element* elt = aNode->AsElement();
  if (!elt->IsHTML(nsGkAtoms::br) ||
      !aNode->IsEditable()) {
    return false;
  }

  // Grab the editor associated with the document
  nsIDocument *doc = aNode->GetCurrentDoc();
  if (doc) {
    nsPIDOMWindow *window = doc->GetWindow();
    if (window) {
      nsIDocShell *docShell = window->GetDocShell();
      if (docShell) {
        nsCOMPtr<nsIEditorDocShell> editorDocShell = do_QueryInterface(docShell);
        if (editorDocShell) {
          nsCOMPtr<nsIEditor> editor;
          editorDocShell->GetEditor(getter_AddRefs(editor));
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
          if (htmlEditor) {
            bool isVisible = false;
            nsCOMPtr<nsIDOMNode> domNode = do_QueryInterface(aNode);
            htmlEditor->BreakIsVisible(domNode, &isVisible);
            return !isVisible;
          }
        }
      }
    }
  }
  return false;
}

nsresult
nsDocumentEncoder::SerializeNodeStart(nsINode* aNode,
                                      int32_t aStartOffset,
                                      int32_t aEndOffset,
                                      nsAString& aStr,
                                      nsINode* aOriginalNode)
{
  if (!IsVisibleNode(aNode))
    return NS_OK;
  
  nsINode* node = nullptr;
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
    if ((mFlags & (nsIDocumentEncoder::OutputPreformatted |
                   nsIDocumentEncoder::OutputDropInvisibleBreak)) &&
        IsInvisibleBreak(node)) {
      return NS_OK;
    }
    Element* originalElement =
      aOriginalNode && aOriginalNode->IsElement() ?
        aOriginalNode->AsElement() : nullptr;
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
  nsINode* maybeFixedNode = nullptr;

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
        frame->IsSelectable(&isSelectable, nullptr);
        if (!isSelectable){
          aDontSerializeRoot = true;
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
    rv = SerializeToStringRecursive(child, aStr, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aDontSerializeRoot) {
    rv = SerializeNodeEnd(node, aStr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return FlushText(aStr, false);
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
        current = current->GetParentNode();
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
  int32_t charLength, startCharLength;
  const nsPromiseFlatString& flat = PromiseFlatString(aString);
  const PRUnichar* unicodeBuf = flat.get();
  int32_t unicodeLength = aString.Length();
  int32_t startLength = unicodeLength;

  rv = aEncoder->GetMaxLength(unicodeBuf, unicodeLength, &charLength);
  startCharLength = charLength;
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString charXferString;
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

    uint32_t written;
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

      nsAutoCString entString("&#");
      if (NS_IS_HIGH_SURROGATE(unicodeBuf[unicodeLength - 1]) && 
          unicodeLength < startLength && NS_IS_LOW_SURROGATE(unicodeBuf[unicodeLength]))  {
        entString.AppendInt(SURROGATE_TO_UCS4(unicodeBuf[unicodeLength - 1],
                                              unicodeBuf[unicodeLength]));
        unicodeLength += 1;
      }
      else
        entString.AppendInt(unicodeBuf[unicodeLength - 1]);
      entString.Append(';');

      // Since entString is an nsAutoCString we know entString.get()
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
static nsresult ChildAt(nsIDOMNode* aNode, int32_t aIndex, nsIDOMNode*& aChild)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

  aChild = nullptr;

  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsIContent *child = content->GetChildAt(aIndex);

  if (child)
    return CallQueryInterface(child, &aChild);

  return NS_OK;
}

static int32_t IndexOf(nsIDOMNode* aParent, nsIDOMNode* aChild)
{
  nsCOMPtr<nsIContent> parent(do_QueryInterface(aParent));
  nsCOMPtr<nsIContent> child(do_QueryInterface(aChild));

  if (!parent)
    return -1;

  return parent->IndexOf(child);
}

static inline int32_t GetIndex(nsTArray<int32_t>& aIndexArray)
{
  int32_t count = aIndexArray.Length();

  if (count) {
    return aIndexArray.ElementAt(count - 1);
  }

  return 0;
}

static nsresult GetNextNode(nsIDOMNode* aNode, nsTArray<int32_t>& aIndexArray,
                            nsIDOMNode*& aNextNode,
                            nsRangeIterationDirection& aDirection)
{
  bool hasChildren;

  aNextNode = nullptr;

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

    int32_t count = aIndexArray.Length();

    if (count) {
      int32_t indx = aIndexArray.ElementAt(count - 1);

      ChildAt(parent, indx + 1, aNextNode);

      if (aNextNode)
        aIndexArray.ElementAt(count - 1) = indx + 1;
      else
        aIndexArray.RemoveElementAt(count - 1);
    } else {
      int32_t indx = IndexOf(parent, aNode);

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

nsresult
nsDocumentEncoder::SerializeRangeNodes(nsRange* aRange,
                                       nsINode* aNode,
                                       nsAString& aString,
                                       int32_t aDepth)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  if (!IsVisibleNode(aNode))
    return NS_OK;

  nsresult rv = NS_OK;

  // get start and end nodes for this recursion level
  nsCOMPtr<nsIContent> startNode, endNode;
  {
    int32_t start = mStartRootIndex - aDepth;
    if (start >= 0 && (uint32_t)start <= mStartNodes.Length())
      startNode = mStartNodes[start];

    int32_t end = mEndRootIndex - aDepth;
    if (end >= 0 && (uint32_t)end <= mEndNodes.Length())
      endNode = mEndNodes[end];
  }

  if (startNode != content && endNode != content)
  {
    // node is completely contained in range.  Serialize the whole subtree
    // rooted by this node.
    rv = SerializeToStringRecursive(aNode, aString, false);
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
        int32_t startOffset = aRange->StartOffset();
        rv = SerializeNodeStart(aNode, startOffset, -1, aString);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else
      {
        int32_t endOffset = aRange->EndOffset();
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
          mHaltRangeHint = true;
        }
        if ((startNode == content) && !mHaltRangeHint) mStartDepth++;
        if ((endNode == content) && !mHaltRangeHint) mEndDepth++;
      
        // serialize the start of this node
        rv = SerializeNodeStart(aNode, 0, -1, aString);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      
      // do some calculations that will tell us which children of this
      // node are in the range.
      nsIContent* childAsNode = nullptr;
      int32_t startOffset = 0, endOffset = -1;
      if (startNode == content && mStartRootIndex >= aDepth)
        startOffset = mStartOffsets[mStartRootIndex - aDepth];
      if (endNode == content && mEndRootIndex >= aDepth)
        endOffset = mEndOffsets[mEndRootIndex - aDepth];
      // generated content will cause offset values of -1 to be returned.  
      int32_t j;
      uint32_t childCount = content->GetChildCount();

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
          rv = SerializeToStringRecursive(childAsNode, aString, false);

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
  if (mDisableContextSerialize) {
    return NS_OK;
  }
  int32_t i = aAncestorArray.Length(), j;
  nsresult rv = NS_OK;

  // currently only for table-related elements; see Bug 137450
  j = GetImmediateContextCount(aAncestorArray);

  while (i > 0) {
    nsINode *node = aAncestorArray.ElementAt(--i);

    if (!node)
      break;

    // Either a general inclusion or as immediate context
    if (IncludeInContext(node) || i < j) {
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
  if (mDisableContextSerialize) {
    return NS_OK;
  }
  int32_t i = 0, j;
  int32_t count = aAncestorArray.Length();
  nsresult rv = NS_OK;

  // currently only for table-related elements
  j = GetImmediateContextCount(aAncestorArray);

  while (i < count) {
    nsINode *node = aAncestorArray.ElementAt(i++);

    if (!node)
      break;

    // Either a general inclusion or as immediate context
    if (IncludeInContext(node) || i - 1 < j) {
      rv = SerializeNodeEnd(node, aString);

      if (NS_FAILED(rv))
        break;
    }
  }

  return rv;
}

nsresult
nsDocumentEncoder::SerializeRangeToString(nsRange *aRange,
                                          nsAString& aOutputString)
{
  if (!aRange || aRange->Collapsed())
    return NS_OK;

  mCommonParent = aRange->GetCommonAncestor();

  if (!mCommonParent)
    return NS_OK;
  
  nsINode* startParent = aRange->GetStartParent();
  NS_ENSURE_TRUE(startParent, NS_ERROR_FAILURE);
  int32_t startOffset = aRange->StartOffset();

  nsINode* endParent = aRange->GetEndParent();
  NS_ENSURE_TRUE(endParent, NS_ERROR_FAILURE);
  int32_t endOffset = aRange->EndOffset();

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
  mCachedBuffer->ToString(0, output, true);
  // output owns the buffer now!
  mCachedBuffer = nullptr;
  

  if (!mSerializer) {
    nsAutoCString progId(NS_CONTENTSERIALIZER_CONTRACTID_PREFIX);
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
    int32_t i, count = 0;

    rv = mSelection->GetRangeCount(&count);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> node, prevNode;
    for (i = 0; i < count; i++) {
      mSelection->GetRangeAt(i, getter_AddRefs(range));

      // Bug 236546: newlines not added when copying table cells into clipboard
      // Each selected cell shows up as a range containing a row with a single cell
      // get the row, compare it to previous row and emit </tr><tr> as needed
      // Bug 137450: Problem copying/pasting a table from a web page to Excel.
      // Each separate block of <tr></tr> produced above will be wrapped by the
      // immediate context. This assumes that you can't select cells that are
      // multiple selections from two tables simultaneously.
      range->GetStartContainer(getter_AddRefs(node));
      NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);
      if (node != prevNode) {
        nsCOMPtr<nsINode> p;
        if (prevNode) {
          p = do_QueryInterface(prevNode);
          rv = SerializeNodeEnd(p, output);
          NS_ENSURE_SUCCESS(rv, rv);
        }
        nsCOMPtr<nsIContent> content = do_QueryInterface(node);
        if (content && content->IsHTML(nsGkAtoms::tr)) {
          nsINode* n = content;
          if (!prevNode) {
            // Went from a non-<tr> to a <tr>
            mCommonAncestors.Clear();
            nsContentUtils::GetAncestors(n->GetParentNode(), mCommonAncestors);
            rv = SerializeRangeContextStart(mCommonAncestors, output);
            NS_ENSURE_SUCCESS(rv, rv);
            // Don't let SerializeRangeToString serialize the context again
            mDisableContextSerialize = true;
          }

          rv = SerializeNodeStart(n, 0, -1, output);
          NS_ENSURE_SUCCESS(rv, rv);
          prevNode = node;
        } else if (prevNode) {
          // Went from a <tr> to a non-<tr>
          mCommonAncestors.Clear();
          nsContentUtils::GetAncestors(p->GetParentNode(), mCommonAncestors);
          mDisableContextSerialize = false;
          rv = SerializeRangeContextEnd(mCommonAncestors, output);
          NS_ENSURE_SUCCESS(rv, rv);
          prevNode = nullptr;
        }
      }

      nsRange* r = static_cast<nsRange*>(range.get());
      rv = SerializeRangeToString(r, output);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (prevNode) {
      nsCOMPtr<nsINode> p = do_QueryInterface(prevNode);
      rv = SerializeNodeEnd(p, output);
      NS_ENSURE_SUCCESS(rv, rv);
      mCommonAncestors.Clear();
      nsContentUtils::GetAncestors(p->GetParentNode(), mCommonAncestors);
      mDisableContextSerialize = false; 
      rv = SerializeRangeContextEnd(mCommonAncestors, output);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Just to be safe
    mDisableContextSerialize = false; 

    mSelection = nullptr;
  } else if (mRange) {
      rv = SerializeRangeToString(mRange, output);

      mRange = nullptr;
  } else if (mNode) {
    if (!mNodeFixup && !(mFlags & SkipInvisibleContent) && !mStream &&
        mNodeIsContainer) {
      rv = SerializeToStringIterative(mNode, output);
    } else {
      rv = SerializeToStringRecursive(mNode, output, mNodeIsContainer);
    }
    mNode = nullptr;
  } else {
    rv = mSerializer->AppendDocumentStart(mDocument, output);

    if (NS_SUCCEEDED(rv)) {
      rv = SerializeToStringRecursive(mDocument, output, false);
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
        setOutput = true;
      }
      mCachedBuffer = nullptr;
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
    rv = mUnicodeEncoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nullptr, '?');
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bool chromeCaller = nsContentUtils::IsCallerChrome();
  if (chromeCaller) {
    mStream = aStream;
  }
  nsAutoString buf;

  rv = EncodeToString(buf);

  if (!chromeCaller) {
    mStream = aStream;
  }

  // Force a flush of the last chunk of data.
  FlushText(buf, true);

  mStream = nullptr;
  mUnicodeEncoder = nullptr;

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

  NS_IMETHOD Init(nsIDOMDocument* aDocument, const nsAString& aMimeType, uint32_t aFlags);

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
                                int32_t *ioStartOffset, 
                                int32_t *ioEndOffset);
  nsresult GetPromotedPoint(Endpoint aWhere, nsIDOMNode *aNode, int32_t aOffset, 
                            nsCOMPtr<nsIDOMNode> *outNode, int32_t *outOffset, nsIDOMNode *aCommon);
  nsCOMPtr<nsIDOMNode> GetChildAt(nsIDOMNode *aParent, int32_t aOffset);
  bool IsMozBR(nsIDOMNode* aNode);
  nsresult GetNodeLocation(nsIDOMNode *inChild, nsCOMPtr<nsIDOMNode> *outParent, int32_t *outOffset);
  bool IsRoot(nsIDOMNode* aNode);
  bool IsFirstNode(nsIDOMNode *aNode);
  bool IsLastNode(nsIDOMNode *aNode);
  bool IsEmptyTextContent(nsIDOMNode* aNode);
  virtual bool IncludeInContext(nsINode *aNode);
  virtual int32_t
  GetImmediateContextCount(const nsTArray<nsINode*>& aAncestorArray);

  bool mIsTextWidget;
};

nsHTMLCopyEncoder::nsHTMLCopyEncoder()
{
  mIsTextWidget = false;
}

nsHTMLCopyEncoder::~nsHTMLCopyEncoder()
{
}

NS_IMETHODIMP
nsHTMLCopyEncoder::Init(nsIDOMDocument* aDocument,
                        const nsAString& aMimeType,
                        uint32_t aFlags)
{
  if (!aDocument)
    return NS_ERROR_INVALID_ARG;

  mIsTextWidget = false;
  Initialize();

  mIsCopying = true;
  mDocument = do_QueryInterface(aDocument);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  // Hack, hack! Traditionally, the caller passes text/unicode, which is
  // treated as "guess text/html or text/plain" in this context. (It has a
  // different meaning in other contexts. Sigh.) From now on, "text/plain"
  // means forcing text/plain instead of guessing.
  if (aMimeType.EqualsLiteral("text/plain")) {
    mMimeType.AssignLiteral("text/plain");
  } else {
    mMimeType.AssignLiteral("text/html");
  }

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
  int32_t count = 0;

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
      mIsTextWidget = true;
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
        mIsTextWidget = true;
        break;
      }
    }
  }
  
  // also consider ourselves in a text widget if we can't find an html document
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
  if (!(htmlDoc && mDocument->IsHTML()))
    mIsTextWidget = true;
  
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
  while (static_cast<nsresult>(NS_ENUMERATOR_FALSE) == enumerator->IsDone())
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
  int32_t count = mCommonAncestors.Length();
  int32_t i;
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
    return false;

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
  int32_t startOffset, endOffset;
  
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
  int32_t opStartOffset, opEndOffset;
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
                                        int32_t *ioStartOffset, 
                                        int32_t *ioEndOffset) 
{
  if (!ioNode || !ioStartOffset || !ioEndOffset) return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  bool done = false;

  nsCOMPtr<nsIDOMNode> frontNode, endNode, parent;
  int32_t frontOffset, endOffset;

  //save the editable state of the ioNode, so we don't promote an ancestor if it has different editable state
  nsCOMPtr<nsINode> node = do_QueryInterface(*ioNode);
  bool isEditable = node->IsEditable();
  
  // loop for as long as we can promote both endpoints
  while (!done)
  {
    rv = (*ioNode)->GetParentNode(getter_AddRefs(parent));
    if ((NS_FAILED(rv)) || !parent)
      done = true;
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
        done = true;
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
nsHTMLCopyEncoder::GetPromotedPoint(Endpoint aWhere, nsIDOMNode *aNode, int32_t aOffset, 
                                  nsCOMPtr<nsIDOMNode> *outNode, int32_t *outOffset, nsIDOMNode *common)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMNode> parent = aNode;
  int32_t offset = aOffset;
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
        bResetPromotion = true;
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
              bResetPromotion = false;
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
      uint32_t len = n->Length();
      if (offset < (int32_t)len)
      {
        // unless everything after us in just whitespace.  NOTE: we need a more
        // general solution that truly detects all cases of non-significant
        // whitespace with no false alarms.
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(aNode);
        nsAutoString text;
        nodeAsText->SubstringData(offset, len-offset, text);
        text.CompressWhitespace();
        if (!text.IsEmpty())
          return NS_OK;
        bResetPromotion = true;
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
              bResetPromotion = false;
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
nsHTMLCopyEncoder::GetChildAt(nsIDOMNode *aParent, int32_t aOffset)
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
  MOZ_ASSERT(aNode);
  nsCOMPtr<Element> element = do_QueryInterface(aNode);
  return element &&
         element->IsHTML(nsGkAtoms::br) &&
         element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                              NS_LITERAL_STRING("_moz"), eIgnoreCase);
}

nsresult 
nsHTMLCopyEncoder::GetNodeLocation(nsIDOMNode *inChild,
                                   nsCOMPtr<nsIDOMNode> *outParent,
                                   int32_t *outOffset)
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
  return false;
}

bool
nsHTMLCopyEncoder::IsFirstNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  int32_t offset, j=0;
  nsresult rv = GetNodeLocation(aNode, address_of(parent), &offset);
  if (NS_FAILED(rv)) 
  {
    NS_NOTREACHED("failure in IsFirstNode");
    return false;
  }
  if (offset == 0)  // easy case, we are first dom child
    return true;
  if (!parent)  
    return true;
  
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
    return true;
  }
  while (j < offset)
  {
    childList->Item(j, getter_AddRefs(child));
    if (!IsEmptyTextContent(child)) 
      return false;
    j++;
  }
  return true;
}


bool
nsHTMLCopyEncoder::IsLastNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  int32_t offset,j;
  nsresult rv = GetNodeLocation(aNode, address_of(parent), &offset);
  if (NS_FAILED(rv)) 
  {
    NS_NOTREACHED("failure in IsLastNode");
    return false;
  }
  nsCOMPtr<nsINode> parentNode = do_QueryInterface(parent);
  if (!parentNode) {
    return true;
  }

  uint32_t numChildren = parentNode->Length();
  if (offset+1 == (int32_t)numChildren) // easy case, we are last dom child
    return true;
  // need to check if any nodes after us are really visible.
  // Mike wrote something for me along these lines in nsSelectionController,
  // but I don't think it's ready for use yet - revisit.
  // HACK: for now, simply consider all whitespace text nodes to be 
  // invisible formatting nodes.
  j = (int32_t)numChildren-1;
  nsCOMPtr<nsIDOMNodeList>childList;
  nsCOMPtr<nsIDOMNode> child;
  rv = parent->GetChildNodes(getter_AddRefs(childList));
  if (NS_FAILED(rv) || !childList) 
  {
    NS_NOTREACHED("failure in IsLastNode");
    return true;
  }
  while (j > offset)
  {
    childList->Item(j, getter_AddRefs(child));
    j--;
    if (IsMozBR(child))  // we ignore trailing moz BRs.  
      continue;
    if (!IsEmptyTextContent(child)) 
      return false;
  }
  return true;
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

int32_t
nsHTMLCopyEncoder::GetImmediateContextCount(const nsTArray<nsINode*>& aAncestorArray)
{
  int32_t i = aAncestorArray.Length(), j = 0;
  while (j < i) {
    nsINode *node = aAncestorArray.ElementAt(j);
    if (!node) {
      break;
    }
    nsCOMPtr<nsIContent> content(do_QueryInterface(node));
    if (!content || !content->IsHTML() || (content->Tag() != nsGkAtoms::tr    &&
                                           content->Tag() != nsGkAtoms::thead &&
                                           content->Tag() != nsGkAtoms::tbody &&
                                           content->Tag() != nsGkAtoms::tfoot &&
                                           content->Tag() != nsGkAtoms::table)) {
      break;
    }
    ++j;
  }
  return j;
}

