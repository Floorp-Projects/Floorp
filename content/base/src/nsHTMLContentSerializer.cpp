/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an HTML (not XHTML!) DOM to an HTML
 * string that could be parsed into more or less the original DOM.
 */

#include "nsHTMLContentSerializer.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsNameSpaceManager.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsIDocumentEncoder.h"
#include "nsGkAtoms.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsITextToSubURI.h"
#include "nsCRT.h"
#include "nsIParserService.h"
#include "nsContentUtils.h"
#include "nsLWBrkCIID.h"
#include "nsIScriptElement.h"
#include "nsAttrName.h"
#include "nsIDocShell.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "mozilla/dom/Element.h"
#include "nsParserConstants.h"

using namespace mozilla::dom;

nsresult NS_NewHTMLContentSerializer(nsIContentSerializer** aSerializer)
{
  nsHTMLContentSerializer* it = new nsHTMLContentSerializer();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aSerializer);
}

nsHTMLContentSerializer::nsHTMLContentSerializer()
{
    mIsHTMLSerializer = true;
}

nsHTMLContentSerializer::~nsHTMLContentSerializer()
{
}


NS_IMETHODIMP
nsHTMLContentSerializer::AppendDocumentStart(nsIDocument *aDocument,
                                             nsAString& aStr)
{
  return NS_OK;
}

void 
nsHTMLContentSerializer::SerializeHTMLAttributes(nsIContent* aContent,
                                                 nsIContent *aOriginalElement,
                                                 nsAString& aTagPrefix,
                                                 const nsAString& aTagNamespaceURI,
                                                 nsIAtom* aTagName,
                                                 int32_t aNamespace,
                                                 nsAString& aStr)
{
  int32_t count = aContent->GetAttrCount();
  if (!count)
    return;

  nsresult rv;
  nsAutoString valueStr;
  NS_NAMED_LITERAL_STRING(_mozStr, "_moz");

  for (int32_t index = count; index > 0;) {
    --index;
    const nsAttrName* name = aContent->GetAttrNameAt(index);
    int32_t namespaceID = name->NamespaceID();
    nsIAtom* attrName = name->LocalName();

    // Filter out any attribute starting with [-|_]moz
    nsDependentAtomString attrNameStr(attrName);
    if (StringBeginsWith(attrNameStr, NS_LITERAL_STRING("_moz")) ||
        StringBeginsWith(attrNameStr, NS_LITERAL_STRING("-moz"))) {
      continue;
    }
    aContent->GetAttr(namespaceID, attrName, valueStr);

    // 
    // Filter out special case of <br type="_moz"> or <br _moz*>,
    // used by the editor.  Bug 16988.  Yuck.
    //
    if (aTagName == nsGkAtoms::br && aNamespace == kNameSpaceID_XHTML &&
        attrName == nsGkAtoms::type && namespaceID == kNameSpaceID_None &&
        StringBeginsWith(valueStr, _mozStr)) {
      continue;
    }

    if (mIsCopying && mIsFirstChildOfOL &&
        aTagName == nsGkAtoms::li && aNamespace == kNameSpaceID_XHTML &&
        attrName == nsGkAtoms::value && namespaceID == kNameSpaceID_None){
      // This is handled separately in SerializeLIValueAttribute()
      continue;
    }
    bool isJS = IsJavaScript(aContent, attrName, namespaceID, valueStr);
    
    if (((attrName == nsGkAtoms::href &&
          (namespaceID == kNameSpaceID_None ||
           namespaceID == kNameSpaceID_XLink)) ||
         (attrName == nsGkAtoms::src && namespaceID == kNameSpaceID_None))) {
      // Make all links absolute when converting only the selection:
      if (mFlags & nsIDocumentEncoder::OutputAbsoluteLinks) {
        // Would be nice to handle OBJECT and APPLET tags,
        // but that gets more complicated since we have to
        // search the tag list for CODEBASE as well.
        // For now, just leave them relative.
        nsCOMPtr<nsIURI> uri = aContent->GetBaseURI();
        if (uri) {
          nsAutoString absURI;
          rv = NS_MakeAbsoluteURI(absURI, valueStr, uri);
          if (NS_SUCCEEDED(rv)) {
            valueStr = absURI;
          }
        }
      }
      // Need to escape URI.
      nsAutoString tempURI(valueStr);
      if (!isJS && NS_FAILED(EscapeURI(aContent, tempURI, valueStr)))
        valueStr = tempURI;
    }

    if (mRewriteEncodingDeclaration && aTagName == nsGkAtoms::meta &&
        aNamespace == kNameSpaceID_XHTML && attrName == nsGkAtoms::content
        && namespaceID == kNameSpaceID_None) {
      // If we're serializing a <meta http-equiv="content-type">,
      // use the proper value, rather than what's in the document.
      nsAutoString header;
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, header);
      if (header.LowerCaseEqualsLiteral("content-type")) {
        valueStr = NS_LITERAL_STRING("text/html; charset=") +
          NS_ConvertASCIItoUTF16(mCharset);
      }
    }

    nsDependentAtomString nameStr(attrName);
    nsAutoString prefix;
    if (namespaceID == kNameSpaceID_XML) {
      prefix.AssignLiteral("xml");
    } else if (namespaceID == kNameSpaceID_XLink) {
      prefix.AssignLiteral("xlink");
    }

    // Expand shorthand attribute.
    if (aNamespace == kNameSpaceID_XHTML &&
        namespaceID == kNameSpaceID_None &&
        IsShorthandAttr(attrName, aTagName) &&
        valueStr.IsEmpty()) {
      valueStr = nameStr;
    }
    SerializeAttr(prefix, nameStr, valueStr, aStr, !isJS);
  }
}

NS_IMETHODIMP
nsHTMLContentSerializer::AppendElementStart(Element* aElement,
                                            Element* aOriginalElement,
                                            nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsIContent* content = aElement;

  bool forceFormat = false;
  if (!CheckElementStart(content, forceFormat, aStr)) {
    return NS_OK;
  }

  nsIAtom *name = content->Tag();
  int32_t ns = content->GetNameSpaceID();

  bool lineBreakBeforeOpen = LineBreakBeforeOpen(ns, name);

  if ((mDoFormat || forceFormat) && !mPreLevel && !mDoRaw) {
    if (mColPos && lineBreakBeforeOpen) {
      AppendNewLineToString(aStr);
    }
    else {
      MaybeAddNewlineForRootNode(aStr);
    }
    if (!mColPos) {
      AppendIndentation(aStr);
    }
    else if (mAddSpace) {
      AppendToString(char16_t(' '), aStr);
      mAddSpace = false;
    }
  }
  else if (mAddSpace) {
    AppendToString(char16_t(' '), aStr);
    mAddSpace = false;
  }
  else {
    MaybeAddNewlineForRootNode(aStr);
  }
  // Always reset to avoid false newlines in case MaybeAddNewlineForRootNode wasn't
  // called
  mAddNewlineForRootNode = false;
  
  AppendToString(kLessThan, aStr);

  AppendToString(nsDependentAtomString(name), aStr);

  MaybeEnterInPreContent(content);

  // for block elements, we increase the indentation
  if ((mDoFormat || forceFormat) && !mPreLevel && !mDoRaw)
    IncrIndentation(name);

  // Need to keep track of OL and LI elements in order to get ordinal number 
  // for the LI.
  if (mIsCopying && name == nsGkAtoms::ol && ns == kNameSpaceID_XHTML){
    // We are copying and current node is an OL;
    // Store its start attribute value in olState->startVal.
    nsAutoString start;
    int32_t startAttrVal = 0;

    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::start, start);
    if (!start.IsEmpty()){
      nsresult rv = NS_OK;
      startAttrVal = start.ToInteger(&rv);
      //If OL has "start" attribute, first LI element has to start with that value
      //Therefore subtracting 1 as all the LI elements are incrementing it before using it;
      //In failure of ToInteger(), default StartAttrValue to 0.
      if (NS_SUCCEEDED(rv))
        startAttrVal--; 
      else
        startAttrVal = 0;
    }
    mOLStateStack.AppendElement(olState(startAttrVal, true));
  }

  if (mIsCopying && name == nsGkAtoms::li && ns == kNameSpaceID_XHTML) {
    mIsFirstChildOfOL = IsFirstChildOfOL(aOriginalElement);
    if (mIsFirstChildOfOL){
      // If OL is parent of this LI, serialize attributes in different manner.
      SerializeLIValueAttribute(aElement, aStr);
    }
  }

  // Even LI passed above have to go through this 
  // for serializing attributes other than "value".
  nsAutoString dummyPrefix;
  SerializeHTMLAttributes(content,
                          aOriginalElement,
                          dummyPrefix,
                          EmptyString(),
                          name,
                          ns,
                          aStr);

  AppendToString(kGreaterThan, aStr);

  if (ns == kNameSpaceID_XHTML &&
      (name == nsGkAtoms::script ||
       name == nsGkAtoms::style ||
       name == nsGkAtoms::noscript ||
       name == nsGkAtoms::noframes)) {
    ++mDisableEntityEncoding;
  }

  if ((mDoFormat || forceFormat) && !mPreLevel &&
    !mDoRaw && LineBreakAfterOpen(ns, name)) {
    AppendNewLineToString(aStr);
  }

  AfterElementStart(content, aOriginalElement, aStr);

  return NS_OK;
}
  
NS_IMETHODIMP 
nsHTMLContentSerializer::AppendElementEnd(Element* aElement,
                                          nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsIContent* content = aElement;

  nsIAtom *name = content->Tag();
  int32_t ns = content->GetNameSpaceID();

  if (ns == kNameSpaceID_XHTML &&
      (name == nsGkAtoms::script ||
       name == nsGkAtoms::style ||
       name == nsGkAtoms::noscript ||
       name == nsGkAtoms::noframes)) {
    --mDisableEntityEncoding;
  }

  bool forceFormat = !(mFlags & nsIDocumentEncoder::OutputIgnoreMozDirty) &&
                     content->HasAttr(kNameSpaceID_None, nsGkAtoms::mozdirty);

  if ((mDoFormat || forceFormat) && !mPreLevel && !mDoRaw) {
    DecrIndentation(name);
  }

  if (name == nsGkAtoms::script) {
    nsCOMPtr<nsIScriptElement> script = do_QueryInterface(aElement);

    if (script && script->IsMalformed()) {
      // We're looking at a malformed script tag. This means that the end tag
      // was missing in the source. Imitate that here by not serializing the end
      // tag.
      --mPreLevel;
      return NS_OK;
    }
  }
  else if (mIsCopying && name == nsGkAtoms::ol && ns == kNameSpaceID_XHTML) {
    NS_ASSERTION((!mOLStateStack.IsEmpty()), "Cannot have an empty OL Stack");
    /* Though at this point we must always have an state to be deleted as all 
    the OL opening tags are supposed to push an olState object to the stack*/
    if (!mOLStateStack.IsEmpty()) {
      mOLStateStack.RemoveElementAt(mOLStateStack.Length() -1);
    }
  }
  
  if (ns == kNameSpaceID_XHTML) {
    nsIParserService* parserService = nsContentUtils::GetParserService();

    if (parserService) {
      bool isContainer;

      parserService->
        IsContainer(parserService->HTMLCaseSensitiveAtomTagToId(name),
                    isContainer);
      if (!isContainer) {
        return NS_OK;
      }
    }
  }

  if ((mDoFormat || forceFormat) && !mPreLevel && !mDoRaw) {

    bool lineBreakBeforeClose = LineBreakBeforeClose(ns, name);

    if (mColPos && lineBreakBeforeClose) {
      AppendNewLineToString(aStr);
    }
    if (!mColPos) {
      AppendIndentation(aStr);
    }
    else if (mAddSpace) {
      AppendToString(char16_t(' '), aStr);
      mAddSpace = false;
    }
  }
  else if (mAddSpace) {
    AppendToString(char16_t(' '), aStr);
    mAddSpace = false;
  }

  AppendToString(kEndTag, aStr);
  AppendToString(nsDependentAtomString(name), aStr);
  AppendToString(kGreaterThan, aStr);

  MaybeLeaveFromPreContent(content);

  if ((mDoFormat || forceFormat) && !mPreLevel
      && !mDoRaw && LineBreakAfterClose(ns, name)) {
    AppendNewLineToString(aStr);
  }
  else {
    MaybeFlagNewlineForRootNode(aElement);
  }

  if (name == nsGkAtoms::body && ns == kNameSpaceID_XHTML) {
    --mInBody;
  }

  return NS_OK;
}

static const uint16_t kValNBSP = 160;
static const char* kEntities[] = {
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "&amp;", nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "&lt;", nullptr, "&gt;", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "&nbsp;"
};

static const char* kAttrEntities[] = {
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, "&quot;", nullptr, nullptr, nullptr, "&amp;", nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "&lt;", nullptr, "&gt;", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "&nbsp;"
};

uint32_t FindNextBasicEntity(const nsAString& aStr,
                             const uint32_t aLen,
                             uint32_t aIndex,
                             const char** aEntityTable,
                             const char** aEntity)
{
  for (; aIndex < aLen; ++aIndex) {
    // for each character in this chunk, check if it
    // needs to be replaced
    char16_t val = aStr[aIndex];
    if (val <= kValNBSP && aEntityTable[val]) {
      *aEntity = aEntityTable[val];
      return aIndex;
    }
  }
  return aIndex;
}

void
nsHTMLContentSerializer::AppendAndTranslateEntities(const nsAString& aStr,
                                                     nsAString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  if (mDisableEntityEncoding) {
    aOutputStr.Append(aStr);
    return;
  }

  bool nonBasicEntities =
    !!(mFlags & (nsIDocumentEncoder::OutputEncodeLatin1Entities |
                 nsIDocumentEncoder::OutputEncodeHTMLEntities   |
                 nsIDocumentEncoder::OutputEncodeW3CEntities));

  if (!nonBasicEntities &&
      (mFlags & (nsIDocumentEncoder::OutputEncodeBasicEntities))) {
    const char **entityTable = mInAttribute ? kAttrEntities : kEntities;
    uint32_t start = 0;
    const uint32_t len = aStr.Length();
    for (uint32_t i = 0; i < len; ++i) {
      const char* entity = nullptr;
      i = FindNextBasicEntity(aStr, len, i, entityTable, &entity);
      uint32_t normalTextLen = i - start; 
      if (normalTextLen) {
        aOutputStr.Append(Substring(aStr, start, normalTextLen));
      }
      if (entity) {
        aOutputStr.AppendASCII(entity);
        start = i + 1;
      }
    }
    return;
  } else if (nonBasicEntities) {
    nsIParserService* parserService = nsContentUtils::GetParserService();

    if (!parserService) {
      NS_ERROR("Can't get parser service");
      return;
    }

    nsReadingIterator<char16_t> done_reading;
    aStr.EndReading(done_reading);

    // for each chunk of |aString|...
    uint32_t advanceLength = 0;
    nsReadingIterator<char16_t> iter;

    const char **entityTable = mInAttribute ? kAttrEntities : kEntities;
    nsAutoCString entityReplacement;

    for (aStr.BeginReading(iter);
         iter != done_reading;
         iter.advance(int32_t(advanceLength))) {
      uint32_t fragmentLength = iter.size_forward();
      uint32_t lengthReplaced = 0; // the number of UTF-16 codepoints
                                    //  replaced by a particular entity
      const char16_t* c = iter.get();
      const char16_t* fragmentStart = c;
      const char16_t* fragmentEnd = c + fragmentLength;
      const char* entityText = nullptr;
      const char* fullConstEntityText = nullptr;
      char* fullEntityText = nullptr;

      advanceLength = 0;
      // for each character in this chunk, check if it
      // needs to be replaced
      for (; c < fragmentEnd; c++, advanceLength++) {
        char16_t val = *c;
        if (val <= kValNBSP && entityTable[val]) {
          fullConstEntityText = entityTable[val];
          break;
        } else if (val > 127 &&
                  ((val < 256 &&
                    mFlags & nsIDocumentEncoder::OutputEncodeLatin1Entities) ||
                    mFlags & nsIDocumentEncoder::OutputEncodeHTMLEntities)) {
          entityReplacement.Truncate();
          parserService->HTMLConvertUnicodeToEntity(val, entityReplacement);

          if (!entityReplacement.IsEmpty()) {
            entityText = entityReplacement.get();
            break;
          }
        }
        else if (val > 127 &&
                  mFlags & nsIDocumentEncoder::OutputEncodeW3CEntities &&
                  mEntityConverter) {
          if (NS_IS_HIGH_SURROGATE(val) &&
              c + 1 < fragmentEnd &&
              NS_IS_LOW_SURROGATE(*(c + 1))) {
            uint32_t valUTF32 = SURROGATE_TO_UCS4(val, *(++c));
            if (NS_SUCCEEDED(mEntityConverter->ConvertUTF32ToEntity(valUTF32,
                              nsIEntityConverter::entityW3C, &fullEntityText))) {
              lengthReplaced = 2;
              break;
            }
            else {
              advanceLength++;
            }
          }
          else if (NS_SUCCEEDED(mEntityConverter->ConvertToEntity(val,
                                nsIEntityConverter::entityW3C, 
                                &fullEntityText))) {
            lengthReplaced = 1;
            break;
          }
        }
      }

      aOutputStr.Append(fragmentStart, advanceLength);
      if (entityText) {
        aOutputStr.Append(char16_t('&'));
        AppendASCIItoUTF16(entityText, aOutputStr);
        aOutputStr.Append(char16_t(';'));
        advanceLength++;
      }
      else if (fullConstEntityText) {
        aOutputStr.AppendASCII(fullConstEntityText);
        ++advanceLength;
      }
      // if it comes from nsIEntityConverter, it already has '&' and ';'
      else if (fullEntityText) {
        AppendASCIItoUTF16(fullEntityText, aOutputStr);
        nsMemory::Free(fullEntityText);
        advanceLength += lengthReplaced;
      }
    }
  } else {
    nsXMLContentSerializer::AppendAndTranslateEntities(aStr, aOutputStr);
  }
}
