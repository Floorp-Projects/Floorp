/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an XML DOM to an XML string that
 * could be parsed into more or less the original DOM.
 */

#include "nsXMLContentSerializer.h"

#include "nsGkAtoms.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocumentType.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsNameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsString.h"
#include "prprf.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsAttrName.h"
#include "nsILineBreaker.h"
#include "mozilla/dom/Element.h"
#include "nsParserConstants.h"

using namespace mozilla::dom;

#define kXMLNS "xmlns"

// to be readable, we assume that an indented line contains
// at least this number of characters (arbitrary value here).
// This is a limit for the indentation.
#define MIN_INDENTED_LINE_LENGTH 15

// the string used to indent.
#define INDENT_STRING "  "
#define INDENT_STRING_LENGTH 2

nsresult NS_NewXMLContentSerializer(nsIContentSerializer** aSerializer)
{
  nsXMLContentSerializer* it = new nsXMLContentSerializer();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aSerializer);
}

nsXMLContentSerializer::nsXMLContentSerializer()
  : mPrefixIndex(0),
    mColPos(0),
    mIndentOverflow(0),
    mIsIndentationAddedOnCurrentLine(false),
    mInAttribute(false),
    mAddNewlineForRootNode(false),
    mAddSpace(false),
    mMayIgnoreLineBreakSequence(false),
    mBodyOnly(false),
    mInBody(0)
{
}

nsXMLContentSerializer::~nsXMLContentSerializer()
{
}

NS_IMPL_ISUPPORTS(nsXMLContentSerializer, nsIContentSerializer)

NS_IMETHODIMP 
nsXMLContentSerializer::Init(uint32_t aFlags, uint32_t aWrapColumn,
                             const char* aCharSet, bool aIsCopying,
                             bool aRewriteEncodingDeclaration)
{
  mPrefixIndex = 0;
  mColPos = 0;
  mIndentOverflow = 0;
  mIsIndentationAddedOnCurrentLine = false;
  mInAttribute = false;
  mAddNewlineForRootNode = false;
  mAddSpace = false;
  mMayIgnoreLineBreakSequence = false;
  mBodyOnly = false;
  mInBody = 0;

  mCharset = aCharSet;
  mFlags = aFlags;

  // Set the line break character:
  if ((mFlags & nsIDocumentEncoder::OutputCRLineBreak)
      && (mFlags & nsIDocumentEncoder::OutputLFLineBreak)) { // Windows
    mLineBreak.AssignLiteral("\r\n");
  }
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) { // Mac
    mLineBreak.Assign('\r');
  }
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) { // Unix/DOM
    mLineBreak.Assign('\n');
  }
  else {
    mLineBreak.AssignLiteral(NS_LINEBREAK);         // Platform/default
  }

  mDoRaw = !!(mFlags & nsIDocumentEncoder::OutputRaw);

  mDoFormat = (mFlags & nsIDocumentEncoder::OutputFormatted && !mDoRaw);

  mDoWrap = (mFlags & nsIDocumentEncoder::OutputWrap && !mDoRaw);

  if (!aWrapColumn) {
    mMaxColumn = 72;
  }
  else {
    mMaxColumn = aWrapColumn;
  }

  mPreLevel = 0;
  mIsIndentationAddedOnCurrentLine = false;
  return NS_OK;
}

nsresult
nsXMLContentSerializer::AppendTextData(nsIContent* aNode,
                                       int32_t aStartOffset,
                                       int32_t aEndOffset,
                                       nsAString& aStr,
                                       bool aTranslateEntities)
{
  nsIContent* content = aNode;
  const nsTextFragment* frag;
  if (!content || !(frag = content->GetText())) {
    return NS_ERROR_FAILURE;
  }

  int32_t fragLength = frag->GetLength();
  int32_t endoffset = (aEndOffset == -1) ? fragLength : std::min(aEndOffset, fragLength);
  int32_t length = endoffset - aStartOffset;

  NS_ASSERTION(aStartOffset >= 0, "Negative start offset for text fragment!");
  NS_ASSERTION(aStartOffset <= endoffset, "A start offset is beyond the end of the text fragment!");

  if (length <= 0) {
    // XXX Zero is a legal value, maybe non-zero values should be an
    // error.
    return NS_OK;
  }
    
  if (frag->Is2b()) {
    const char16_t *strStart = frag->Get2b() + aStartOffset;
    if (aTranslateEntities) {
      AppendAndTranslateEntities(Substring(strStart, strStart + length), aStr);
    }
    else {
      aStr.Append(Substring(strStart, strStart + length));
    }
  }
  else {
    if (aTranslateEntities) {
      AppendAndTranslateEntities(NS_ConvertASCIItoUTF16(frag->Get1b()+aStartOffset, length), aStr);
    }
    else {
      aStr.Append(NS_ConvertASCIItoUTF16(frag->Get1b()+aStartOffset, length));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendText(nsIContent* aText,
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

  if (mPreLevel > 0 || mDoRaw) {
    AppendToStringConvertLF(data, aStr);
  }
  else if (mDoFormat) {
    AppendToStringFormatedWrapped(data, aStr);
  }
  else if (mDoWrap) {
    AppendToStringWrapped(data, aStr);
  }
  else {
    AppendToStringConvertLF(data, aStr);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendCDATASection(nsIContent* aCDATASection,
                                           int32_t aStartOffset,
                                           int32_t aEndOffset,
                                           nsAString& aStr)
{
  NS_ENSURE_ARG(aCDATASection);
  nsresult rv;

  NS_NAMED_LITERAL_STRING(cdata , "<![CDATA[");

  if (mPreLevel > 0 || mDoRaw) {
    AppendToString(cdata, aStr);
  }
  else if (mDoFormat) {
    AppendToStringFormatedWrapped(cdata, aStr);
  }
  else if (mDoWrap) {
    AppendToStringWrapped(cdata, aStr);
  }
  else {
    AppendToString(cdata, aStr);
  }

  nsAutoString data;
  rv = AppendTextData(aCDATASection, aStartOffset, aEndOffset, data, false);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  AppendToStringConvertLF(data, aStr);

  AppendToString(NS_LITERAL_STRING("]]>"), aStr);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendProcessingInstruction(nsIContent* aPI,
                                                    int32_t aStartOffset,
                                                    int32_t aEndOffset,
                                                    nsAString& aStr)
{
  nsCOMPtr<nsIDOMProcessingInstruction> pi = do_QueryInterface(aPI);
  NS_ENSURE_ARG(pi);
  nsresult rv;
  nsAutoString target, data, start;

  MaybeAddNewlineForRootNode(aStr);

  rv = pi->GetTarget(target);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  rv = pi->GetData(data);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  start.AppendLiteral("<?");
  start.Append(target);

  if (mPreLevel > 0 || mDoRaw) {
    AppendToString(start, aStr);
  }
  else if (mDoFormat) {
    if (mAddSpace) {
      AppendNewLineToString(aStr);
    }
    AppendToStringFormatedWrapped(start, aStr);
  }
  else if (mDoWrap) {
    AppendToStringWrapped(start, aStr);
  }
  else {
    AppendToString(start, aStr);
  }

  if (!data.IsEmpty()) {
    AppendToString(char16_t(' '), aStr);
    AppendToStringConvertLF(data, aStr);
  }
  AppendToString(NS_LITERAL_STRING("?>"), aStr);

  MaybeFlagNewlineForRootNode(aPI);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendComment(nsIContent* aComment,
                                      int32_t aStartOffset,
                                      int32_t aEndOffset,
                                      nsAString& aStr)
{
  nsCOMPtr<nsIDOMComment> comment = do_QueryInterface(aComment);
  NS_ENSURE_ARG(comment);
  nsresult rv;
  nsAutoString data;

  rv = comment->GetData(data);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  int32_t dataLength = data.Length();
  if (aStartOffset || (aEndOffset != -1 && aEndOffset < dataLength)) {
    int32_t length =
      (aEndOffset == -1) ? dataLength : std::min(aEndOffset, dataLength);
    length -= aStartOffset;

    nsAutoString frag;
    if (length > 0) {
      data.Mid(frag, aStartOffset, length);
    }
    data.Assign(frag);
  }

  MaybeAddNewlineForRootNode(aStr);

  NS_NAMED_LITERAL_STRING(startComment, "<!--");

  if (mPreLevel > 0 || mDoRaw) {
    AppendToString(startComment, aStr);
  }
  else if (mDoFormat) {
    if (mAddSpace) {
      AppendNewLineToString(aStr);
    }
    AppendToStringFormatedWrapped(startComment, aStr);
  }
  else if (mDoWrap) {
    AppendToStringWrapped(startComment, aStr);
  }
  else {
    AppendToString(startComment, aStr);
  }

  // Even if mDoformat, we don't format the content because it
  // could have been preformated by the author
  AppendToStringConvertLF(data, aStr);
  AppendToString(NS_LITERAL_STRING("-->"), aStr);

  MaybeFlagNewlineForRootNode(aComment);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendDoctype(nsIContent* aDocType,
                                      nsAString& aStr)
{
  nsCOMPtr<nsIDOMDocumentType> docType = do_QueryInterface(aDocType);
  NS_ENSURE_ARG(docType);
  nsresult rv;
  nsAutoString name, publicId, systemId, internalSubset;

  rv = docType->GetName(name);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = docType->GetPublicId(publicId);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = docType->GetSystemId(systemId);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = docType->GetInternalSubset(internalSubset);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  MaybeAddNewlineForRootNode(aStr);

  AppendToString(NS_LITERAL_STRING("<!DOCTYPE "), aStr);
  AppendToString(name, aStr);

  char16_t quote;
  if (!publicId.IsEmpty()) {
    AppendToString(NS_LITERAL_STRING(" PUBLIC "), aStr);
    if (publicId.FindChar(char16_t('"')) == -1) {
      quote = char16_t('"');
    }
    else {
      quote = char16_t('\'');
    }
    AppendToString(quote, aStr);
    AppendToString(publicId, aStr);
    AppendToString(quote, aStr);

    if (!systemId.IsEmpty()) {
      AppendToString(char16_t(' '), aStr);
      if (systemId.FindChar(char16_t('"')) == -1) {
        quote = char16_t('"');
      }
      else {
        quote = char16_t('\'');
      }
      AppendToString(quote, aStr);
      AppendToString(systemId, aStr);
      AppendToString(quote, aStr);
    }
  }
  else if (!systemId.IsEmpty()) {
    if (systemId.FindChar(char16_t('"')) == -1) {
      quote = char16_t('"');
    }
    else {
      quote = char16_t('\'');
    }
    AppendToString(NS_LITERAL_STRING(" SYSTEM "), aStr);
    AppendToString(quote, aStr);
    AppendToString(systemId, aStr);
    AppendToString(quote, aStr);
  }
  
  if (!internalSubset.IsEmpty()) {
    AppendToString(NS_LITERAL_STRING(" ["), aStr);
    AppendToString(internalSubset, aStr);
    AppendToString(char16_t(']'), aStr);
  }
    
  AppendToString(kGreaterThan, aStr);
  MaybeFlagNewlineForRootNode(aDocType);

  return NS_OK;
}

nsresult
nsXMLContentSerializer::PushNameSpaceDecl(const nsAString& aPrefix,
                                          const nsAString& aURI,
                                          nsIContent* aOwner)
{
  NameSpaceDecl* decl = mNameSpaceStack.AppendElement();
  if (!decl) return NS_ERROR_OUT_OF_MEMORY;

  decl->mPrefix.Assign(aPrefix);
  decl->mURI.Assign(aURI);
  // Don't addref - this weak reference will be removed when
  // we pop the stack
  decl->mOwner = aOwner;
  return NS_OK;
}

void
nsXMLContentSerializer::PopNameSpaceDeclsFor(nsIContent* aOwner)
{
  int32_t index, count;

  count = mNameSpaceStack.Length();
  for (index = count - 1; index >= 0; index--) {
    if (mNameSpaceStack[index].mOwner != aOwner) {
      break;
    }
    mNameSpaceStack.RemoveElementAt(index);
  }
}

bool
nsXMLContentSerializer::ConfirmPrefix(nsAString& aPrefix,
                                      const nsAString& aURI,
                                      nsIContent* aElement,
                                      bool aIsAttribute)
{
  if (aPrefix.EqualsLiteral(kXMLNS)) {
    return false;
  }

  if (aURI.EqualsLiteral("http://www.w3.org/XML/1998/namespace")) {
    // The prefix must be xml for this namespace. We don't need to declare it,
    // so always just set the prefix to xml.
    aPrefix.AssignLiteral("xml");

    return false;
  }

  bool mustHavePrefix;
  if (aIsAttribute) {
    if (aURI.IsEmpty()) {
      // Attribute in the null namespace.  This just shouldn't have a prefix.
      // And there's no need to push any namespace decls
      aPrefix.Truncate();
      return false;
    }

    // Attribute not in the null namespace -- must have a prefix
    mustHavePrefix = true;
  } else {
    // Not an attribute, so doesn't _have_ to have a prefix
    mustHavePrefix = false;
  }

  // Keep track of the closest prefix that's bound to aURI and whether we've
  // found such a thing.  closestURIMatch holds the prefix, and uriMatch
  // indicates whether we actually have one.
  nsAutoString closestURIMatch;
  bool uriMatch = false;

  // Also keep track of whether we've seen aPrefix already.  If we have, that
  // means that it's already bound to a URI different from aURI, so even if we
  // later (so in a more outer scope) see it bound to aURI we can't reuse it.
  bool haveSeenOurPrefix = false;

  int32_t count = mNameSpaceStack.Length();
  int32_t index = count - 1;
  while (index >= 0) {
    NameSpaceDecl& decl = mNameSpaceStack.ElementAt(index);
    // Check if we've found a prefix match
    if (aPrefix.Equals(decl.mPrefix)) {

      // If the URIs match and aPrefix is not bound to any other URI, we can
      // use aPrefix
      if (!haveSeenOurPrefix && aURI.Equals(decl.mURI)) {
        // Just use our uriMatch stuff.  That will deal with an empty aPrefix
        // the right way.  We can break out of the loop now, though.
        uriMatch = true;
        closestURIMatch = aPrefix;
        break;
      }

      haveSeenOurPrefix = true;      

      // If they don't, and either:
      // 1) We have a prefix (so we'd be redeclaring this prefix to point to a
      //    different namespace) or
      // 2) We're looking at an existing default namespace decl on aElement (so
      //    we can't create a new default namespace decl for this URI)
      // then generate a new prefix.  Note that we do NOT generate new prefixes
      // if we happen to have aPrefix == decl->mPrefix == "" and mismatching
      // URIs when |decl| doesn't have aElement as its owner.  In that case we
      // can simply push the new namespace URI as the default namespace for
      // aElement.
      if (!aPrefix.IsEmpty() || decl.mOwner == aElement) {
        NS_ASSERTION(!aURI.IsEmpty(),
                     "Not allowed to add a xmlns attribute with an empty "
                     "namespace name unless it declares the default "
                     "namespace.");

        GenerateNewPrefix(aPrefix);
        // Now we need to validate our new prefix/uri combination; check it
        // against the full namespace stack again.  Note that just restarting
        // the while loop is ok, since we haven't changed aURI, so the
        // closestURIMatch and uriMatch state is not affected.
        index = count - 1;
        haveSeenOurPrefix = false;
        continue;
      }
    }
    
    // If we've found a URI match, then record the first one
    if (!uriMatch && aURI.Equals(decl.mURI)) {
      // Need to check that decl->mPrefix is not declared anywhere closer to
      // us.  If it is, we can't use it.
      bool prefixOK = true;
      int32_t index2;
      for (index2 = count-1; index2 > index && prefixOK; --index2) {
        prefixOK = (mNameSpaceStack[index2].mPrefix != decl.mPrefix);
      }
      
      if (prefixOK) {
        uriMatch = true;
        closestURIMatch.Assign(decl.mPrefix);
      }
    }
    
    --index;
  }

  // At this point the following invariants hold:
  // 1) The prefix in closestURIMatch is mapped to aURI in our scope if
  //    uriMatch is set.
  // 2) There is nothing on the namespace stack that has aPrefix as the prefix
  //    and a _different_ URI, except for the case aPrefix.IsEmpty (and
  //    possible default namespaces on ancestors)
  
  // So if uriMatch is set it's OK to use the closestURIMatch prefix.  The one
  // exception is when closestURIMatch is actually empty (default namespace
  // decl) and we must have a prefix.
  if (uriMatch && (!mustHavePrefix || !closestURIMatch.IsEmpty())) {
    aPrefix.Assign(closestURIMatch);
    return false;
  }
  
  if (aPrefix.IsEmpty()) {
    // At this point, aPrefix is empty (which means we never had a prefix to
    // start with).  If we must have a prefix, just generate a new prefix and
    // then send it back through the namespace stack checks to make sure it's
    // OK.
    if (mustHavePrefix) {
      GenerateNewPrefix(aPrefix);
      return ConfirmPrefix(aPrefix, aURI, aElement, aIsAttribute);
    }

    // One final special case.  If aPrefix is empty and we never saw an empty
    // prefix (default namespace decl) on the namespace stack and we're in the
    // null namespace there is no reason to output an |xmlns=""| here.  It just
    // makes the output less readable.
    if (!haveSeenOurPrefix && aURI.IsEmpty()) {
      return false;
    }
  }

  // Now just set aURI as the new default namespace URI.  Indicate that we need
  // to create a namespace decl for the final prefix
  return true;
}

void
nsXMLContentSerializer::GenerateNewPrefix(nsAString& aPrefix)
{
  aPrefix.Assign('a');
  char buf[128];
  PR_snprintf(buf, sizeof(buf), "%d", mPrefixIndex++);
  AppendASCIItoUTF16(buf, aPrefix);
}

void
nsXMLContentSerializer::SerializeAttr(const nsAString& aPrefix,
                                      const nsAString& aName,
                                      const nsAString& aValue,
                                      nsAString& aStr,
                                      bool aDoEscapeEntities)
{
  nsAutoString attrString_;
  // For innerHTML we can do faster appending without
  // temporary strings.
  bool rawAppend = mDoRaw && aDoEscapeEntities;
  nsAString& attrString = (rawAppend) ? aStr : attrString_;

  attrString.Append(char16_t(' '));
  if (!aPrefix.IsEmpty()) {
    attrString.Append(aPrefix);
    attrString.Append(char16_t(':'));
  }
  attrString.Append(aName);

  if (aDoEscapeEntities) {
    // if problem characters are turned into character entity references
    // then there will be no problem with the value delimiter characters
    attrString.AppendLiteral("=\"");

    mInAttribute = true;
    AppendAndTranslateEntities(aValue, attrString);
    mInAttribute = false;

    attrString.Append(char16_t('"'));
    if (rawAppend) {
      return;
    }
  }
  else {
    // Depending on whether the attribute value contains quotes or apostrophes we
    // need to select the delimiter character and escape characters using
    // character entity references, ignoring the value of aDoEscapeEntities.
    // See http://www.w3.org/TR/REC-html40/appendix/notes.html#h-B.3.2.2 for
    // the standard on character entity references in values.  We also have to
    // make sure to escape any '&' characters.
    
    bool bIncludesSingle = false;
    bool bIncludesDouble = false;
    nsAString::const_iterator iCurr, iEnd;
    uint32_t uiSize, i;
    aValue.BeginReading(iCurr);
    aValue.EndReading(iEnd);
    for ( ; iCurr != iEnd; iCurr.advance(uiSize) ) {
      const char16_t * buf = iCurr.get();
      uiSize = iCurr.size_forward();
      for ( i = 0; i < uiSize; i++, buf++ ) {
        if ( *buf == char16_t('\'') )
        {
          bIncludesSingle = true;
          if ( bIncludesDouble ) break;
        }
        else if ( *buf == char16_t('"') )
        {
          bIncludesDouble = true;
          if ( bIncludesSingle ) break;
        }
      }
      // if both have been found we don't need to search further
      if ( bIncludesDouble && bIncludesSingle ) break;
    }

    // Delimiter and escaping is according to the following table
    //    bIncludesDouble     bIncludesSingle     Delimiter       Escape Double Quote
    //    FALSE               FALSE               "               FALSE
    //    FALSE               TRUE                "               FALSE
    //    TRUE                FALSE               '               FALSE
    //    TRUE                TRUE                "               TRUE
    char16_t cDelimiter = 
        (bIncludesDouble && !bIncludesSingle) ? char16_t('\'') : char16_t('"');
    attrString.Append(char16_t('='));
    attrString.Append(cDelimiter);
    nsAutoString sValue(aValue);
    sValue.ReplaceSubstring(NS_LITERAL_STRING("&"),
                            NS_LITERAL_STRING("&amp;"));
    if (bIncludesDouble && bIncludesSingle) {
      sValue.ReplaceSubstring(NS_LITERAL_STRING("\""),
                              NS_LITERAL_STRING("&quot;"));
    }
    attrString.Append(sValue);
    attrString.Append(cDelimiter);
  }
  if (mPreLevel > 0 || mDoRaw) {
    AppendToStringConvertLF(attrString, aStr);
  }
  else if (mDoFormat) {
    AppendToStringFormatedWrapped(attrString, aStr);
  }
  else if (mDoWrap) {
    AppendToStringWrapped(attrString, aStr);
  }
  else {
    AppendToStringConvertLF(attrString, aStr);
  }
}

uint32_t 
nsXMLContentSerializer::ScanNamespaceDeclarations(nsIContent* aContent,
                                                  nsIContent *aOriginalElement,
                                                  const nsAString& aTagNamespaceURI)
{
  uint32_t index, count;
  nsAutoString uriStr, valueStr;

  count = aContent->GetAttrCount();

  // First scan for namespace declarations, pushing each on the stack
  uint32_t skipAttr = count;
  for (index = 0; index < count; index++) {
    
    const nsAttrName* name = aContent->GetAttrNameAt(index);
    int32_t namespaceID = name->NamespaceID();
    nsIAtom *attrName = name->LocalName();
    
    if (namespaceID == kNameSpaceID_XMLNS ||
        // Also push on the stack attrs named "xmlns" in the null
        // namespace... because once we serialize those out they'll look like
        // namespace decls.  :(
        // XXXbz what if we have both "xmlns" in the null namespace and "xmlns"
        // in the xmlns namespace?
        (namespaceID == kNameSpaceID_None &&
         attrName == nsGkAtoms::xmlns)) {
      aContent->GetAttr(namespaceID, attrName, uriStr);

      if (!name->GetPrefix()) {
        if (aTagNamespaceURI.IsEmpty() && !uriStr.IsEmpty()) {
          // If the element is in no namespace we need to add a xmlns
          // attribute to declare that. That xmlns attribute must not have a
          // prefix (see http://www.w3.org/TR/REC-xml-names/#dt-prefix), ie it
          // must declare the default namespace. We just found an xmlns
          // attribute that declares the default namespace to something
          // non-empty. We're going to ignore this attribute, for children we
          // will detect that we need to add it again and attributes aren't
          // affected by the default namespace.
          skipAttr = index;
        }
        else {
          // Default NS attribute does not have prefix (and the name is "xmlns")
          PushNameSpaceDecl(EmptyString(), uriStr, aOriginalElement);
        }
      }
      else {
        PushNameSpaceDecl(nsDependentAtomString(attrName), uriStr,
                          aOriginalElement);
      }
    }
  }
  return skipAttr;
}


bool
nsXMLContentSerializer::IsJavaScript(nsIContent * aContent, nsIAtom* aAttrNameAtom,
                                     int32_t aAttrNamespaceID, const nsAString& aValueString)
{
  bool isHtml = aContent->IsHTML();
  bool isXul = aContent->IsXUL();
  bool isSvg = aContent->IsSVG();

  if (aAttrNamespaceID == kNameSpaceID_None &&
      (isHtml || isXul || isSvg) &&
      (aAttrNameAtom == nsGkAtoms::href ||
       aAttrNameAtom == nsGkAtoms::src)) {

    static const char kJavaScript[] = "javascript";
    int32_t pos = aValueString.FindChar(':');
    if (pos < (int32_t)(sizeof kJavaScript - 1))
        return false;
    nsAutoString scheme(Substring(aValueString, 0, pos));
    scheme.StripWhitespace();
    if ((scheme.Length() == (sizeof kJavaScript - 1)) &&
        scheme.EqualsIgnoreCase(kJavaScript))
      return true;
    else
      return false;
  }

  return aContent->IsEventAttributeName(aAttrNameAtom);
}


void 
nsXMLContentSerializer::SerializeAttributes(nsIContent* aContent,
                                            nsIContent *aOriginalElement,
                                            nsAString& aTagPrefix,
                                            const nsAString& aTagNamespaceURI,
                                            nsIAtom* aTagName,
                                            nsAString& aStr,
                                            uint32_t aSkipAttr,
                                            bool aAddNSAttr)
{

  nsAutoString prefixStr, uriStr, valueStr;
  nsAutoString xmlnsStr;
  xmlnsStr.AssignLiteral(kXMLNS);
  uint32_t index, count;

  // If we had to add a new namespace declaration, serialize
  // and push it on the namespace stack
  if (aAddNSAttr) {
    if (aTagPrefix.IsEmpty()) {
      // Serialize default namespace decl
      SerializeAttr(EmptyString(), xmlnsStr, aTagNamespaceURI, aStr, true);
    }
    else {
      // Serialize namespace decl
      SerializeAttr(xmlnsStr, aTagPrefix, aTagNamespaceURI, aStr, true);
    }
    PushNameSpaceDecl(aTagPrefix, aTagNamespaceURI, aOriginalElement);
  }

  count = aContent->GetAttrCount();

  // Now serialize each of the attributes
  // XXX Unfortunately we need a namespace manager to get
  // attribute URIs.
  for (index = 0; index < count; index++) {
    if (aSkipAttr == index) {
        continue;
    }

    const nsAttrName* name = aContent->GetAttrNameAt(index);
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
    
    aContent->GetAttr(namespaceID, attrName, valueStr);

    nsDependentAtomString nameStr(attrName);
    bool isJS = IsJavaScript(aContent, attrName, namespaceID, valueStr);

    SerializeAttr(prefixStr, nameStr, valueStr, aStr, !isJS);
    
    if (addNSAttr) {
      NS_ASSERTION(!prefixStr.IsEmpty(),
                   "Namespaced attributes must have a prefix");
      SerializeAttr(xmlnsStr, prefixStr, uriStr, aStr, true);
      PushNameSpaceDecl(prefixStr, uriStr, aOriginalElement);
    }
  }
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendElementStart(Element* aElement,
                                           Element* aOriginalElement,
                                           nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsIContent* content = aElement;

  bool forceFormat = false;
  if (!CheckElementStart(content, forceFormat, aStr)) {
    return NS_OK;
  }

  nsAutoString tagPrefix, tagLocalName, tagNamespaceURI;
  aElement->NodeInfo()->GetPrefix(tagPrefix);
  aElement->NodeInfo()->GetName(tagLocalName);
  aElement->NodeInfo()->GetNamespaceURI(tagNamespaceURI);

  uint32_t skipAttr = ScanNamespaceDeclarations(content,
                          aOriginalElement, tagNamespaceURI);

  nsIAtom *name = content->Tag();
  bool lineBreakBeforeOpen = LineBreakBeforeOpen(content->GetNameSpaceID(), name);

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

  bool addNSAttr;
  addNSAttr = ConfirmPrefix(tagPrefix, tagNamespaceURI, aOriginalElement,
                            false);

  // Serialize the qualified name of the element
  AppendToString(kLessThan, aStr);
  if (!tagPrefix.IsEmpty()) {
    AppendToString(tagPrefix, aStr);
    AppendToString(NS_LITERAL_STRING(":"), aStr);
  }
  AppendToString(tagLocalName, aStr);

  MaybeEnterInPreContent(content);

  if ((mDoFormat || forceFormat) && !mPreLevel && !mDoRaw) {
    IncrIndentation(name);
  }

  SerializeAttributes(content, aOriginalElement, tagPrefix, tagNamespaceURI,
                      name, aStr, skipAttr, addNSAttr);

  AppendEndOfElementStart(aOriginalElement, name, content->GetNameSpaceID(),
                          aStr);

  if ((mDoFormat || forceFormat) && !mPreLevel 
    && !mDoRaw && LineBreakAfterOpen(content->GetNameSpaceID(), name)) {
    AppendNewLineToString(aStr);
  }

  AfterElementStart(content, aOriginalElement, aStr);

  return NS_OK;
}

void 
nsXMLContentSerializer::AppendEndOfElementStart(nsIContent *aOriginalElement,
                                                nsIAtom * aName,
                                                int32_t aNamespaceID,
                                                nsAString& aStr)
{
  // We don't output a separate end tag for empty elements
  if (!aOriginalElement->GetChildCount()) {
    AppendToString(NS_LITERAL_STRING("/>"), aStr);
  }
  else {
    AppendToString(kGreaterThan, aStr);
  }
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendElementEnd(Element* aElement,
                                         nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsIContent* content = aElement;

  bool forceFormat = false, outputElementEnd;
  outputElementEnd = CheckElementEnd(content, forceFormat, aStr);

  nsIAtom *name = content->Tag();

  if ((mDoFormat || forceFormat) && !mPreLevel && !mDoRaw) {
    DecrIndentation(name);
  }

  if (!outputElementEnd) {
    PopNameSpaceDeclsFor(aElement);
    MaybeFlagNewlineForRootNode(aElement);
    return NS_OK;
  }

  nsAutoString tagPrefix, tagLocalName, tagNamespaceURI;
  
  aElement->NodeInfo()->GetPrefix(tagPrefix);
  aElement->NodeInfo()->GetName(tagLocalName);
  aElement->NodeInfo()->GetNamespaceURI(tagNamespaceURI);

#ifdef DEBUG
  bool debugNeedToPushNamespace =
#endif
  ConfirmPrefix(tagPrefix, tagNamespaceURI, aElement, false);
  NS_ASSERTION(!debugNeedToPushNamespace, "Can't push namespaces in closing tag!");

  if ((mDoFormat || forceFormat) && !mPreLevel && !mDoRaw) {

    bool lineBreakBeforeClose = LineBreakBeforeClose(content->GetNameSpaceID(), name);

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
  if (!tagPrefix.IsEmpty()) {
    AppendToString(tagPrefix, aStr);
    AppendToString(NS_LITERAL_STRING(":"), aStr);
  }
  AppendToString(tagLocalName, aStr);
  AppendToString(kGreaterThan, aStr);

  PopNameSpaceDeclsFor(aElement);

  MaybeLeaveFromPreContent(content);

  if ((mDoFormat || forceFormat) && !mPreLevel
      && !mDoRaw && LineBreakAfterClose(content->GetNameSpaceID(), name)) {
    AppendNewLineToString(aStr);
  }
  else {
    MaybeFlagNewlineForRootNode(aElement);
  }

  AfterElementEnd(content, aStr);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSerializer::AppendDocumentStart(nsIDocument *aDocument,
                                            nsAString& aStr)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  nsAutoString version, encoding, standalone;
  aDocument->GetXMLDeclaration(version, encoding, standalone);

  if (version.IsEmpty())
    return NS_OK; // A declaration must have version, or there is no decl

  NS_NAMED_LITERAL_STRING(endQuote, "\"");

  aStr += NS_LITERAL_STRING("<?xml version=\"") + version + endQuote;
  
  if (!mCharset.IsEmpty()) {
    aStr += NS_LITERAL_STRING(" encoding=\"") +
      NS_ConvertASCIItoUTF16(mCharset) + endQuote;
  }
  // Otherwise just don't output an encoding attr.  Not that we expect
  // mCharset to ever be empty.
#ifdef DEBUG
  else {
    NS_WARNING("Empty mCharset?  How come?");
  }
#endif

  if (!standalone.IsEmpty()) {
    aStr += NS_LITERAL_STRING(" standalone=\"") + standalone + endQuote;
  }

  aStr.AppendLiteral("?>");
  mAddNewlineForRootNode = true;

  return NS_OK;
}

bool
nsXMLContentSerializer::CheckElementStart(nsIContent * aContent,
                                          bool & aForceFormat,
                                          nsAString& aStr)
{
  aForceFormat = false;
  return true;
}

bool
nsXMLContentSerializer::CheckElementEnd(nsIContent * aContent,
                                        bool & aForceFormat,
                                        nsAString& aStr)
{
  // We don't output a separate end tag for empty element
  aForceFormat = false;
  return aContent->GetChildCount() > 0;
}

void 
nsXMLContentSerializer::AppendToString(const char16_t aChar,
                                       nsAString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }
  mColPos += 1;
  aOutputStr.Append(aChar);
}

void
nsXMLContentSerializer::AppendToString(const nsAString& aStr,
                                       nsAString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }
  mColPos += aStr.Length();
  aOutputStr.Append(aStr);
}


static const uint16_t kGTVal = 62;
static const char* kEntities[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "&amp;", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "&lt;", "", "&gt;"
};

static const char* kAttrEntities[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "&quot;", "", "", "", "&amp;", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "&lt;", "", "&gt;"
};

void
nsXMLContentSerializer::AppendAndTranslateEntities(const nsAString& aStr,
                                                   nsAString& aOutputStr)
{
  nsReadingIterator<char16_t> done_reading;
  aStr.EndReading(done_reading);

  // for each chunk of |aString|...
  uint32_t advanceLength = 0;
  nsReadingIterator<char16_t> iter;

  const char **entityTable = mInAttribute ? kAttrEntities : kEntities;

  for (aStr.BeginReading(iter);
       iter != done_reading;
       iter.advance(int32_t(advanceLength))) {
    uint32_t fragmentLength = iter.size_forward();
    const char16_t* c = iter.get();
    const char16_t* fragmentStart = c;
    const char16_t* fragmentEnd = c + fragmentLength;
    const char* entityText = nullptr;

    advanceLength = 0;
    // for each character in this chunk, check if it
    // needs to be replaced
    for (; c < fragmentEnd; c++, advanceLength++) {
      char16_t val = *c;
      if ((val <= kGTVal) && (entityTable[val][0] != 0)) {
        entityText = entityTable[val];
        break;
      }
    }

    aOutputStr.Append(fragmentStart, advanceLength);
    if (entityText) {
      AppendASCIItoUTF16(entityText, aOutputStr);
      advanceLength++;
    }
  }
}

void
nsXMLContentSerializer::MaybeAddNewlineForRootNode(nsAString& aStr)
{
  if (mAddNewlineForRootNode) {
    AppendNewLineToString(aStr);
  }
}

void
nsXMLContentSerializer::MaybeFlagNewlineForRootNode(nsINode* aNode)
{
  nsINode* parent = aNode->GetParentNode();
  if (parent) {
    mAddNewlineForRootNode = parent->IsNodeOfType(nsINode::eDOCUMENT);
  }
}

void
nsXMLContentSerializer::MaybeEnterInPreContent(nsIContent* aNode)
{
  // support of the xml:space attribute
  if (aNode->HasAttr(kNameSpaceID_XML, nsGkAtoms::space)) {
    nsAutoString space;
    aNode->GetAttr(kNameSpaceID_XML, nsGkAtoms::space, space);
    if (space.EqualsLiteral("preserve"))
      ++mPreLevel;
  }
}

void
nsXMLContentSerializer::MaybeLeaveFromPreContent(nsIContent* aNode)
{
  // support of the xml:space attribute
  if (aNode->HasAttr(kNameSpaceID_XML, nsGkAtoms::space)) {
    nsAutoString space;
    aNode->GetAttr(kNameSpaceID_XML, nsGkAtoms::space, space);
    if (space.EqualsLiteral("preserve"))
      --mPreLevel;
  }
}

void
nsXMLContentSerializer::AppendNewLineToString(nsAString& aStr)
{
  AppendToString(mLineBreak, aStr);
  mMayIgnoreLineBreakSequence = true;
  mColPos = 0;
  mAddSpace = false;
  mIsIndentationAddedOnCurrentLine = false;
}

void
nsXMLContentSerializer::AppendIndentation(nsAString& aStr)
{
  mIsIndentationAddedOnCurrentLine = true;
  AppendToString(mIndent, aStr);
  mAddSpace = false;
  mMayIgnoreLineBreakSequence = false;
}

void
nsXMLContentSerializer::IncrIndentation(nsIAtom* aName)
{
  // we want to keep the source readable
  if (mDoWrap &&
      mIndent.Length() >= uint32_t(mMaxColumn) - MIN_INDENTED_LINE_LENGTH) {
    ++mIndentOverflow;
  }
  else {
    mIndent.AppendLiteral(INDENT_STRING);
  }
}

void
nsXMLContentSerializer::DecrIndentation(nsIAtom* aName)
{
  if(mIndentOverflow)
    --mIndentOverflow;
  else
    mIndent.Cut(0, INDENT_STRING_LENGTH);
}

bool
nsXMLContentSerializer::LineBreakBeforeOpen(int32_t aNamespaceID, nsIAtom* aName)
{
  return mAddSpace;
}

bool 
nsXMLContentSerializer::LineBreakAfterOpen(int32_t aNamespaceID, nsIAtom* aName)
{
  return false;
}

bool 
nsXMLContentSerializer::LineBreakBeforeClose(int32_t aNamespaceID, nsIAtom* aName)
{
  return mAddSpace;
}

bool 
nsXMLContentSerializer::LineBreakAfterClose(int32_t aNamespaceID, nsIAtom* aName)
{
  return false;
}

void
nsXMLContentSerializer::AppendToStringConvertLF(const nsAString& aStr,
                                                nsAString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  if (mDoRaw) {
    AppendToString(aStr, aOutputStr);
  }
  else {
    // Convert line-endings to mLineBreak
    uint32_t start = 0;
    uint32_t theLen = aStr.Length();
    while (start < theLen) {
      int32_t eol = aStr.FindChar('\n', start);
      if (eol == kNotFound) {
        nsDependentSubstring dataSubstring(aStr, start, theLen - start);
        AppendToString(dataSubstring, aOutputStr);
        start = theLen;
        // if there was a line break before this substring
        // AppendNewLineToString was called, so we should reverse
        // this flag
        mMayIgnoreLineBreakSequence = false;
      }
      else {
        nsDependentSubstring dataSubstring(aStr, start, eol - start);
        AppendToString(dataSubstring, aOutputStr);
        AppendNewLineToString(aOutputStr);
        start = eol + 1;
      }
    }
  }
}

void
nsXMLContentSerializer::AppendFormatedWrapped_WhitespaceSequence(
                        nsASingleFragmentString::const_char_iterator &aPos,
                        const nsASingleFragmentString::const_char_iterator aEnd,
                        const nsASingleFragmentString::const_char_iterator aSequenceStart,
                        bool &aMayIgnoreStartOfLineWhitespaceSequence,
                        nsAString &aOutputStr)
{
  // Handle the complete sequence of whitespace.
  // Continue to iterate until we find the first non-whitespace char.
  // Updates "aPos" to point to the first unhandled char.
  // Also updates the aMayIgnoreStartOfLineWhitespaceSequence flag,
  // as well as the other "global" state flags.

  bool sawBlankOrTab = false;
  bool leaveLoop = false;

  do {
    switch (*aPos) {
      case ' ':
      case '\t':
        sawBlankOrTab = true;
        // no break
      case '\n':
        ++aPos;
        // do not increase mColPos,
        // because we will reduce the whitespace to a single char
        break;
      default:
        leaveLoop = true;
        break;
    }
  } while (!leaveLoop && aPos < aEnd);

  if (mAddSpace) {
    // if we had previously been asked to add space,
    // our situation has not changed
  }
  else if (!sawBlankOrTab && mMayIgnoreLineBreakSequence) {
    // nothing to do in the case where line breaks have already been added
    // before the call of AppendToStringWrapped
    // and only if we found line break in the sequence
    mMayIgnoreLineBreakSequence = false;
  }
  else if (aMayIgnoreStartOfLineWhitespaceSequence) {
    // nothing to do
    aMayIgnoreStartOfLineWhitespaceSequence = false;
  }
  else {
    if (sawBlankOrTab) {
      if (mDoWrap && mColPos + 1 >= mMaxColumn) {
        // no much sense in delaying, we only have one slot left,
        // let's write a break now
        aOutputStr.Append(mLineBreak);
        mColPos = 0;
        mIsIndentationAddedOnCurrentLine = false;
        mMayIgnoreLineBreakSequence = true;
      }
      else {
        // do not write out yet, we may write out either a space or a linebreak
        // let's delay writing it out until we know more
        mAddSpace = true;
        ++mColPos; // eat a slot of available space
      }
    }
    else {
      // Asian text usually does not contain spaces, therefore we should not
      // transform a linebreak into a space.
      // Since we only saw linebreaks, but no spaces or tabs,
      // let's write a linebreak now.
      AppendNewLineToString(aOutputStr);
    }
  }
}

void
nsXMLContentSerializer::AppendWrapped_NonWhitespaceSequence(
                        nsASingleFragmentString::const_char_iterator &aPos,
                        const nsASingleFragmentString::const_char_iterator aEnd,
                        const nsASingleFragmentString::const_char_iterator aSequenceStart,
                        bool &aMayIgnoreStartOfLineWhitespaceSequence,
                        bool &aSequenceStartAfterAWhiteSpace,
                        nsAString& aOutputStr)
{
  mMayIgnoreLineBreakSequence = false;
  aMayIgnoreStartOfLineWhitespaceSequence = false;

  // Handle the complete sequence of non-whitespace in this block
  // Iterate until we find the first whitespace char or an aEnd condition
  // Updates "aPos" to point to the first unhandled char.
  // Also updates the aMayIgnoreStartOfLineWhitespaceSequence flag,
  // as well as the other "global" state flags.

  bool thisSequenceStartsAtBeginningOfLine = !mColPos;
  bool onceAgainBecauseWeAddedBreakInFront = false;
  bool foundWhitespaceInLoop;
  uint32_t length, colPos;

  do {

    if (mColPos) {
      colPos = mColPos;
    }
    else {
      if (mDoFormat && !mPreLevel && !onceAgainBecauseWeAddedBreakInFront) {
        colPos = mIndent.Length();
      }
      else
        colPos = 0;
    }
    foundWhitespaceInLoop = false;
    length = 0;
    // we iterate until the next whitespace character
    // or until we reach the maximum of character per line
    // or until the end of the string to add.
    do {
      if (*aPos == ' ' || *aPos == '\t' || *aPos == '\n') {
        foundWhitespaceInLoop = true;
        break;
      }

      ++aPos;
      ++length;
    } while ( (!mDoWrap || colPos + length < mMaxColumn) && aPos < aEnd);

    // in the case we don't reached the end of the string, but we reached the maxcolumn,
    // we see if there is a whitespace after the maxcolumn
    // if yes, then we can append directly the string instead of
    // appending a new line etc.
    if (*aPos == ' ' || *aPos == '\t' || *aPos == '\n') {
      foundWhitespaceInLoop = true;
    }

    if (aPos == aEnd || foundWhitespaceInLoop) {
      // there is enough room for the complete block we found
      if (mDoFormat && !mColPos) {
        AppendIndentation(aOutputStr);
      }
      else if (mAddSpace) {
        aOutputStr.Append(char16_t(' '));
        mAddSpace = false;
      }

      mColPos += length;
      aOutputStr.Append(aSequenceStart, aPos - aSequenceStart);

      // We have not yet reached the max column, we will continue to
      // fill the current line in the next outer loop iteration
      // (this one in AppendToStringWrapped)
      // make sure we return in this outer loop
      onceAgainBecauseWeAddedBreakInFront = false;
    }
    else { // we reach the max column
      if (!thisSequenceStartsAtBeginningOfLine &&
          (mAddSpace || (!mDoFormat && aSequenceStartAfterAWhiteSpace))) { 
          // when !mDoFormat, mAddSpace is not used, mAddSpace is always false
          // so, in the case where mDoWrap && !mDoFormat, if we want to enter in this condition...

        // We can avoid to wrap. We try to add the whole block 
        // in an empty new line

        AppendNewLineToString(aOutputStr);
        aPos = aSequenceStart;
        thisSequenceStartsAtBeginningOfLine = true;
        onceAgainBecauseWeAddedBreakInFront = true;
      }
      else {
        // we must wrap
        onceAgainBecauseWeAddedBreakInFront = false;
        bool foundWrapPosition = false;
        int32_t wrapPosition;

        nsILineBreaker *lineBreaker = nsContentUtils::LineBreaker();

        wrapPosition = lineBreaker->Prev(aSequenceStart,
                                         (aEnd - aSequenceStart),
                                         (aPos - aSequenceStart) + 1);
        if (wrapPosition != NS_LINEBREAKER_NEED_MORE_TEXT) {
          foundWrapPosition = true;
        }
        else {
          wrapPosition = lineBreaker->Next(aSequenceStart,
                                           (aEnd - aSequenceStart),
                                           (aPos - aSequenceStart));
          if (wrapPosition != NS_LINEBREAKER_NEED_MORE_TEXT) {
            foundWrapPosition = true;
          }
        }

        if (foundWrapPosition) {
          if (!mColPos && mDoFormat) {
            AppendIndentation(aOutputStr);
          }
          else if (mAddSpace) {
            aOutputStr.Append(char16_t(' '));
            mAddSpace = false;
          }
          aOutputStr.Append(aSequenceStart, wrapPosition);

          AppendNewLineToString(aOutputStr);
          aPos = aSequenceStart + wrapPosition;
          aMayIgnoreStartOfLineWhitespaceSequence = true;
        }
        else {
          // try some simple fallback logic
          // go forward up to the next whitespace position,
          // in the worst case this will be all the rest of the data

          // we update the mColPos variable with the length of
          // the part already parsed.
          mColPos += length;

          // now try to find the next whitespace
          do {
            if (*aPos == ' ' || *aPos == '\t' || *aPos == '\n') {
              break;
            }

            ++aPos;
            ++mColPos;
          } while (aPos < aEnd);

          if (mAddSpace) {
            aOutputStr.Append(char16_t(' '));
            mAddSpace = false;
          }
          aOutputStr.Append(aSequenceStart, aPos - aSequenceStart);
        }
      }
      aSequenceStartAfterAWhiteSpace = false;
    }
  } while (onceAgainBecauseWeAddedBreakInFront);
}

void 
nsXMLContentSerializer::AppendToStringFormatedWrapped(const nsASingleFragmentString& aStr,
                                                      nsAString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  nsASingleFragmentString::const_char_iterator pos, end, sequenceStart;

  aStr.BeginReading(pos);
  aStr.EndReading(end);

  bool sequenceStartAfterAWhitespace = false;
  if (pos < end) {
    nsAString::const_char_iterator end2;
    aOutputStr.EndReading(end2);
    --end2;
    if (*end2 == ' ' || *end2 == '\n' || *end2 == '\t') {
      sequenceStartAfterAWhitespace = true;
    }
  }

  // if the current line already has text on it, such as a tag,
  // leading whitespace is significant
  bool mayIgnoreStartOfLineWhitespaceSequence =
    (!mColPos || (mIsIndentationAddedOnCurrentLine &&
                  sequenceStartAfterAWhitespace &&
                  uint32_t(mColPos) == mIndent.Length()));

  while (pos < end) {
    sequenceStart = pos;

    // if beginning of a whitespace sequence
    if (*pos == ' ' || *pos == '\n' || *pos == '\t') {
      AppendFormatedWrapped_WhitespaceSequence(pos, end, sequenceStart,
        mayIgnoreStartOfLineWhitespaceSequence, aOutputStr);
    }
    else { // any other non-whitespace char
      AppendWrapped_NonWhitespaceSequence(pos, end, sequenceStart,
        mayIgnoreStartOfLineWhitespaceSequence, sequenceStartAfterAWhitespace, aOutputStr);
    }
  }
}

void
nsXMLContentSerializer::AppendWrapped_WhitespaceSequence(
                        nsASingleFragmentString::const_char_iterator &aPos,
                        const nsASingleFragmentString::const_char_iterator aEnd,
                        const nsASingleFragmentString::const_char_iterator aSequenceStart,
                        nsAString &aOutputStr)
{
  // Handle the complete sequence of whitespace.
  // Continue to iterate until we find the first non-whitespace char.
  // Updates "aPos" to point to the first unhandled char.
  mAddSpace = false;
  mIsIndentationAddedOnCurrentLine = false;

  bool leaveLoop = false;
  nsASingleFragmentString::const_char_iterator lastPos = aPos;

  do {
    switch (*aPos) {
      case ' ':
      case '\t':
        // if there are too many spaces on a line, we wrap
        if (mColPos >= mMaxColumn) {
          if (lastPos != aPos) {
            aOutputStr.Append(lastPos, aPos - lastPos);
          }
          AppendToString(mLineBreak, aOutputStr);
          mColPos = 0;
          lastPos = aPos;
        }

        ++mColPos;
        ++aPos;
        break;
      case '\n':
        if (lastPos != aPos) {
          aOutputStr.Append(lastPos, aPos - lastPos);
        }
        AppendToString(mLineBreak, aOutputStr);
        mColPos = 0;
        ++aPos;
        lastPos = aPos;
        break;
      default:
        leaveLoop = true;
        break;
    }
  } while (!leaveLoop && aPos < aEnd);

  if (lastPos != aPos) {
    aOutputStr.Append(lastPos, aPos - lastPos);
  }
}

void 
nsXMLContentSerializer::AppendToStringWrapped(const nsASingleFragmentString& aStr,
                                              nsAString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  nsASingleFragmentString::const_char_iterator pos, end, sequenceStart;

  aStr.BeginReading(pos);
  aStr.EndReading(end);

  // not used in this case, but needed by AppendWrapped_NonWhitespaceSequence
  bool mayIgnoreStartOfLineWhitespaceSequence = false;
  mMayIgnoreLineBreakSequence = false;

  bool sequenceStartAfterAWhitespace = false;
  if (pos < end && !aOutputStr.IsEmpty()) {
    nsAString::const_char_iterator end2;
    aOutputStr.EndReading(end2);
    --end2;
    if (*end2 == ' ' || *end2 == '\n' || *end2 == '\t') {
      sequenceStartAfterAWhitespace = true;
    }
  }

  while (pos < end) {
    sequenceStart = pos;

    // if beginning of a whitespace sequence
    if (*pos == ' ' || *pos == '\n' || *pos == '\t') {
      sequenceStartAfterAWhitespace = true;
      AppendWrapped_WhitespaceSequence(pos, end, sequenceStart, aOutputStr);
    }
    else { // any other non-whitespace char
      AppendWrapped_NonWhitespaceSequence(pos, end, sequenceStart,
        mayIgnoreStartOfLineWhitespaceSequence, sequenceStartAfterAWhitespace, aOutputStr);
    }
  }
}
