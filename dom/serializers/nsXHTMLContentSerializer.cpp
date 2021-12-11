/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an XHTML (not HTML!) DOM to an XHTML
 * string that could be parsed into more or less the original DOM.
 */

#include "nsXHTMLContentSerializer.h"

#include "mozilla/dom/Element.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsElementTable.h"
#include "nsNameSpaceManager.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsIDocumentEncoder.h"
#include "nsGkAtoms.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsIScriptElement.h"
#include "nsStubMutationObserver.h"
#include "nsAttrName.h"
#include "nsComputedDOMStyle.h"

using namespace mozilla;
using namespace mozilla::dom;

static const int32_t kLongLineLen = 128;

#define kXMLNS "xmlns"

nsresult NS_NewXHTMLContentSerializer(nsIContentSerializer** aSerializer) {
  RefPtr<nsXHTMLContentSerializer> it = new nsXHTMLContentSerializer();
  it.forget(aSerializer);
  return NS_OK;
}

nsXHTMLContentSerializer::nsXHTMLContentSerializer()
    : mIsHTMLSerializer(false),
      mIsCopying(false),
      mDisableEntityEncoding(0),
      mRewriteEncodingDeclaration(false),
      mIsFirstChildOfOL(false) {}

nsXHTMLContentSerializer::~nsXHTMLContentSerializer() {
  NS_ASSERTION(mOLStateStack.IsEmpty(), "Expected OL State stack to be empty");
}

NS_IMETHODIMP
nsXHTMLContentSerializer::Init(uint32_t aFlags, uint32_t aWrapColumn,
                               const Encoding* aEncoding, bool aIsCopying,
                               bool aRewriteEncodingDeclaration,
                               bool* aNeedsPreformatScanning,
                               nsAString& aOutput) {
  // The previous version of the HTML serializer did implicit wrapping
  // when there is no flags, so we keep wrapping in order to keep
  // compatibility with the existing calling code
  // XXXLJ perhaps should we remove this default settings later ?
  if (aFlags & nsIDocumentEncoder::OutputFormatted) {
    aFlags = aFlags | nsIDocumentEncoder::OutputWrap;
  }

  nsresult rv;
  rv = nsXMLContentSerializer::Init(aFlags, aWrapColumn, aEncoding, aIsCopying,
                                    aRewriteEncodingDeclaration,
                                    aNeedsPreformatScanning, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  mRewriteEncodingDeclaration = aRewriteEncodingDeclaration;
  mIsCopying = aIsCopying;
  mIsFirstChildOfOL = false;
  mInBody = 0;
  mDisableEntityEncoding = 0;
  mBodyOnly = (mFlags & nsIDocumentEncoder::OutputBodyOnly);

  return NS_OK;
}

// See if the string has any lines longer than longLineLen:
// if so, we presume formatting is wonky (e.g. the node has been edited)
// and we'd better rewrap the whole text node.
bool nsXHTMLContentSerializer::HasLongLines(const nsString& text,
                                            int32_t& aLastNewlineOffset) {
  uint32_t start = 0;
  uint32_t theLen = text.Length();
  bool rv = false;
  aLastNewlineOffset = kNotFound;
  for (start = 0; start < theLen;) {
    int32_t eol = text.FindChar('\n', start);
    if (eol < 0) {
      eol = text.Length();
    } else {
      aLastNewlineOffset = eol;
    }
    if (int32_t(eol - start) > kLongLineLen) rv = true;
    start = eol + 1;
  }
  return rv;
}

NS_IMETHODIMP
nsXHTMLContentSerializer::AppendText(nsIContent* aText, int32_t aStartOffset,
                                     int32_t aEndOffset) {
  NS_ENSURE_ARG(aText);
  NS_ENSURE_STATE(mOutput);

  nsAutoString data;
  nsresult rv;

  rv = AppendTextData(aText, aStartOffset, aEndOffset, data, true);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  if (mDoRaw || PreLevel() > 0) {
    NS_ENSURE_TRUE(AppendToStringConvertLF(data, *mOutput),
                   NS_ERROR_OUT_OF_MEMORY);
  } else if (mDoFormat) {
    NS_ENSURE_TRUE(AppendToStringFormatedWrapped(data, *mOutput),
                   NS_ERROR_OUT_OF_MEMORY);
  } else if (mDoWrap) {
    NS_ENSURE_TRUE(AppendToStringWrapped(data, *mOutput),
                   NS_ERROR_OUT_OF_MEMORY);
  } else {
    int32_t lastNewlineOffset = kNotFound;
    if (HasLongLines(data, lastNewlineOffset)) {
      // We have long lines, rewrap
      mDoWrap = true;
      bool result = AppendToStringWrapped(data, *mOutput);
      mDoWrap = false;
      NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);
    } else {
      NS_ENSURE_TRUE(AppendToStringConvertLF(data, *mOutput),
                     NS_ERROR_OUT_OF_MEMORY);
    }
  }

  return NS_OK;
}

bool nsXHTMLContentSerializer::SerializeAttributes(
    Element* aElement, Element* aOriginalElement, nsAString& aTagPrefix,
    const nsAString& aTagNamespaceURI, nsAtom* aTagName, nsAString& aStr,
    uint32_t aSkipAttr, bool aAddNSAttr) {
  nsresult rv;
  uint32_t index, count;
  nsAutoString prefixStr, uriStr, valueStr;
  nsAutoString xmlnsStr;
  xmlnsStr.AssignLiteral(kXMLNS);

  int32_t contentNamespaceID = aElement->GetNameSpaceID();

  MaybeSerializeIsValue(aElement, aStr);

  // this method is not called by nsHTMLContentSerializer
  // so we don't have to check HTML element, just XHTML

  if (mIsCopying && kNameSpaceID_XHTML == contentNamespaceID) {
    // Need to keep track of OL and LI elements in order to get ordinal number
    // for the LI.
    if (aTagName == nsGkAtoms::ol) {
      // We are copying and current node is an OL;
      // Store its start attribute value in olState->startVal.
      nsAutoString start;
      int32_t startAttrVal = 0;
      aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::start, start);
      if (!start.IsEmpty()) {
        nsresult rv = NS_OK;
        startAttrVal = start.ToInteger(&rv);
        // If OL has "start" attribute, first LI element has to start with that
        // value Therefore subtracting 1 as all the LI elements are incrementing
        // it before using it; In failure of ToInteger(), default StartAttrValue
        // to 0.
        if (NS_SUCCEEDED(rv))
          --startAttrVal;
        else
          startAttrVal = 0;
      }
      olState state(startAttrVal, true);
      mOLStateStack.AppendElement(state);
    } else if (aTagName == nsGkAtoms::li) {
      mIsFirstChildOfOL = IsFirstChildOfOL(aOriginalElement);
      if (mIsFirstChildOfOL) {
        // If OL is parent of this LI, serialize attributes in different manner.
        NS_ENSURE_TRUE(SerializeLIValueAttribute(aElement, aStr), false);
      }
    }
  }

  // If we had to add a new namespace declaration, serialize
  // and push it on the namespace stack
  if (aAddNSAttr) {
    if (aTagPrefix.IsEmpty()) {
      // Serialize default namespace decl
      NS_ENSURE_TRUE(
          SerializeAttr(u""_ns, xmlnsStr, aTagNamespaceURI, aStr, true), false);
    } else {
      // Serialize namespace decl
      NS_ENSURE_TRUE(
          SerializeAttr(xmlnsStr, aTagPrefix, aTagNamespaceURI, aStr, true),
          false);
    }
    PushNameSpaceDecl(aTagPrefix, aTagNamespaceURI, aOriginalElement);
  }

  count = aElement->GetAttrCount();

  // Now serialize each of the attributes
  // XXX Unfortunately we need a namespace manager to get
  // attribute URIs.
  for (index = 0; index < count; index++) {
    if (aSkipAttr == index) {
      continue;
    }

    dom::BorrowedAttrInfo info = aElement->GetAttrInfoAt(index);
    const nsAttrName* name = info.mName;

    int32_t namespaceID = name->NamespaceID();
    nsAtom* attrName = name->LocalName();
    nsAtom* attrPrefix = name->GetPrefix();

    // Filter out any attribute starting with [-|_]moz
    nsDependentAtomString attrNameStr(attrName);
    if (StringBeginsWith(attrNameStr, u"_moz"_ns) ||
        StringBeginsWith(attrNameStr, u"-moz"_ns)) {
      continue;
    }

    if (attrPrefix) {
      attrPrefix->ToString(prefixStr);
    } else {
      prefixStr.Truncate();
    }

    bool addNSAttr = false;
    if (kNameSpaceID_XMLNS != namespaceID) {
      nsNameSpaceManager::GetInstance()->GetNameSpaceURI(namespaceID, uriStr);
      addNSAttr = ConfirmPrefix(prefixStr, uriStr, aOriginalElement, true);
    }

    info.mValue->ToString(valueStr);

    nsDependentAtomString nameStr(attrName);
    bool isJS = false;

    if (kNameSpaceID_XHTML == contentNamespaceID) {
      if (mIsCopying && mIsFirstChildOfOL && (aTagName == nsGkAtoms::li) &&
          (attrName == nsGkAtoms::value)) {
        // This is handled separately in SerializeLIValueAttribute()
        continue;
      }

      isJS = IsJavaScript(aElement, attrName, namespaceID, valueStr);

      if (namespaceID == kNameSpaceID_None &&
          ((attrName == nsGkAtoms::href) || (attrName == nsGkAtoms::src))) {
        // Make all links absolute when converting only the selection:
        if (mFlags & nsIDocumentEncoder::OutputAbsoluteLinks) {
          // Would be nice to handle OBJECT tags,
          // but that gets more complicated since we have to
          // search the tag list for CODEBASE as well.
          // For now, just leave them relative.
          nsIURI* uri = aElement->GetBaseURI();
          if (uri) {
            nsAutoString absURI;
            rv = NS_MakeAbsoluteURI(absURI, valueStr, uri);
            if (NS_SUCCEEDED(rv)) {
              valueStr = absURI;
            }
          }
        }
      }

      if (mRewriteEncodingDeclaration && aTagName == nsGkAtoms::meta &&
          attrName == nsGkAtoms::content) {
        // If we're serializing a <meta http-equiv="content-type">,
        // use the proper value, rather than what's in the document.
        nsAutoString header;
        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, header);
        if (header.LowerCaseEqualsLiteral("content-type")) {
          valueStr =
              u"text/html; charset="_ns + NS_ConvertASCIItoUTF16(mCharset);
        }
      }

      // Expand shorthand attribute.
      if (namespaceID == kNameSpaceID_None &&
          IsShorthandAttr(attrName, aTagName) && valueStr.IsEmpty()) {
        valueStr = nameStr;
      }
    } else {
      isJS = IsJavaScript(aElement, attrName, namespaceID, valueStr);
    }

    NS_ENSURE_TRUE(SerializeAttr(prefixStr, nameStr, valueStr, aStr, !isJS),
                   false);

    if (addNSAttr) {
      NS_ASSERTION(!prefixStr.IsEmpty(),
                   "Namespaced attributes must have a prefix");
      NS_ENSURE_TRUE(SerializeAttr(xmlnsStr, prefixStr, uriStr, aStr, true),
                     false);
      PushNameSpaceDecl(prefixStr, uriStr, aOriginalElement);
    }
  }

  return true;
}

bool nsXHTMLContentSerializer::AfterElementStart(nsIContent* aContent,
                                                 nsIContent* aOriginalElement,
                                                 nsAString& aStr) {
  if (mRewriteEncodingDeclaration && aContent->IsHTMLElement(nsGkAtoms::head)) {
    // Check if there already are any content-type meta children.
    // If there are, they will be modified to use the correct charset.
    // If there aren't, we'll insert one here.
    bool hasMeta = false;
    for (nsIContent* child = aContent->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsHTMLElement(nsGkAtoms::meta) &&
          child->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::content)) {
        nsAutoString header;
        child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv,
                                    header);

        if (header.LowerCaseEqualsLiteral("content-type")) {
          hasMeta = true;
          break;
        }
      }
    }

    if (!hasMeta) {
      NS_ENSURE_TRUE(AppendNewLineToString(aStr), false);
      if (mDoFormat) {
        NS_ENSURE_TRUE(AppendIndentation(aStr), false);
      }
      NS_ENSURE_TRUE(
          AppendToString(u"<meta http-equiv=\"content-type\""_ns, aStr), false);
      NS_ENSURE_TRUE(AppendToString(u" content=\"text/html; charset="_ns, aStr),
                     false);
      NS_ENSURE_TRUE(AppendToString(NS_ConvertASCIItoUTF16(mCharset), aStr),
                     false);
      if (mIsHTMLSerializer) {
        NS_ENSURE_TRUE(AppendToString(u"\">"_ns, aStr), false);
      } else {
        NS_ENSURE_TRUE(AppendToString(u"\" />"_ns, aStr), false);
      }
    }
  }

  return true;
}

void nsXHTMLContentSerializer::AfterElementEnd(nsIContent* aContent,
                                               nsAString& aStr) {
  NS_ASSERTION(!mIsHTMLSerializer,
               "nsHTMLContentSerializer shouldn't call this method !");

  // this method is not called by nsHTMLContentSerializer
  // so we don't have to check HTML element, just XHTML
  if (aContent->IsHTMLElement(nsGkAtoms::body)) {
    --mInBody;
  }
}

NS_IMETHODIMP
nsXHTMLContentSerializer::AppendDocumentStart(Document* aDocument) {
  if (!mBodyOnly) {
    return nsXMLContentSerializer::AppendDocumentStart(aDocument);
  }

  return NS_OK;
}

bool nsXHTMLContentSerializer::CheckElementStart(Element* aElement,
                                                 bool& aForceFormat,
                                                 nsAString& aStr,
                                                 nsresult& aResult) {
  aResult = NS_OK;

  // The _moz_dirty attribute is emitted by the editor to
  // indicate that this element should be pretty printed
  // even if we're not in pretty printing mode
  aForceFormat = !(mFlags & nsIDocumentEncoder::OutputIgnoreMozDirty) &&
                 aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::mozdirty);

  if (aElement->IsHTMLElement(nsGkAtoms::br) &&
      (mFlags & nsIDocumentEncoder::OutputNoFormattingInPre) &&
      PreLevel() > 0) {
    aResult = AppendNewLineToString(aStr) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    return false;
  }

  if (aElement->IsHTMLElement(nsGkAtoms::body)) {
    ++mInBody;
  }

  return true;
}

bool nsXHTMLContentSerializer::CheckElementEnd(Element* aElement,
                                               Element* aOriginalElement,
                                               bool& aForceFormat,
                                               nsAString& aStr) {
  NS_ASSERTION(!mIsHTMLSerializer,
               "nsHTMLContentSerializer shouldn't call this method !");

  aForceFormat = !(mFlags & nsIDocumentEncoder::OutputIgnoreMozDirty) &&
                 aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::mozdirty);

  if (mIsCopying && aElement->IsHTMLElement(nsGkAtoms::ol)) {
    NS_ASSERTION((!mOLStateStack.IsEmpty()), "Cannot have an empty OL Stack");
    /* Though at this point we must always have an state to be deleted as all
       the OL opening tags are supposed to push an olState object to the stack*/
    if (!mOLStateStack.IsEmpty()) {
      mOLStateStack.RemoveLastElement();
    }
  }

  bool dummyFormat;
  return nsXMLContentSerializer::CheckElementEnd(aElement, aOriginalElement,
                                                 dummyFormat, aStr);
}

bool nsXHTMLContentSerializer::AppendAndTranslateEntities(
    const nsAString& aStr, nsAString& aOutputStr) {
  if (mBodyOnly && !mInBody) {
    return true;
  }

  if (mDisableEntityEncoding) {
    return aOutputStr.Append(aStr, fallible);
  }

  return nsXMLContentSerializer::AppendAndTranslateEntities(aStr, aOutputStr);
}

bool nsXHTMLContentSerializer::IsShorthandAttr(const nsAtom* aAttrName,
                                               const nsAtom* aElementName) {
  // checked
  if ((aAttrName == nsGkAtoms::checked) && (aElementName == nsGkAtoms::input)) {
    return true;
  }

  // compact
  if ((aAttrName == nsGkAtoms::compact) &&
      (aElementName == nsGkAtoms::dir || aElementName == nsGkAtoms::dl ||
       aElementName == nsGkAtoms::menu || aElementName == nsGkAtoms::ol ||
       aElementName == nsGkAtoms::ul)) {
    return true;
  }

  // declare
  if ((aAttrName == nsGkAtoms::declare) &&
      (aElementName == nsGkAtoms::object)) {
    return true;
  }

  // defer
  if ((aAttrName == nsGkAtoms::defer) && (aElementName == nsGkAtoms::script)) {
    return true;
  }

  // disabled
  if ((aAttrName == nsGkAtoms::disabled) &&
      (aElementName == nsGkAtoms::button || aElementName == nsGkAtoms::input ||
       aElementName == nsGkAtoms::optgroup ||
       aElementName == nsGkAtoms::option || aElementName == nsGkAtoms::select ||
       aElementName == nsGkAtoms::textarea)) {
    return true;
  }

  // ismap
  if ((aAttrName == nsGkAtoms::ismap) &&
      (aElementName == nsGkAtoms::img || aElementName == nsGkAtoms::input)) {
    return true;
  }

  // multiple
  if ((aAttrName == nsGkAtoms::multiple) &&
      (aElementName == nsGkAtoms::select)) {
    return true;
  }

  // noresize
  if ((aAttrName == nsGkAtoms::noresize) &&
      (aElementName == nsGkAtoms::frame)) {
    return true;
  }

  // noshade
  if ((aAttrName == nsGkAtoms::noshade) && (aElementName == nsGkAtoms::hr)) {
    return true;
  }

  // nowrap
  if ((aAttrName == nsGkAtoms::nowrap) &&
      (aElementName == nsGkAtoms::td || aElementName == nsGkAtoms::th)) {
    return true;
  }

  // readonly
  if ((aAttrName == nsGkAtoms::readonly) &&
      (aElementName == nsGkAtoms::input ||
       aElementName == nsGkAtoms::textarea)) {
    return true;
  }

  // selected
  if ((aAttrName == nsGkAtoms::selected) &&
      (aElementName == nsGkAtoms::option)) {
    return true;
  }

  // autoplay and controls
  if ((aElementName == nsGkAtoms::video || aElementName == nsGkAtoms::audio) &&
      (aAttrName == nsGkAtoms::autoplay || aAttrName == nsGkAtoms::muted ||
       aAttrName == nsGkAtoms::controls)) {
    return true;
  }

  return false;
}

bool nsXHTMLContentSerializer::LineBreakBeforeOpen(int32_t aNamespaceID,
                                                   nsAtom* aName) {
  if (aNamespaceID != kNameSpaceID_XHTML) {
    return mAddSpace;
  }

  if (aName == nsGkAtoms::title || aName == nsGkAtoms::meta ||
      aName == nsGkAtoms::link || aName == nsGkAtoms::style ||
      aName == nsGkAtoms::select || aName == nsGkAtoms::option ||
      aName == nsGkAtoms::script || aName == nsGkAtoms::html) {
    return true;
  }

  return nsHTMLElement::IsBlock(nsHTMLTags::CaseSensitiveAtomTagToId(aName));
}

bool nsXHTMLContentSerializer::LineBreakAfterOpen(int32_t aNamespaceID,
                                                  nsAtom* aName) {
  if (aNamespaceID != kNameSpaceID_XHTML) {
    return false;
  }

  if ((aName == nsGkAtoms::html) || (aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::body) || (aName == nsGkAtoms::ul) ||
      (aName == nsGkAtoms::ol) || (aName == nsGkAtoms::dl) ||
      (aName == nsGkAtoms::table) || (aName == nsGkAtoms::tbody) ||
      (aName == nsGkAtoms::tr) || (aName == nsGkAtoms::br) ||
      (aName == nsGkAtoms::meta) || (aName == nsGkAtoms::link) ||
      (aName == nsGkAtoms::script) || (aName == nsGkAtoms::select) ||
      (aName == nsGkAtoms::map) || (aName == nsGkAtoms::area) ||
      (aName == nsGkAtoms::style)) {
    return true;
  }

  return false;
}

bool nsXHTMLContentSerializer::LineBreakBeforeClose(int32_t aNamespaceID,
                                                    nsAtom* aName) {
  if (aNamespaceID != kNameSpaceID_XHTML) {
    return false;
  }

  if ((aName == nsGkAtoms::html) || (aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::body) || (aName == nsGkAtoms::ul) ||
      (aName == nsGkAtoms::ol) || (aName == nsGkAtoms::dl) ||
      (aName == nsGkAtoms::select) || (aName == nsGkAtoms::table) ||
      (aName == nsGkAtoms::tbody)) {
    return true;
  }
  return false;
}

bool nsXHTMLContentSerializer::LineBreakAfterClose(int32_t aNamespaceID,
                                                   nsAtom* aName) {
  if (aNamespaceID != kNameSpaceID_XHTML) {
    return false;
  }

  if ((aName == nsGkAtoms::html) || (aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::body) || (aName == nsGkAtoms::tr) ||
      (aName == nsGkAtoms::th) || (aName == nsGkAtoms::td) ||
      (aName == nsGkAtoms::title) || (aName == nsGkAtoms::dt) ||
      (aName == nsGkAtoms::dd) || (aName == nsGkAtoms::select) ||
      (aName == nsGkAtoms::option) || (aName == nsGkAtoms::map)) {
    return true;
  }

  return nsHTMLElement::IsBlock(nsHTMLTags::CaseSensitiveAtomTagToId(aName));
}

void nsXHTMLContentSerializer::MaybeEnterInPreContent(nsIContent* aNode) {
  if (!ShouldMaintainPreLevel() || !aNode->IsHTMLElement()) {
    return;
  }

  if (IsElementPreformatted(aNode) ||
      aNode->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::style,
                                 nsGkAtoms::noscript, nsGkAtoms::noframes)) {
    PreLevel()++;
  }
}

void nsXHTMLContentSerializer::MaybeLeaveFromPreContent(nsIContent* aNode) {
  if (!ShouldMaintainPreLevel() || !aNode->IsHTMLElement()) {
    return;
  }

  if (IsElementPreformatted(aNode) ||
      aNode->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::style,
                                 nsGkAtoms::noscript, nsGkAtoms::noframes)) {
    --PreLevel();
  }
}

bool nsXHTMLContentSerializer::IsElementPreformatted(nsIContent* aNode) {
  MOZ_ASSERT(ShouldMaintainPreLevel(),
             "We should not be calling this needlessly");

  if (!aNode->IsElement()) {
    return false;
  }
  RefPtr<ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(aNode->AsElement());
  if (computedStyle) {
    const nsStyleText* textStyle = computedStyle->StyleText();
    return textStyle->WhiteSpaceOrNewlineIsSignificant();
  }
  return false;
}

bool nsXHTMLContentSerializer::SerializeLIValueAttribute(nsIContent* aElement,
                                                         nsAString& aStr) {
  // We are copying and we are at the "first" LI node of OL in selected range.
  // It may not be the first LI child of OL but it's first in the selected
  // range. Note that we get into this condition only once per a OL.
  bool found = false;
  nsAutoString valueStr;

  olState state(0, false);

  if (!mOLStateStack.IsEmpty()) {
    state = mOLStateStack[mOLStateStack.Length() - 1];
    // isFirstListItem should be true only before the serialization of the
    // first item in the list.
    state.isFirstListItem = false;
    mOLStateStack[mOLStateStack.Length() - 1] = state;
  }

  int32_t startVal = state.startVal;
  int32_t offset = 0;

  // Traverse previous siblings until we find one with "value" attribute.
  // offset keeps track of how many previous siblings we had to traverse.
  nsIContent* currNode = aElement;
  while (currNode && !found) {
    if (currNode->IsHTMLElement(nsGkAtoms::li)) {
      currNode->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::value,
                                     valueStr);
      if (valueStr.IsEmpty()) {
        offset++;
      } else {
        found = true;
        nsresult rv = NS_OK;
        startVal = valueStr.ToInteger(&rv);
      }
    }
    currNode = currNode->GetPreviousSibling();
  }
  // If LI was not having "value", Set the "value" attribute for it.
  // Note that We are at the first LI in the selected range of OL.
  if (offset == 0 && found) {
    // offset = 0 => LI itself has the value attribute and we did not need to
    // traverse back. Just serialize value attribute like other tags.
    NS_ENSURE_TRUE(SerializeAttr(u""_ns, u"value"_ns, valueStr, aStr, false),
                   false);
  } else if (offset == 1 && !found) {
    /*(offset = 1 && !found) means either LI is the first child node of OL
    and LI is not having "value" attribute.
    In that case we would not like to set "value" attribute to reduce the
    changes.
    */
    // do nothing...
  } else if (offset > 0) {
    // Set value attribute.
    nsAutoString valueStr;

    // As serializer needs to use this valueAttr we are creating here,
    valueStr.AppendInt(startVal + offset);
    NS_ENSURE_TRUE(SerializeAttr(u""_ns, u"value"_ns, valueStr, aStr, false),
                   false);
  }

  return true;
}

bool nsXHTMLContentSerializer::IsFirstChildOfOL(nsIContent* aElement) {
  nsIContent* parent = aElement->GetParent();
  if (parent && parent->NodeName().LowerCaseEqualsLiteral("ol")) {
    if (!mOLStateStack.IsEmpty()) {
      olState state = mOLStateStack[mOLStateStack.Length() - 1];
      if (state.isFirstListItem) return true;
    }
  }

  return false;
}

bool nsXHTMLContentSerializer::HasNoChildren(nsIContent* aContent) {
  for (nsIContent* child = aContent->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (!child->IsText()) return false;

    if (child->TextLength()) return false;
  }

  return true;
}
