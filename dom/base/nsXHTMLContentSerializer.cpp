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
#include "nsCRT.h"
#include "nsIParserService.h"
#include "nsContentUtils.h"
#include "nsLWBrkCIID.h"
#include "nsIScriptElement.h"
#include "nsStubMutationObserver.h"
#include "nsAttrName.h"
#include "nsComputedDOMStyle.h"
#include "mozilla/dom/Element.h"

static const int32_t kLongLineLen = 128;

#define kXMLNS "xmlns"

nsresult
NS_NewXHTMLContentSerializer(nsIContentSerializer** aSerializer)
{
  RefPtr<nsXHTMLContentSerializer> it = new nsXHTMLContentSerializer();
  it.forget(aSerializer);
  return NS_OK;
}

nsXHTMLContentSerializer::nsXHTMLContentSerializer()
  : mIsHTMLSerializer(false)
{
}

nsXHTMLContentSerializer::~nsXHTMLContentSerializer()
{
  NS_ASSERTION(mOLStateStack.IsEmpty(), "Expected OL State stack to be empty");
}

NS_IMETHODIMP
nsXHTMLContentSerializer::Init(uint32_t aFlags, uint32_t aWrapColumn,
                              const char* aCharSet, bool aIsCopying,
                              bool aRewriteEncodingDeclaration)
{
  // The previous version of the HTML serializer did implicit wrapping
  // when there is no flags, so we keep wrapping in order to keep
  // compatibility with the existing calling code
  // XXXLJ perhaps should we remove this default settings later ?
  if (aFlags & nsIDocumentEncoder::OutputFormatted ) {
      aFlags = aFlags | nsIDocumentEncoder::OutputWrap;
  }

  nsresult rv;
  rv = nsXMLContentSerializer::Init(aFlags, aWrapColumn, aCharSet, aIsCopying, aRewriteEncodingDeclaration);
  NS_ENSURE_SUCCESS(rv, rv);

  mRewriteEncodingDeclaration = aRewriteEncodingDeclaration;
  mIsCopying = aIsCopying;
  mIsFirstChildOfOL = false;
  mInBody = 0;
  mDisableEntityEncoding = 0;
  mBodyOnly = (mFlags & nsIDocumentEncoder::OutputBodyOnly) ? true
                                                            : false;

  // set up entity converter if we are going to need it
  if (mFlags & nsIDocumentEncoder::OutputEncodeW3CEntities) {
    mEntityConverter = do_CreateInstance(NS_ENTITYCONVERTER_CONTRACTID);
  }
  return NS_OK;
}


// See if the string has any lines longer than longLineLen:
// if so, we presume formatting is wonky (e.g. the node has been edited)
// and we'd better rewrap the whole text node.
bool
nsXHTMLContentSerializer::HasLongLines(const nsString& text, int32_t& aLastNewlineOffset)
{
  uint32_t start=0;
  uint32_t theLen = text.Length();
  bool rv = false;
  aLastNewlineOffset = kNotFound;
  for (start = 0; start < theLen; ) {
    int32_t eol = text.FindChar('\n', start);
    if (eol < 0) {
      eol = text.Length();
    }
    else {
      aLastNewlineOffset = eol;
    }
    if (int32_t(eol - start) > kLongLineLen)
      rv = true;
    start = eol + 1;
  }
  return rv;
}

NS_IMETHODIMP
nsXHTMLContentSerializer::AppendText(nsIContent* aText,
                                     int32_t aStartOffset,
                                     int32_t aEndOffset,
                                     nsAString& aStr)
{
  NS_ENSURE_ARG(aText);

  nsAutoString data;
  nsresult rv;

  rv = AppendTextData(aText, aStartOffset, aEndOffset, data, true);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if (mDoRaw || PreLevel() > 0) {
    NS_ENSURE_TRUE(AppendToStringConvertLF(data, aStr), NS_ERROR_OUT_OF_MEMORY);
  }
  else if (mDoFormat) {
    NS_ENSURE_TRUE(AppendToStringFormatedWrapped(data, aStr), NS_ERROR_OUT_OF_MEMORY);
  }
  else if (mDoWrap) {
    NS_ENSURE_TRUE(AppendToStringWrapped(data, aStr), NS_ERROR_OUT_OF_MEMORY);
  }
  else {
    int32_t lastNewlineOffset = kNotFound;
    if (HasLongLines(data, lastNewlineOffset)) {
      // We have long lines, rewrap
      mDoWrap = true;
      bool result = AppendToStringWrapped(data, aStr);
      mDoWrap = false;
      NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
      NS_ENSURE_TRUE(AppendToStringConvertLF(data, aStr), NS_ERROR_OUT_OF_MEMORY);
    }
  }

  return NS_OK;
}

bool
nsXHTMLContentSerializer::SerializeAttributes(nsIContent* aContent,
                                              nsIContent *aOriginalElement,
                                              nsAString& aTagPrefix,
                                              const nsAString& aTagNamespaceURI,
                                              nsIAtom* aTagName,
                                              nsAString& aStr,
                                              uint32_t aSkipAttr,
                                              bool aAddNSAttr)
{
  nsresult rv;
  uint32_t index, count;
  nsAutoString prefixStr, uriStr, valueStr;
  nsAutoString xmlnsStr;
  xmlnsStr.AssignLiteral(kXMLNS);

  int32_t contentNamespaceID = aContent->GetNameSpaceID();

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
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::start, start);
      if (!start.IsEmpty()) {
        nsresult rv = NS_OK;
        startAttrVal = start.ToInteger(&rv);
        //If OL has "start" attribute, first LI element has to start with that value
        //Therefore subtracting 1 as all the LI elements are incrementing it before using it;
        //In failure of ToInteger(), default StartAttrValue to 0.
        if (NS_SUCCEEDED(rv))
          --startAttrVal;
        else
          startAttrVal = 0;
      }
      olState state (startAttrVal, true);
      mOLStateStack.AppendElement(state);
    }
    else if (aTagName == nsGkAtoms::li) {
      mIsFirstChildOfOL = IsFirstChildOfOL(aOriginalElement);
      if (mIsFirstChildOfOL) {
        // If OL is parent of this LI, serialize attributes in different manner.
        NS_ENSURE_TRUE(SerializeLIValueAttribute(aContent, aStr), false);
      }
    }
  }

  // If we had to add a new namespace declaration, serialize
  // and push it on the namespace stack
  if (aAddNSAttr) {
    if (aTagPrefix.IsEmpty()) {
      // Serialize default namespace decl
      NS_ENSURE_TRUE(SerializeAttr(EmptyString(), xmlnsStr,
                                   aTagNamespaceURI,
                                   aStr, true), false);
    } else {
      // Serialize namespace decl
      NS_ENSURE_TRUE(SerializeAttr(xmlnsStr, aTagPrefix,
                                   aTagNamespaceURI,
                                   aStr, true), false);
    }
    PushNameSpaceDecl(aTagPrefix, aTagNamespaceURI, aOriginalElement);
  }

  NS_NAMED_LITERAL_STRING(_mozStr, "_moz");

  count = aContent->GetAttrCount();

  // Now serialize each of the attributes
  // XXX Unfortunately we need a namespace manager to get
  // attribute URIs.
  for (index = 0; index < count; index++) {

    if (aSkipAttr == index) {
        continue;
    }

    mozilla::dom::BorrowedAttrInfo info = aContent->GetAttrInfoAt(index);
    const nsAttrName* name = info.mName;

    int32_t namespaceID = name->NamespaceID();
    nsIAtom* attrName = name->LocalName();
    nsIAtom* attrPrefix = name->GetPrefix();

    // Filter out any attribute starting with [-|_]moz
    nsDependentAtomString attrNameStr(attrName);
    if (StringBeginsWith(attrNameStr, NS_LITERAL_STRING("_moz")) ||
        StringBeginsWith(attrNameStr, NS_LITERAL_STRING("-moz"))) {
      continue;
    }

    if (attrPrefix) {
      attrPrefix->ToString(prefixStr);
    }
    else {
      prefixStr.Truncate();
    }

    bool addNSAttr = false;
    if (kNameSpaceID_XMLNS != namespaceID) {
      nsContentUtils::NameSpaceManager()->GetNameSpaceURI(namespaceID, uriStr);
      addNSAttr = ConfirmPrefix(prefixStr, uriStr, aOriginalElement, true);
    }

    info.mValue->ToString(valueStr);

    nsDependentAtomString nameStr(attrName);
    bool isJS = false;

    if (kNameSpaceID_XHTML == contentNamespaceID) {
      //
      // Filter out special case of <br type="_moz"> or <br _moz*>,
      // used by the editor.  Bug 16988.  Yuck.
      //
      if (namespaceID == kNameSpaceID_None && aTagName == nsGkAtoms::br && attrName == nsGkAtoms::type
          && StringBeginsWith(valueStr, _mozStr)) {
        continue;
      }

      if (mIsCopying && mIsFirstChildOfOL && (aTagName == nsGkAtoms::li)
          && (attrName == nsGkAtoms::value)) {
        // This is handled separately in SerializeLIValueAttribute()
        continue;
      }

      isJS = IsJavaScript(aContent, attrName, namespaceID, valueStr);

      if (namespaceID == kNameSpaceID_None &&
          ((attrName == nsGkAtoms::href) ||
          (attrName == nsGkAtoms::src))) {
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
      }

      if (mRewriteEncodingDeclaration && aTagName == nsGkAtoms::meta &&
          attrName == nsGkAtoms::content) {
        // If we're serializing a <meta http-equiv="content-type">,
        // use the proper value, rather than what's in the document.
        nsAutoString header;
        aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, header);
        if (header.LowerCaseEqualsLiteral("content-type")) {
          valueStr = NS_LITERAL_STRING("text/html; charset=") +
            NS_ConvertASCIItoUTF16(mCharset);
        }
      }

      // Expand shorthand attribute.
      if (namespaceID == kNameSpaceID_None && IsShorthandAttr(attrName, aTagName) && valueStr.IsEmpty()) {
        valueStr = nameStr;
      }
    }
    else {
      isJS = IsJavaScript(aContent, attrName, namespaceID, valueStr);
    }

    NS_ENSURE_TRUE(SerializeAttr(prefixStr, nameStr, valueStr, aStr, !isJS), false);

    if (addNSAttr) {
      NS_ASSERTION(!prefixStr.IsEmpty(),
                   "Namespaced attributes must have a prefix");
      NS_ENSURE_TRUE(SerializeAttr(xmlnsStr, prefixStr, uriStr, aStr, true), false);
      PushNameSpaceDecl(prefixStr, uriStr, aOriginalElement);
    }
  }

  return true;
}

bool
nsXHTMLContentSerializer::AfterElementStart(nsIContent* aContent,
                                            nsIContent* aOriginalElement,
                                            nsAString& aStr)
{
  if (mRewriteEncodingDeclaration &&
      aContent->IsHTMLElement(nsGkAtoms::head)) {

    // Check if there already are any content-type meta children.
    // If there are, they will be modified to use the correct charset.
    // If there aren't, we'll insert one here.
    bool hasMeta = false;
    for (nsIContent* child = aContent->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      if (child->IsHTMLElement(nsGkAtoms::meta) &&
          child->HasAttr(kNameSpaceID_None, nsGkAtoms::content)) {
        nsAutoString header;
        child->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, header);

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
      NS_ENSURE_TRUE(AppendToString(NS_LITERAL_STRING("<meta http-equiv=\"content-type\""), aStr), false);
      NS_ENSURE_TRUE(AppendToString(NS_LITERAL_STRING(" content=\"text/html; charset="), aStr), false);
      NS_ENSURE_TRUE(AppendToString(NS_ConvertASCIItoUTF16(mCharset), aStr), false);
      if (mIsHTMLSerializer) {
        NS_ENSURE_TRUE(AppendToString(NS_LITERAL_STRING("\">"), aStr), false);
      } else {
        NS_ENSURE_TRUE(AppendToString(NS_LITERAL_STRING("\" />"), aStr), false);
      }
    }
  }

  return true;
}

void
nsXHTMLContentSerializer::AfterElementEnd(nsIContent * aContent,
                                          nsAString& aStr)
{
  NS_ASSERTION(!mIsHTMLSerializer, "nsHTMLContentSerializer shouldn't call this method !");

  // this method is not called by nsHTMLContentSerializer
  // so we don't have to check HTML element, just XHTML
  if (aContent->IsHTMLElement(nsGkAtoms::body)) {
    --mInBody;
  }
}


NS_IMETHODIMP
nsXHTMLContentSerializer::AppendDocumentStart(nsIDocument *aDocument,
                                              nsAString& aStr)
{
  if (!mBodyOnly)
    return nsXMLContentSerializer::AppendDocumentStart(aDocument, aStr);

  return NS_OK;
}

bool
nsXHTMLContentSerializer::CheckElementStart(nsIContent * aContent,
                                            bool & aForceFormat,
                                            nsAString& aStr,
                                            nsresult& aResult)
{
  aResult = NS_OK;

  // The _moz_dirty attribute is emitted by the editor to
  // indicate that this element should be pretty printed
  // even if we're not in pretty printing mode
  aForceFormat = !(mFlags & nsIDocumentEncoder::OutputIgnoreMozDirty) &&
                 aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::mozdirty);

  if (aContent->IsHTMLElement(nsGkAtoms::br) &&
      (mFlags & nsIDocumentEncoder::OutputNoFormattingInPre) &&
      PreLevel() > 0) {
    aResult = AppendNewLineToString(aStr) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    return false;
  }

  if (aContent->IsHTMLElement(nsGkAtoms::body)) {
    ++mInBody;
  }

  return true;
}

bool
nsXHTMLContentSerializer::CheckElementEnd(mozilla::dom::Element* aElement,
                                          bool& aForceFormat,
                                          nsAString& aStr)
{
  NS_ASSERTION(!mIsHTMLSerializer, "nsHTMLContentSerializer shouldn't call this method !");

  aForceFormat = !(mFlags & nsIDocumentEncoder::OutputIgnoreMozDirty) &&
                 aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::mozdirty);

  if (mIsCopying && aElement->IsHTMLElement(nsGkAtoms::ol)) {
    NS_ASSERTION((!mOLStateStack.IsEmpty()), "Cannot have an empty OL Stack");
    /* Though at this point we must always have an state to be deleted as all
       the OL opening tags are supposed to push an olState object to the stack*/
    if (!mOLStateStack.IsEmpty()) {
        mOLStateStack.RemoveElementAt(mOLStateStack.Length() -1);
    }
  }

  bool dummyFormat;
  return nsXMLContentSerializer::CheckElementEnd(aElement, dummyFormat, aStr);
}

bool
nsXHTMLContentSerializer::AppendAndTranslateEntities(const nsAString& aStr,
                                                     nsAString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return true;
  }

  if (mDisableEntityEncoding) {
    return aOutputStr.Append(aStr, mozilla::fallible);
  }

  return nsXMLContentSerializer::AppendAndTranslateEntities(aStr, aOutputStr);
}

bool
nsXHTMLContentSerializer::IsShorthandAttr(const nsIAtom* aAttrName,
                                          const nsIAtom* aElementName)
{
  // checked
  if ((aAttrName == nsGkAtoms::checked) &&
      (aElementName == nsGkAtoms::input)) {
    return true;
  }

  // compact
  if ((aAttrName == nsGkAtoms::compact) &&
      (aElementName == nsGkAtoms::dir ||
       aElementName == nsGkAtoms::dl ||
       aElementName == nsGkAtoms::menu ||
       aElementName == nsGkAtoms::ol ||
       aElementName == nsGkAtoms::ul)) {
    return true;
  }

  // declare
  if ((aAttrName == nsGkAtoms::declare) &&
      (aElementName == nsGkAtoms::object)) {
    return true;
  }

  // defer
  if ((aAttrName == nsGkAtoms::defer) &&
      (aElementName == nsGkAtoms::script)) {
    return true;
  }

  // disabled
  if ((aAttrName == nsGkAtoms::disabled) &&
      (aElementName == nsGkAtoms::button ||
       aElementName == nsGkAtoms::input ||
       aElementName == nsGkAtoms::optgroup ||
       aElementName == nsGkAtoms::option ||
       aElementName == nsGkAtoms::select ||
       aElementName == nsGkAtoms::textarea)) {
    return true;
  }

  // ismap
  if ((aAttrName == nsGkAtoms::ismap) &&
      (aElementName == nsGkAtoms::img ||
       aElementName == nsGkAtoms::input)) {
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
  if ((aAttrName == nsGkAtoms::noshade) &&
      (aElementName == nsGkAtoms::hr)) {
    return true;
  }

  // nowrap
  if ((aAttrName == nsGkAtoms::nowrap) &&
      (aElementName == nsGkAtoms::td ||
       aElementName == nsGkAtoms::th)) {
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

bool
nsXHTMLContentSerializer::LineBreakBeforeOpen(int32_t aNamespaceID, nsIAtom* aName)
{

  if (aNamespaceID != kNameSpaceID_XHTML) {
    return mAddSpace;
  }

  if (aName == nsGkAtoms::title ||
      aName == nsGkAtoms::meta  ||
      aName == nsGkAtoms::link  ||
      aName == nsGkAtoms::style ||
      aName == nsGkAtoms::select ||
      aName == nsGkAtoms::option ||
      aName == nsGkAtoms::script ||
      aName == nsGkAtoms::html) {
    return true;
  }
  else {
    nsIParserService* parserService = nsContentUtils::GetParserService();

    if (parserService) {
      bool res;
      parserService->
        IsBlock(parserService->HTMLCaseSensitiveAtomTagToId(aName), res);
      return res;
    }
  }

  return mAddSpace;
}

bool
nsXHTMLContentSerializer::LineBreakAfterOpen(int32_t aNamespaceID, nsIAtom* aName)
{

  if (aNamespaceID != kNameSpaceID_XHTML) {
    return false;
  }

  if ((aName == nsGkAtoms::html) ||
      (aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::body) ||
      (aName == nsGkAtoms::ul) ||
      (aName == nsGkAtoms::ol) ||
      (aName == nsGkAtoms::dl) ||
      (aName == nsGkAtoms::table) ||
      (aName == nsGkAtoms::tbody) ||
      (aName == nsGkAtoms::tr) ||
      (aName == nsGkAtoms::br) ||
      (aName == nsGkAtoms::meta) ||
      (aName == nsGkAtoms::link) ||
      (aName == nsGkAtoms::script) ||
      (aName == nsGkAtoms::select) ||
      (aName == nsGkAtoms::map) ||
      (aName == nsGkAtoms::area) ||
      (aName == nsGkAtoms::style)) {
    return true;
  }

  return false;
}

bool
nsXHTMLContentSerializer::LineBreakBeforeClose(int32_t aNamespaceID, nsIAtom* aName)
{

  if (aNamespaceID != kNameSpaceID_XHTML) {
    return false;
  }

  if ((aName == nsGkAtoms::html) ||
      (aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::body) ||
      (aName == nsGkAtoms::ul) ||
      (aName == nsGkAtoms::ol) ||
      (aName == nsGkAtoms::dl) ||
      (aName == nsGkAtoms::select) ||
      (aName == nsGkAtoms::table) ||
      (aName == nsGkAtoms::tbody)) {
    return true;
  }
  return false;
}

bool
nsXHTMLContentSerializer::LineBreakAfterClose(int32_t aNamespaceID, nsIAtom* aName)
{

  if (aNamespaceID != kNameSpaceID_XHTML) {
    return false;
  }

  if ((aName == nsGkAtoms::html) ||
      (aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::body) ||
      (aName == nsGkAtoms::tr) ||
      (aName == nsGkAtoms::th) ||
      (aName == nsGkAtoms::td) ||
      (aName == nsGkAtoms::pre) ||
      (aName == nsGkAtoms::title) ||
      (aName == nsGkAtoms::li) ||
      (aName == nsGkAtoms::dt) ||
      (aName == nsGkAtoms::dd) ||
      (aName == nsGkAtoms::blockquote) ||
      (aName == nsGkAtoms::select) ||
      (aName == nsGkAtoms::option) ||
      (aName == nsGkAtoms::p) ||
      (aName == nsGkAtoms::map) ||
      (aName == nsGkAtoms::div)) {
    return true;
  }
  else {
    nsIParserService* parserService = nsContentUtils::GetParserService();

    if (parserService) {
      bool res;
      parserService->
        IsBlock(parserService->HTMLCaseSensitiveAtomTagToId(aName), res);
      return res;
    }
  }

  return false;
}


void
nsXHTMLContentSerializer::MaybeEnterInPreContent(nsIContent* aNode)
{
  if (!ShouldMaintainPreLevel() ||
      !aNode->IsHTMLElement()) {
    return;
  }

  if (IsElementPreformatted(aNode) ||
      aNode->IsAnyOfHTMLElements(nsGkAtoms::script,
                                 nsGkAtoms::style,
                                 nsGkAtoms::noscript,
                                 nsGkAtoms::noframes)) {
    PreLevel()++;
  }
}

void
nsXHTMLContentSerializer::MaybeLeaveFromPreContent(nsIContent* aNode)
{
  if (!ShouldMaintainPreLevel() ||
      !aNode->IsHTMLElement()) {
    return;
  }

  if (IsElementPreformatted(aNode) ||
      aNode->IsAnyOfHTMLElements(nsGkAtoms::script,
                                 nsGkAtoms::style,
                                 nsGkAtoms::noscript,
                                 nsGkAtoms::noframes)) {
    --PreLevel();
  }
}

bool
nsXHTMLContentSerializer::IsElementPreformatted(nsIContent* aNode)
{
  MOZ_ASSERT(ShouldMaintainPreLevel(), "We should not be calling this needlessly");

  if (!aNode->IsElement()) {
    return false;
  }
  RefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextNoFlush(aNode->AsElement(),
                                               nullptr, nullptr);
  if (styleContext) {
    const nsStyleText* textStyle = styleContext->StyleText();
    return textStyle->WhiteSpaceOrNewlineIsSignificant();
  }
  return false;
}

bool
nsXHTMLContentSerializer::SerializeLIValueAttribute(nsIContent* aElement,
                                                    nsAString& aStr)
{
  // We are copying and we are at the "first" LI node of OL in selected range.
  // It may not be the first LI child of OL but it's first in the selected range.
  // Note that we get into this condition only once per a OL.
  bool found = false;
  nsCOMPtr<nsIDOMNode> currNode = do_QueryInterface(aElement);
  nsAutoString valueStr;

  olState state (0, false);

  if (!mOLStateStack.IsEmpty()) {
    state = mOLStateStack[mOLStateStack.Length()-1];
    // isFirstListItem should be true only before the serialization of the
    // first item in the list.
    state.isFirstListItem = false;
    mOLStateStack[mOLStateStack.Length()-1] = state;
  }

  int32_t startVal = state.startVal;
  int32_t offset = 0;

  // Traverse previous siblings until we find one with "value" attribute.
  // offset keeps track of how many previous siblings we had tocurrNode traverse.
  while (currNode && !found) {
    nsCOMPtr<nsIDOMElement> currElement = do_QueryInterface(currNode);
    // currElement may be null if it were a text node.
    if (currElement) {
      nsAutoString tagName;
      currElement->GetTagName(tagName);
      if (tagName.LowerCaseEqualsLiteral("li")) {
        currElement->GetAttribute(NS_LITERAL_STRING("value"), valueStr);
        if (valueStr.IsEmpty())
          offset++;
        else {
          found = true;
          nsresult rv = NS_OK;
          startVal = valueStr.ToInteger(&rv);
        }
      }
    }
    nsCOMPtr<nsIDOMNode> tmp;
    currNode->GetPreviousSibling(getter_AddRefs(tmp));
    currNode.swap(tmp);
  }
  // If LI was not having "value", Set the "value" attribute for it.
  // Note that We are at the first LI in the selected range of OL.
  if (offset == 0 && found) {
    // offset = 0 => LI itself has the value attribute and we did not need to traverse back.
    // Just serialize value attribute like other tags.
    NS_ENSURE_TRUE(SerializeAttr(EmptyString(), NS_LITERAL_STRING("value"),
                                 valueStr, aStr, false), false);
  }
  else if (offset == 1 && !found) {
    /*(offset = 1 && !found) means either LI is the first child node of OL
    and LI is not having "value" attribute.
    In that case we would not like to set "value" attribute to reduce the changes.
    */
    //do nothing...
  }
  else if (offset > 0) {
    // Set value attribute.
    nsAutoString valueStr;

    //As serializer needs to use this valueAttr we are creating here,
    valueStr.AppendInt(startVal + offset);
    NS_ENSURE_TRUE(SerializeAttr(EmptyString(), NS_LITERAL_STRING("value"),
                                 valueStr, aStr, false), false);
  }

  return true;
}

bool
nsXHTMLContentSerializer::IsFirstChildOfOL(nsIContent* aElement)
{
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
  nsAutoString parentName;

  nsCOMPtr<nsIDOMNode> parentNode;
  node->GetParentNode(getter_AddRefs(parentNode));
  if (parentNode)
    parentNode->GetNodeName(parentName);
  else
    return false;

  if (parentName.LowerCaseEqualsLiteral("ol")) {

    if (!mOLStateStack.IsEmpty()) {
      olState state = mOLStateStack[mOLStateStack.Length()-1];
      if (state.isFirstListItem)
        return true;
    }

    return false;
  }
  else
    return false;
}

bool
nsXHTMLContentSerializer::HasNoChildren(nsIContent * aContent) {

  for (nsIContent* child = aContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {

    if (!child->IsNodeOfType(nsINode::eTEXT))
      return false;

    if (child->TextLength())
      return false;
  }

  return true;
}
