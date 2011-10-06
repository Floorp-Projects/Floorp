/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 *   Ryan Jones <sciguyryan@gmail.com>
 *   Laurent Jouanneau <laurent.jouanneau@disruptive-innovations.com>
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
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an XHTML (not HTML!) DOM to an XHTML
 * string that could be parsed into more or less the original DOM.
 */

#include "nsXHTMLContentSerializer.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsINameSpaceManager.h"
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
#include "nsParserConstants.h"

static const char kMozStr[] = "moz";

static const PRInt32 kLongLineLen = 128;

#define kXMLNS "xmlns"

nsresult NS_NewXHTMLContentSerializer(nsIContentSerializer** aSerializer)
{
  nsXHTMLContentSerializer* it = new nsXHTMLContentSerializer();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aSerializer);
}

nsXHTMLContentSerializer::nsXHTMLContentSerializer()
  : mIsHTMLSerializer(PR_FALSE)
{
}

nsXHTMLContentSerializer::~nsXHTMLContentSerializer()
{
  NS_ASSERTION(mOLStateStack.IsEmpty(), "Expected OL State stack to be empty");
}

NS_IMETHODIMP
nsXHTMLContentSerializer::Init(PRUint32 aFlags, PRUint32 aWrapColumn,
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
  mIsFirstChildOfOL = PR_FALSE;
  mInBody = 0;
  mDisableEntityEncoding = 0;
  mBodyOnly = (mFlags & nsIDocumentEncoder::OutputBodyOnly) ? PR_TRUE
                                                            : PR_FALSE;

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
nsXHTMLContentSerializer::HasLongLines(const nsString& text, PRInt32& aLastNewlineOffset)
{
  PRUint32 start=0;
  PRUint32 theLen = text.Length();
  bool rv = false;
  aLastNewlineOffset = kNotFound;
  for (start = 0; start < theLen; ) {
    PRInt32 eol = text.FindChar('\n', start);
    if (eol < 0) {
      eol = text.Length();
    }
    else {
      aLastNewlineOffset = eol;
    }
    if (PRInt32(eol - start) > kLongLineLen)
      rv = PR_TRUE;
    start = eol + 1;
  }
  return rv;
}

NS_IMETHODIMP
nsXHTMLContentSerializer::AppendText(nsIContent* aText,
                                     PRInt32 aStartOffset,
                                     PRInt32 aEndOffset,
                                     nsAString& aStr)
{
  NS_ENSURE_ARG(aText);

  nsAutoString data;
  nsresult rv;

  rv = AppendTextData(aText, aStartOffset, aEndOffset, data, PR_TRUE);
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
    PRInt32 lastNewlineOffset = kNotFound;
    if (HasLongLines(data, lastNewlineOffset)) {
      // We have long lines, rewrap
      mDoWrap = PR_TRUE;
      AppendToStringWrapped(data, aStr);
      mDoWrap = PR_FALSE;
    }
    else {
      AppendToStringConvertLF(data, aStr);
    }
  }

  return NS_OK;
}

nsresult
nsXHTMLContentSerializer::EscapeURI(nsIContent* aContent, const nsAString& aURI, nsAString& aEscapedURI)
{
  // URL escape %xx cannot be used in JS.
  // No escaping if the scheme is 'javascript'.
  if (IsJavaScript(aContent, nsGkAtoms::href, kNameSpaceID_None, aURI)) {
    aEscapedURI = aURI;
    return NS_OK;
  }

  // nsITextToSubURI does charset convert plus uri escape
  // This is needed to convert to a document charset which is needed to support existing browsers.
  // But we eventually want to use UTF-8 instead of a document charset, then the code would be much simpler.
  // See HTML 4.01 spec, "Appendix B.2.1 Non-ASCII characters in URI attribute values"
  nsCOMPtr<nsITextToSubURI> textToSubURI;
  nsAutoString uri(aURI); // in order to use FindCharInSet()
  nsresult rv = NS_OK;

  if (!mCharset.IsEmpty() && !IsASCII(uri)) {
    textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt32 start = 0;
  PRInt32 end;
  nsAutoString part;
  nsXPIDLCString escapedURI;
  aEscapedURI.Truncate(0);

  // Loop and escape parts by avoiding escaping reserved characters
  // (and '%', '#', as well as '[' and ']' for IPv6 address literals).
  while ((end = uri.FindCharInSet("%#;/?:@&=+$,[]", start)) != -1) {
    part = Substring(aURI, start, (end-start));
    if (textToSubURI && !IsASCII(part)) {
      rv = textToSubURI->ConvertAndEscape(mCharset.get(), part.get(), getter_Copies(escapedURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      escapedURI.Adopt(nsEscape(NS_ConvertUTF16toUTF8(part).get(), url_Path));
    }
    AppendASCIItoUTF16(escapedURI, aEscapedURI);

    // Append a reserved character without escaping.
    part = Substring(aURI, end, 1);
    aEscapedURI.Append(part);
    start = end + 1;
  }

  if (start < (PRInt32) aURI.Length()) {
    // Escape the remaining part.
    part = Substring(aURI, start, aURI.Length()-start);
    if (textToSubURI) {
      rv = textToSubURI->ConvertAndEscape(mCharset.get(), part.get(), getter_Copies(escapedURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      escapedURI.Adopt(nsEscape(NS_ConvertUTF16toUTF8(part).get(), url_Path));
    }
    AppendASCIItoUTF16(escapedURI, aEscapedURI);
  }

  return rv;
}

void
nsXHTMLContentSerializer::SerializeAttributes(nsIContent* aContent,
                                              nsIContent *aOriginalElement,
                                              nsAString& aTagPrefix,
                                              const nsAString& aTagNamespaceURI,
                                              nsIAtom* aTagName,
                                              nsAString& aStr,
                                              PRUint32 aSkipAttr,
                                              bool aAddNSAttr)
{
  nsresult rv;
  PRUint32 index, count;
  nsAutoString prefixStr, uriStr, valueStr;
  nsAutoString xmlnsStr;
  xmlnsStr.AssignLiteral(kXMLNS);

  PRInt32 contentNamespaceID = aContent->GetNameSpaceID();

  // this method is not called by nsHTMLContentSerializer
  // so we don't have to check HTML element, just XHTML

  if (mIsCopying && kNameSpaceID_XHTML == contentNamespaceID) {

    // Need to keep track of OL and LI elements in order to get ordinal number 
    // for the LI.
    if (aTagName == nsGkAtoms::ol) {
      // We are copying and current node is an OL;
      // Store its start attribute value in olState->startVal.
      nsAutoString start;
      PRInt32 startAttrVal = 0;
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::start, start);
      if (!start.IsEmpty()) {
        PRInt32 rv = 0;
        startAttrVal = start.ToInteger(&rv);
        //If OL has "start" attribute, first LI element has to start with that value
        //Therefore subtracting 1 as all the LI elements are incrementing it before using it;
        //In failure of ToInteger(), default StartAttrValue to 0.
        if (NS_SUCCEEDED(rv))
          --startAttrVal;
        else
          startAttrVal = 0;
      }
      olState state (startAttrVal, PR_TRUE);
      mOLStateStack.AppendElement(state);
    }
    else if (aTagName == nsGkAtoms::li) {
      mIsFirstChildOfOL = IsFirstChildOfOL(aOriginalElement);
      if (mIsFirstChildOfOL) {
        // If OL is parent of this LI, serialize attributes in different manner.
        SerializeLIValueAttribute(aContent, aStr);
      }
    }
  }

  // If we had to add a new namespace declaration, serialize
  // and push it on the namespace stack
  if (aAddNSAttr) {
    if (aTagPrefix.IsEmpty()) {
      // Serialize default namespace decl
      SerializeAttr(EmptyString(), xmlnsStr, aTagNamespaceURI, aStr, PR_TRUE);
    } else {
      // Serialize namespace decl
      SerializeAttr(xmlnsStr, aTagPrefix, aTagNamespaceURI, aStr, PR_TRUE);
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

    const nsAttrName* name = aContent->GetAttrNameAt(index);
    PRInt32 namespaceID = name->NamespaceID();
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
      addNSAttr = ConfirmPrefix(prefixStr, uriStr, aOriginalElement, PR_TRUE);
    }

    aContent->GetAttr(namespaceID, attrName, valueStr);

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
        // Need to escape URI.
        nsAutoString tempURI(valueStr);
        if (!isJS && NS_FAILED(EscapeURI(aContent, tempURI, valueStr)))
          valueStr = tempURI;
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

    SerializeAttr(prefixStr, nameStr, valueStr, aStr, !isJS);

    if (addNSAttr) {
      NS_ASSERTION(!prefixStr.IsEmpty(),
                   "Namespaced attributes must have a prefix");
      SerializeAttr(xmlnsStr, prefixStr, uriStr, aStr, PR_TRUE);
      PushNameSpaceDecl(prefixStr, uriStr, aOriginalElement);
    }
  }
}


void 
nsXHTMLContentSerializer::AppendEndOfElementStart(nsIContent *aOriginalElement,
                                                  nsIAtom * aName,
                                                  PRInt32 aNamespaceID,
                                                  nsAString& aStr)
{
  // this method is not called by nsHTMLContentSerializer
  // so we don't have to check HTML element, just XHTML
  NS_ASSERTION(!mIsHTMLSerializer, "nsHTMLContentSerializer shouldn't call this method !");

  if (kNameSpaceID_XHTML != aNamespaceID) {
    nsXMLContentSerializer::AppendEndOfElementStart(aOriginalElement, aName,
                                                    aNamespaceID, aStr);
    return;
  }

  nsIContent* content = aOriginalElement;

  // for non empty elements, even if they are not a container, we always
  // serialize their content, because the XHTML element could contain non XHTML
  // nodes useful in some context, like in an XSLT stylesheet
  if (HasNoChildren(content)) {

    nsIParserService* parserService = nsContentUtils::GetParserService();
  
    if (parserService) {
      bool isContainer;
      parserService->
        IsContainer(parserService->HTMLCaseSensitiveAtomTagToId(aName),
                    isContainer);
      if (!isContainer) {
        // for backward compatibility with HTML 4 user agents
        // only non-container HTML elements can be closed immediatly,
        // and a space is added before />
        AppendToString(NS_LITERAL_STRING(" />"), aStr);
        return;
      }
    }
  }
  AppendToString(kGreaterThan, aStr);
}

void
nsXHTMLContentSerializer::AfterElementStart(nsIContent * aContent,
                                            nsIContent *aOriginalElement,
                                            nsAString& aStr)
{
  nsIAtom *name = aContent->Tag();
  if (aContent->GetNameSpaceID() == kNameSpaceID_XHTML &&
      mRewriteEncodingDeclaration &&
      name == nsGkAtoms::head) {

    // Check if there already are any content-type meta children.
    // If there are, they will be modified to use the correct charset.
    // If there aren't, we'll insert one here.
    bool hasMeta = false;
    for (nsIContent* child = aContent->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      if (child->IsHTML(nsGkAtoms::meta) &&
          child->HasAttr(kNameSpaceID_None, nsGkAtoms::content)) {
        nsAutoString header;
        child->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, header);

        if (header.LowerCaseEqualsLiteral("content-type")) {
          hasMeta = PR_TRUE;
          break;
        }
      }
    }

    if (!hasMeta) {
      AppendNewLineToString(aStr);
      if (mDoFormat) {
        AppendIndentation(aStr);
      }
      AppendToString(NS_LITERAL_STRING("<meta http-equiv=\"content-type\""),
                    aStr);
      AppendToString(NS_LITERAL_STRING(" content=\"text/html; charset="), aStr);
      AppendToString(NS_ConvertASCIItoUTF16(mCharset), aStr);
      if (mIsHTMLSerializer)
        AppendToString(NS_LITERAL_STRING("\">"), aStr);
      else
        AppendToString(NS_LITERAL_STRING("\" />"), aStr);
    }
  }
}

void
nsXHTMLContentSerializer::AfterElementEnd(nsIContent * aContent,
                                          nsAString& aStr)
{
  NS_ASSERTION(!mIsHTMLSerializer, "nsHTMLContentSerializer shouldn't call this method !");

  PRInt32 namespaceID = aContent->GetNameSpaceID();
  nsIAtom *name = aContent->Tag();

  // this method is not called by nsHTMLContentSerializer
  // so we don't have to check HTML element, just XHTML
  if (kNameSpaceID_XHTML == namespaceID && name == nsGkAtoms::body) {
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
                                            nsAString& aStr)
{
  // The _moz_dirty attribute is emitted by the editor to
  // indicate that this element should be pretty printed
  // even if we're not in pretty printing mode
  aForceFormat = aContent->HasAttr(kNameSpaceID_None,
                                   nsGkAtoms::mozdirty);

  nsIAtom *name = aContent->Tag();
  PRInt32 namespaceID = aContent->GetNameSpaceID();

  if (namespaceID == kNameSpaceID_XHTML) {
    if (name == nsGkAtoms::br && mPreLevel > 0 && 
        (mFlags & nsIDocumentEncoder::OutputNoFormattingInPre)) {
      AppendNewLineToString(aStr);
      return PR_FALSE;
    }

    if (name == nsGkAtoms::body) {
      ++mInBody;
    }
  }
  return PR_TRUE;
}

bool
nsXHTMLContentSerializer::CheckElementEnd(nsIContent * aContent,
                                          bool & aForceFormat,
                                          nsAString& aStr)
{
  NS_ASSERTION(!mIsHTMLSerializer, "nsHTMLContentSerializer shouldn't call this method !");

  aForceFormat = aContent->HasAttr(kNameSpaceID_None,
                                   nsGkAtoms::mozdirty);

  nsIAtom *name = aContent->Tag();
  PRInt32 namespaceID = aContent->GetNameSpaceID();

  // this method is not called by nsHTMLContentSerializer
  // so we don't have to check HTML element, just XHTML
  if (namespaceID == kNameSpaceID_XHTML) {
    if (mIsCopying && name == nsGkAtoms::ol) {
      NS_ASSERTION((!mOLStateStack.IsEmpty()), "Cannot have an empty OL Stack");
      /* Though at this point we must always have an state to be deleted as all 
      the OL opening tags are supposed to push an olState object to the stack*/
      if (!mOLStateStack.IsEmpty()) {
        mOLStateStack.RemoveElementAt(mOLStateStack.Length() -1);
      }
    }

    if (HasNoChildren(aContent)) {
      nsIParserService* parserService = nsContentUtils::GetParserService();

      if (parserService) {
        bool isContainer;

        parserService->
          IsContainer(parserService->HTMLCaseSensitiveAtomTagToId(name),
                      isContainer);
        if (!isContainer) {
          // non-container HTML elements are already closed,
          // see AppendEndOfElementStart
          return PR_FALSE;
        }
      }
    }
    // for backward compatibility with old HTML user agents,
    // empty elements should have an ending tag, so we mustn't call
    // nsXMLContentSerializer::CheckElementEnd
    return PR_TRUE;
  }

  bool dummyFormat;
  return nsXMLContentSerializer::CheckElementEnd(aContent, dummyFormat, aStr);
}

void
nsXHTMLContentSerializer::AppendAndTranslateEntities(const nsAString& aStr,
                                                     nsAString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  if (mDisableEntityEncoding) {
    aOutputStr.Append(aStr);
    return;
  }
 
  nsXMLContentSerializer::AppendAndTranslateEntities(aStr, aOutputStr);
}

bool
nsXHTMLContentSerializer::IsShorthandAttr(const nsIAtom* aAttrName,
                                          const nsIAtom* aElementName)
{
  // checked
  if ((aAttrName == nsGkAtoms::checked) &&
      (aElementName == nsGkAtoms::input)) {
    return PR_TRUE;
  }

  // compact
  if ((aAttrName == nsGkAtoms::compact) &&
      (aElementName == nsGkAtoms::dir || 
       aElementName == nsGkAtoms::dl ||
       aElementName == nsGkAtoms::menu ||
       aElementName == nsGkAtoms::ol ||
       aElementName == nsGkAtoms::ul)) {
    return PR_TRUE;
  }

  // declare
  if ((aAttrName == nsGkAtoms::declare) &&
      (aElementName == nsGkAtoms::object)) {
    return PR_TRUE;
  }

  // defer
  if ((aAttrName == nsGkAtoms::defer) &&
      (aElementName == nsGkAtoms::script)) {
    return PR_TRUE;
  }

  // disabled
  if ((aAttrName == nsGkAtoms::disabled) &&
      (aElementName == nsGkAtoms::button ||
       aElementName == nsGkAtoms::input ||
       aElementName == nsGkAtoms::optgroup ||
       aElementName == nsGkAtoms::option ||
       aElementName == nsGkAtoms::select ||
       aElementName == nsGkAtoms::textarea)) {
    return PR_TRUE;
  }

  // ismap
  if ((aAttrName == nsGkAtoms::ismap) &&
      (aElementName == nsGkAtoms::img ||
       aElementName == nsGkAtoms::input)) {
    return PR_TRUE;
  }

  // multiple
  if ((aAttrName == nsGkAtoms::multiple) &&
      (aElementName == nsGkAtoms::select)) {
    return PR_TRUE;
  }

  // noresize
  if ((aAttrName == nsGkAtoms::noresize) &&
      (aElementName == nsGkAtoms::frame)) {
    return PR_TRUE;
  }

  // noshade
  if ((aAttrName == nsGkAtoms::noshade) &&
      (aElementName == nsGkAtoms::hr)) {
    return PR_TRUE;
  }

  // nowrap
  if ((aAttrName == nsGkAtoms::nowrap) &&
      (aElementName == nsGkAtoms::td ||
       aElementName == nsGkAtoms::th)) {
    return PR_TRUE;
  }

  // readonly
  if ((aAttrName == nsGkAtoms::readonly) &&
      (aElementName == nsGkAtoms::input ||
       aElementName == nsGkAtoms::textarea)) {
    return PR_TRUE;
  }

  // selected
  if ((aAttrName == nsGkAtoms::selected) &&
      (aElementName == nsGkAtoms::option)) {
    return PR_TRUE;
  }

#ifdef MOZ_MEDIA
  // autoplay and controls
  if ((aElementName == nsGkAtoms::video || aElementName == nsGkAtoms::audio) &&
    (aAttrName == nsGkAtoms::autoplay ||
     aAttrName == nsGkAtoms::controls)) {
    return PR_TRUE;
  }
#endif

  return PR_FALSE;
}

bool
nsXHTMLContentSerializer::LineBreakBeforeOpen(PRInt32 aNamespaceID, nsIAtom* aName)
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
    return PR_TRUE;
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
nsXHTMLContentSerializer::LineBreakAfterOpen(PRInt32 aNamespaceID, nsIAtom* aName)
{

  if (aNamespaceID != kNameSpaceID_XHTML) {
    return PR_FALSE;
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
    return PR_TRUE;
  }

  return PR_FALSE;
}

bool 
nsXHTMLContentSerializer::LineBreakBeforeClose(PRInt32 aNamespaceID, nsIAtom* aName)
{

  if (aNamespaceID != kNameSpaceID_XHTML) {
    return PR_FALSE;
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
    return PR_TRUE;
  }
  return PR_FALSE;
}

bool 
nsXHTMLContentSerializer::LineBreakAfterClose(PRInt32 aNamespaceID, nsIAtom* aName)
{

  if (aNamespaceID != kNameSpaceID_XHTML) {
    return PR_FALSE;
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
    return PR_TRUE;
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

  return PR_FALSE;
}


void
nsXHTMLContentSerializer::MaybeEnterInPreContent(nsIContent* aNode)
{

  if (aNode->GetNameSpaceID() != kNameSpaceID_XHTML) {
    return;
  }

  nsIAtom *name = aNode->Tag();

  if (name == nsGkAtoms::pre ||
      name == nsGkAtoms::script ||
      name == nsGkAtoms::style ||
      name == nsGkAtoms::noscript ||
      name == nsGkAtoms::noframes
      ) {
    mPreLevel++;
  }
}

void
nsXHTMLContentSerializer::MaybeLeaveFromPreContent(nsIContent* aNode)
{
  if (aNode->GetNameSpaceID() != kNameSpaceID_XHTML) {
    return;
  }

  nsIAtom *name = aNode->Tag();
  if (name == nsGkAtoms::pre ||
      name == nsGkAtoms::script ||
      name == nsGkAtoms::style ||
      name == nsGkAtoms::noscript ||
      name == nsGkAtoms::noframes
    ) {
    --mPreLevel;
  }
}

void 
nsXHTMLContentSerializer::SerializeLIValueAttribute(nsIContent* aElement,
                                                    nsAString& aStr)
{
  // We are copying and we are at the "first" LI node of OL in selected range.
  // It may not be the first LI child of OL but it's first in the selected range.
  // Note that we get into this condition only once per a OL.
  bool found = false;
  nsCOMPtr<nsIDOMNode> currNode = do_QueryInterface(aElement);
  nsAutoString valueStr;

  olState state (0, PR_FALSE);

  if (!mOLStateStack.IsEmpty()) {
    state = mOLStateStack[mOLStateStack.Length()-1];
    // isFirstListItem should be true only before the serialization of the
    // first item in the list.
    state.isFirstListItem = PR_FALSE;
    mOLStateStack[mOLStateStack.Length()-1] = state;
  }

  PRInt32 startVal = state.startVal;
  PRInt32 offset = 0;

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
          found = PR_TRUE;
          PRInt32 rv = 0;
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
    SerializeAttr(EmptyString(), NS_LITERAL_STRING("value"), valueStr, aStr, PR_FALSE);
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
    SerializeAttr(EmptyString(), NS_LITERAL_STRING("value"), valueStr, aStr, PR_FALSE);
  }
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
    return PR_FALSE;

  if (parentName.LowerCaseEqualsLiteral("ol")) {

    if (!mOLStateStack.IsEmpty()) {
      olState state = mOLStateStack[mOLStateStack.Length()-1];
      if (state.isFirstListItem)
        return PR_TRUE;
    }

    return PR_FALSE;
  }
  else
    return PR_FALSE;
}

bool
nsXHTMLContentSerializer::HasNoChildren(nsIContent * aContent) {

  for (nsIContent* child = aContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
       
    if (!child->IsNodeOfType(nsINode::eTEXT))
      return PR_FALSE;

    if (child->TextLength())
      return PR_FALSE;
  }

  return PR_TRUE;
}
