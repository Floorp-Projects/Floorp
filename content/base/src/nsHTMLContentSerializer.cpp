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

#include "nsHTMLContentSerializer.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsParserCIID.h"
#include "nsIServiceManager.h"
#include "nsIDocumentEncoder.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsITextToSubURI.h"

static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);

#define kIndentStr NS_LITERAL_STRING("  ")
#define kMozStr "_moz"
#define kLessThan NS_LITERAL_STRING("<")
#define kGreaterThan NS_LITERAL_STRING(">")
#define kEndTag NS_LITERAL_STRING("</")

static const PRInt32 kLongLineLen = 128;

nsresult NS_NewHTMLContentSerializer(nsIContentSerializer** aSerializer)
{
  nsHTMLContentSerializer* it = new nsHTMLContentSerializer();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsIContentSerializer), (void**)aSerializer);
}

nsHTMLContentSerializer::nsHTMLContentSerializer()
{
  mColPos = 0;
  mIndent = 0;
  mInBody = PR_FALSE;
  mInCDATA = PR_FALSE;
}

nsHTMLContentSerializer::~nsHTMLContentSerializer()
{
}

nsresult
nsHTMLContentSerializer::GetParserService(nsIParserService** aParserService)
{
  if (!mParserService) {
    nsresult rv;
    mParserService = do_GetService(kParserServiceCID, &rv);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }

  CallQueryInterface(mParserService.get(), aParserService);
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLContentSerializer::Init(PRUint32 aFlags, PRUint32 aWrapColumn,
                              nsIAtom* aCharSet)
{
  mFlags = aFlags;
  if (!aWrapColumn) {
    mMaxColumn = 72;
  }
  else {
    mMaxColumn = aWrapColumn;
  }

  mDoFormat = (mFlags & nsIDocumentEncoder::OutputFormatted) ? PR_TRUE
                                                             : PR_FALSE;
  mBodyOnly = (mFlags & nsIDocumentEncoder::OutputBodyOnly) ? PR_TRUE
                                                            : PR_FALSE;
  // Set the line break character:
  if ((mFlags & nsIDocumentEncoder::OutputCRLineBreak)
      && (mFlags & nsIDocumentEncoder::OutputLFLineBreak)) { // Windows
    mLineBreak.AssignWithConversion("\r\n");
  }
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) { // Mac
    mLineBreak.AssignWithConversion("\r");
  }
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) { // Unix/DOM
    mLineBreak.AssignWithConversion("\n");
  }
  else {
    mLineBreak.AssignWithConversion(NS_LINEBREAK);         // Platform/default
  }

  mPreLevel = 0;

  mCharSet = aCharSet;
  mIsLatin1 = PR_FALSE;
  if (aCharSet) {
    const PRUnichar *charset;
    aCharSet->GetUnicode(&charset);

    if (NS_LITERAL_STRING("ISO-8859-1").Equals(charset)) {
      mIsLatin1 = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLContentSerializer::AppendText(nsIDOMText* aText, 
                                    PRInt32 aStartOffset,
                                    PRInt32 aEndOffset,
                                    nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aText);

  nsAutoString data;

  nsresult rv;
  rv = AppendTextData((nsIDOMNode*)aText, aStartOffset, 
                      aEndOffset, data, PR_TRUE, PR_FALSE);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  PRInt32 lastNewlineOffset = kNotFound;
  PRBool hasLongLines = HasLongLines(data, lastNewlineOffset);

  if (mPreLevel || (!mDoFormat && !hasLongLines) ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
    AppendToString(data, aStr);

    if (lastNewlineOffset != kNotFound) {
      mColPos = data.Length() - lastNewlineOffset;
    }   
  }
  else {
    AppendToStringWrapped(data, aStr, PR_FALSE);
  }

  return NS_OK;
}

PRBool
nsHTMLContentSerializer::IsJavaScript(nsIAtom* aAttrNameAtom, const nsAReadableString& aValueString)
{
  if (aAttrNameAtom == nsHTMLAtoms::href
  || aAttrNameAtom == nsHTMLAtoms::src) {
    // note that there is a problem in that if this value starts with leading spaces we won't do the right thing
    // this is covered in bug #59604
    static const char kJavaScript[] = "javascript";
    PRInt32 pos = aValueString.FindChar(':');
    nsAutoString scheme;
    if ((pos == (PRInt32)(sizeof kJavaScript - 1)) &&
        (aValueString.Left(scheme, pos) != -1) &&
        scheme.EqualsIgnoreCase(kJavaScript))
      return PR_TRUE;
    else
      return PR_FALSE;  
  }

  PRBool result = 
                 (aAttrNameAtom == nsLayoutAtoms::onblur)      || (aAttrNameAtom == nsLayoutAtoms::onchange)
              || (aAttrNameAtom == nsLayoutAtoms::onclick)     || (aAttrNameAtom == nsLayoutAtoms::ondblclick)
              || (aAttrNameAtom == nsLayoutAtoms::onfocus)     || (aAttrNameAtom == nsLayoutAtoms::onkeydown)
              || (aAttrNameAtom == nsLayoutAtoms::onkeypress)  || (aAttrNameAtom == nsLayoutAtoms::onkeyup)
              || (aAttrNameAtom == nsLayoutAtoms::onload)      || (aAttrNameAtom == nsLayoutAtoms::onmousedown)
              || (aAttrNameAtom == nsLayoutAtoms::onmousemove) || (aAttrNameAtom == nsLayoutAtoms::onmouseout)
              || (aAttrNameAtom == nsLayoutAtoms::onmouseover) || (aAttrNameAtom == nsLayoutAtoms::onmouseup)
              || (aAttrNameAtom == nsLayoutAtoms::onreset)     || (aAttrNameAtom == nsLayoutAtoms::onselect)
              || (aAttrNameAtom == nsLayoutAtoms::onsubmit)    || (aAttrNameAtom == nsLayoutAtoms::onunload)
              || (aAttrNameAtom == nsLayoutAtoms::onabort)     || (aAttrNameAtom == nsLayoutAtoms::onerror)
              || (aAttrNameAtom == nsLayoutAtoms::onpaint)     || (aAttrNameAtom == nsLayoutAtoms::onresize)
              || (aAttrNameAtom == nsLayoutAtoms::onscroll)    || (aAttrNameAtom == nsLayoutAtoms::onbroadcast)
              || (aAttrNameAtom == nsLayoutAtoms::onclose)     || (aAttrNameAtom == nsLayoutAtoms::oncontextmenu)
              || (aAttrNameAtom == nsLayoutAtoms::oncommand)   || (aAttrNameAtom == nsLayoutAtoms::oncommandupdate)
              || (aAttrNameAtom == nsLayoutAtoms::ondragdrop)  || (aAttrNameAtom == nsLayoutAtoms::ondragenter)
              || (aAttrNameAtom == nsLayoutAtoms::ondragexit)  || (aAttrNameAtom == nsLayoutAtoms::ondraggesture)
              || (aAttrNameAtom == nsLayoutAtoms::ondragover)  || (aAttrNameAtom == nsLayoutAtoms::oninput);
  return result;
}

nsresult 
nsHTMLContentSerializer::EscapeURI(const nsAReadableString& aURI, nsAWritableString& aEscapedURI)
{
  // URL escape %xx cannot be used in JS.
  // No escaping if the scheme is 'javascript'.
  if (IsJavaScript(nsHTMLAtoms::href, aURI)) {
    aEscapedURI = aURI;
    return NS_OK;
  }

  // nsITextToSubURI does charset convert plus uri escape
  // This is needed to convert to a document charset which is needed to support existing browsers.
  // But we eventually want to use UTF-8 instead of a document charset, then the code would be much simpler.
  // See HTML 4.01 spec, "Appendix B.2.1 Non-ASCII characters in URI attribute values"
  nsCOMPtr<nsITextToSubURI> textToSubURI;
  nsAutoString uri(aURI); // in order to use FindCharInSet(), IsASCII()
  nsXPIDLCString documentCharset;
  nsresult rv = NS_OK;


  if (mCharSet && !uri.IsASCII()) {
    const PRUnichar *charset;
    mCharSet->GetUnicode(&charset);
    documentCharset.Adopt(ToNewUTF8String(nsDependentString(charset)));
    textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt32 start = 0;
  PRInt32 end;
  nsAutoString part;
  nsXPIDLCString escapedURI;
  aEscapedURI.Truncate(0);

  // Loop and escape parts by avoiding escaping reserved characters (and '%', '#' ).
  while ((end = uri.FindCharInSet("%#;/?:@&=+$,", start)) != -1) {
    aURI.Mid(part, start, (end-start));
    if (textToSubURI && !part.IsASCII()) {
      rv = textToSubURI->ConvertAndEscape(documentCharset, part.get(), getter_Copies(escapedURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      escapedURI.Adopt(nsEscape(NS_ConvertUCS2toUTF8(part).get(), url_Path));
    }
    aEscapedURI.Append(NS_ConvertASCIItoUCS2(escapedURI));

    // Append a reserved character without escaping.
    aURI.Mid(part, end, 1);
    aEscapedURI.Append(part);
    start = end + 1;
  }

  if (start < (PRInt32) aURI.Length()) {
    // Escape the remaining part.
    aURI.Mid(part, start, aURI.Length()-start);
    if (textToSubURI) {
      rv = textToSubURI->ConvertAndEscape(documentCharset, part.get(), getter_Copies(escapedURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      escapedURI.Adopt(nsEscape(NS_ConvertUCS2toUTF8(part).get(), url_Path));
    }
    aEscapedURI.Append(NS_ConvertASCIItoUCS2(escapedURI));
  }

  return rv;
}

void 
nsHTMLContentSerializer::SerializeAttributes(nsIContent* aContent,
                                             nsIAtom* aTagName,
                                             nsAWritableString& aStr)
{
  nsresult rv;
  PRInt32 index, count;
  nsAutoString nameStr, valueStr;
  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> attrName, attrPrefix;

  aContent->GetAttrCount(count);

  for (index = 0; index < count; index++) {
    aContent->GetAttrNameAt(index, 
                            namespaceID,
                            *getter_AddRefs(attrName),
                            *getter_AddRefs(attrPrefix));

    // Filter out any attribute starting with _moz
    const PRUnichar* sharedName;
    attrName->GetUnicode(&sharedName);
    if (nsCRT::strncmp(sharedName, 
                       NS_ConvertASCIItoUCS2(kMozStr).get(), 
                       sizeof(kMozStr)-1) == 0) {
      continue;
    }

    aContent->GetAttr(namespaceID, attrName, valueStr);

    // 
    // Filter out special case of <br type="_moz"> or <br _moz*>,
    // used by the editor.  Bug 16988.  Yuck.
    //
    if ((aTagName == nsHTMLAtoms::br) &&
        (attrName.get() == nsHTMLAtoms::type) &&
        (valueStr.EqualsWithConversion(kMozStr, PR_FALSE, sizeof(kMozStr)-1))) {
      continue;
    }
    
    // XXX: This special cased textarea code should be
    //      removed when bug #17003 is fixed.  
    if ( (aTagName == nsHTMLAtoms::textarea) &&
         ((attrName.get() == nsHTMLAtoms::value) || 
         (attrName.get() == nsHTMLAtoms::defaultvalue)) ){
        continue;
    }

    PRBool isJS = IsJavaScript(attrName, valueStr);
    
    if (((attrName.get() == nsHTMLAtoms::href) || 
         (attrName.get() == nsHTMLAtoms::src))) {
      // Make all links absolute when converting only the selection:
      if (mFlags & nsIDocumentEncoder::OutputAbsoluteLinks) {
        // Would be nice to handle OBJECT and APPLET tags,
        // but that gets more complicated since we have to
        // search the tag list for CODEBASE as well.
        // For now, just leave them relative.
        nsCOMPtr<nsIDocument> document;
        aContent->GetDocument(*getter_AddRefs(document));
        if (document) {
          nsCOMPtr<nsIURI> uri;
          document->GetDocumentURL(getter_AddRefs(uri));
          if (uri) {
            nsAutoString absURI;
            rv = NS_MakeAbsoluteURI(absURI, valueStr, uri);
            if (NS_SUCCEEDED(rv)) {
              valueStr = absURI;
            }
          }
        }
      }
      // Need to escape URI.
      nsAutoString tempURI(valueStr);
      if (!isJS && NS_FAILED(EscapeURI(tempURI, valueStr)))
        valueStr = tempURI;
    }
  
    attrName->ToString(nameStr);

    SerializeAttr(nsAutoString(), nameStr, valueStr, aStr, !isJS);
  }
}

NS_IMETHODIMP
nsHTMLContentSerializer::AppendElementStart(nsIDOMElement *aElement,
                                            nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (!content) return NS_ERROR_FAILURE;
  
  // The _moz_dirty attribute is emitted by the editor to
  // indicate that this element should be pretty printed
  // even if we're not in pretty printing mode
  PRBool hasDirtyAttr = HasDirtyAttr(content);

  nsCOMPtr<nsIAtom> name;
  content->GetTag(*getter_AddRefs(name));

  if (name.get() == nsHTMLAtoms::body) {
    mInBody = PR_TRUE;
  }

  if (LineBreakBeforeOpen(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
  }

  StartIndentation(name, hasDirtyAttr, aStr);

  if ((name.get() == nsHTMLAtoms::pre) ||
      (name.get() == nsHTMLAtoms::script) ||
      (name.get() == nsHTMLAtoms::style)) {
    mPreLevel++;
  }
  
  AppendToString(kLessThan, aStr);

  const PRUnichar* sharedName;
  name->GetUnicode(&sharedName);
  AppendToString(sharedName, -1, aStr);

  SerializeAttributes(content, name, aStr);

  AppendToString(kGreaterThan, aStr);

  if (LineBreakAfterOpen(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
  }

  // XXX: This special cased textarea code should be
  //      removed when bug #17003 is fixed.  
  if (name.get() == nsHTMLAtoms::textarea)
  {
    nsAutoString valueStr;
    content->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::value, valueStr);
    AppendToString(valueStr, aStr);
  }

  if ((name.get() == nsHTMLAtoms::script) ||
      (name.get() == nsHTMLAtoms::style) ||
      (name.get() == nsHTMLAtoms::noscript)) {
    mInCDATA = PR_TRUE;
  }

  return NS_OK;
}
  
NS_IMETHODIMP 
nsHTMLContentSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                          nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (!content) return NS_ERROR_FAILURE;

  PRBool hasDirtyAttr = HasDirtyAttr(content);

  nsCOMPtr<nsIAtom> name;
  content->GetTag(*getter_AddRefs(name));

  if ((name.get() == nsHTMLAtoms::pre) ||
      (name.get() == nsHTMLAtoms::script) ||
      (name.get() == nsHTMLAtoms::style)) {
    mPreLevel--;
  }

  const PRUnichar* sharedName;
  name->GetUnicode(&sharedName);

  nsCOMPtr<nsIParserService> parserService;
  GetParserService(getter_AddRefs(parserService));

  if (parserService && (name.get() != nsHTMLAtoms::style)) {
    nsAutoString nameStr(sharedName);
    PRBool isContainer;
    PRInt32 id;

    parserService->HTMLStringTagToId(nameStr, &id);
    parserService->IsContainer(id, isContainer);
    if (!isContainer) return NS_OK;
  }

  if (LineBreakBeforeClose(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
  }

  EndIndentation(name, hasDirtyAttr, aStr);

  AppendToString(kEndTag, aStr);
  AppendToString(sharedName, -1, aStr);
  AppendToString(kGreaterThan, aStr);

  if (LineBreakAfterClose(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
  }

  mInCDATA = PR_FALSE;

  return NS_OK;
}

void
nsHTMLContentSerializer::AppendToString(const PRUnichar* aStr,
                                        PRInt32 aLength,
                                        nsAWritableString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  PRInt32 length = (aLength == -1) ? nsCRT::strlen(aStr) : aLength;
  
  mColPos += length;

  aOutputStr.Append(aStr, length);
}

void 
nsHTMLContentSerializer::AppendToString(const PRUnichar aChar,
                                        nsAWritableString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  mColPos += 1;

  aOutputStr.Append(aChar);
}

void 
nsHTMLContentSerializer::AppendToStringWrapped(const nsAReadableString& aStr,
                                               nsAWritableString& aOutputStr,
                                               PRBool aTranslateEntities)
{
  PRInt32 length = aStr.Length();

  nsAutoString line;
  PRBool    done = PR_FALSE;
  PRInt32   indx = 0;
  PRInt32   strOffset = 0;
  PRInt32   lineLength, oldLineEnd;
  
  // Make sure we haven't gone too far already
  if (mColPos > mMaxColumn) {
    AppendToString(mLineBreak, aOutputStr);
    mColPos = 0;
  }
  
  // Find the end of the first old line
  oldLineEnd = aStr.FindChar(PRUnichar('\n'), 0);
  
  while ((!done) && (strOffset < length)) {
    // This is how much is needed to fill up the new line
    PRInt32 leftInLine = mMaxColumn - mColPos;
    
    // This is the last position in the current old line
    PRInt32 oldLineLimit;
    if (oldLineEnd == kNotFound) {
      oldLineLimit = length;
    }
    else {
      oldLineLimit = oldLineEnd;
    }
    
    PRBool addLineBreak = PR_FALSE;

    // if we can fill up the new line with less than what's
    // in the current old line...
    if ((strOffset + leftInLine) < oldLineLimit) {
      addLineBreak = PR_TRUE;
      
      // Look for the next word end to break
      indx = aStr.FindChar(PRUnichar(' '), strOffset + leftInLine);
      
      // If it's after the end of the current line, then break at
      // the current line
      if ((indx == kNotFound) || 
          ((oldLineEnd != kNotFound) && (oldLineEnd < indx))) {
        indx = oldLineEnd;
      }
    }
    else {
      indx = oldLineEnd;
    }
    
    // if there was no place to break, then just add the entire string
    if (indx == kNotFound) {
      if (strOffset == 0) {
        AppendToString(aStr, aOutputStr, aTranslateEntities);
      }
      else {
        lineLength = length - strOffset;
        aStr.Right(line, lineLength);
        AppendToString(line, aOutputStr, aTranslateEntities);
      }
      done = PR_TRUE;
    }
    else {
      // Add the part of the current old line that's part of the 
      // new line
      lineLength = indx - strOffset;
      aStr.Mid(line, strOffset, lineLength);
      AppendToString(line, aOutputStr, aTranslateEntities);
      
      // if we've reached the end of an old line, don't add the
      // old line break and find the end of the next old line.
      if (indx == oldLineEnd) {
        oldLineEnd = aStr.FindChar(PRUnichar('\n'), indx+1);
        AppendToString(NS_LITERAL_STRING(" "), aOutputStr);
      }
      
      if (addLineBreak) {
        AppendToString(mLineBreak, aOutputStr);
        mColPos = 0;
      }
      strOffset = indx+1;
    }
  }
}

static PRUint16 kGTVal = 62;
static const char* kEntities[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "amp", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "lt", "", "gt"
};

static const char* kAttrEntities[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "quot", "", "", "", "amp", "apos",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "lt", "", "gt"
};

void
nsHTMLContentSerializer::AppendToString(const nsAReadableString& aStr,
                                        nsAWritableString& aOutputStr,
                                        PRBool aTranslateEntities,
                                        PRBool aIncrColumn)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  if (aIncrColumn) {
    mColPos += aStr.Length();
  }

  if (aTranslateEntities && !mInCDATA) {
    if (mFlags & nsIDocumentEncoder::OutputEncodeEntities) {
      nsCOMPtr<nsIParserService> parserService;
      GetParserService(getter_AddRefs(parserService));

      if (!parserService) {
        NS_ERROR("Can't get parser service");
        return;
      }

      nsReadingIterator<PRUnichar> done_reading;
      aStr.EndReading(done_reading);

      // for each chunk of |aString|...
      PRUint32 advanceLength = 0;
      nsReadingIterator<PRUnichar> iter;

      const char **entityTable = mInAttribute ? kAttrEntities : kEntities;

      for (aStr.BeginReading(iter); 
           iter != done_reading; 
           iter.advance(PRInt32(advanceLength))) {
        PRUint32 fragmentLength = iter.size_forward();
        const PRUnichar* c = iter.get();
        const PRUnichar* fragmentStart = c;
        const PRUnichar* fragmentEnd = c + fragmentLength;
        const char* entityText = nsnull;
        nsCAutoString entityReplacement;

        advanceLength = 0;
        // for each character in this chunk, check if it
        // needs to be replaced
        for (; c < fragmentEnd; c++, advanceLength++) {
          PRUnichar val = *c;
          if ((val <= kGTVal) && (entityTable[val][0] != 0)) {
            entityText = entityTable[val];
            break;
          } else if (mIsLatin1 && val > 127 && val < 256) {
            parserService->HTMLConvertUnicodeToEntity(val, entityReplacement);

            if (entityReplacement.Length() > 0) {
              entityText = entityReplacement.get();
              break;
            }
          }
        }

        aOutputStr.Append(fragmentStart, advanceLength);
        if (entityText) {
          aOutputStr.Append(PRUnichar('&'));
          aOutputStr.Append(NS_ConvertASCIItoUCS2(entityText));
          aOutputStr.Append(PRUnichar(';'));
          advanceLength++;
        }
      }
    } else {
      nsXMLContentSerializer::AppendToString(aStr, aOutputStr, aTranslateEntities, aIncrColumn);
    }

    return;
  }

  aOutputStr.Append(aStr);
}

PRBool
nsHTMLContentSerializer::HasDirtyAttr(nsIContent* aContent)
{
  nsAutoString val;

  if (NS_CONTENT_ATTR_NOT_THERE != aContent->GetAttr(kNameSpaceID_None,
                                                     nsLayoutAtoms::mozdirty,
                                                     val)) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

PRBool 
nsHTMLContentSerializer::LineBreakBeforeOpen(nsIAtom* aName, 
                                             PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel || !mColPos ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
    return PR_FALSE;
  }
        
  if (aName == nsHTMLAtoms::title ||
      aName == nsHTMLAtoms::meta  ||
      aName == nsHTMLAtoms::link  ||
      aName == nsHTMLAtoms::style ||
      aName == nsHTMLAtoms::select ||
      aName == nsHTMLAtoms::option ||
      aName == nsHTMLAtoms::script ||
      aName == nsHTMLAtoms::html) {
    return PR_TRUE;
  }
  else {
    nsCOMPtr<nsIParserService> parserService;
    GetParserService(getter_AddRefs(parserService));
    
    if (parserService) {
      nsAutoString str;
      aName->ToString(str);
      PRBool res;
      PRInt32 id;

      parserService->HTMLStringTagToId(str, &id);
      parserService->IsBlock(id, res);
      return res;
    }
  }

  return PR_FALSE;
}

PRBool 
nsHTMLContentSerializer::LineBreakAfterOpen(nsIAtom* aName, 
                                            PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
    return PR_FALSE;
  }

  if ((aName == nsHTMLAtoms::html) ||
      (aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::body) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::dl) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::br) ||
      (aName == nsHTMLAtoms::meta) ||
      (aName == nsHTMLAtoms::link) ||
      (aName == nsHTMLAtoms::script) ||
      (aName == nsHTMLAtoms::select) ||
      (aName == nsHTMLAtoms::img) ||
      (aName == nsHTMLAtoms::map) ||
      (aName == nsHTMLAtoms::area) ||
      (aName == nsHTMLAtoms::style)) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool 
nsHTMLContentSerializer::LineBreakBeforeClose(nsIAtom* aName, 
                                              PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel || !mColPos ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
    return PR_FALSE;
  }

  if ((aName == nsHTMLAtoms::html) ||
      (aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::body) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::dl) ||
      (aName == nsHTMLAtoms::select) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tbody)) {
    return PR_TRUE;
  }
  
  return PR_FALSE;
}

PRBool 
nsHTMLContentSerializer::LineBreakAfterClose(nsIAtom* aName, 
                                             PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
    return PR_FALSE;
  }

  if ((aName == nsHTMLAtoms::html) ||
      (aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::body) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::th) ||
      (aName == nsHTMLAtoms::td) ||
      (aName == nsHTMLAtoms::pre) ||
      (aName == nsHTMLAtoms::title) ||
      (aName == nsHTMLAtoms::li) ||
      (aName == nsHTMLAtoms::dt) ||
      (aName == nsHTMLAtoms::dd) ||
      (aName == nsHTMLAtoms::blockquote) ||
      (aName == nsHTMLAtoms::select) ||
      (aName == nsHTMLAtoms::option) ||
      (aName == nsHTMLAtoms::p) ||
      (aName == nsHTMLAtoms::map) ||
      (aName == nsHTMLAtoms::div)) {
    return PR_TRUE;
  }
  else {
    nsCOMPtr<nsIParserService> parserService;
    GetParserService(getter_AddRefs(parserService));
    
    if (parserService) {
      nsAutoString str;
      aName->ToString(str);
      PRBool res;
      PRInt32 id;

      parserService->HTMLStringTagToId(str, &id);
      parserService->IsBlock(id, res);
      return res;
    }
  }

  return PR_FALSE;
}

void
nsHTMLContentSerializer::StartIndentation(nsIAtom* aName,
                                          PRBool aHasDirtyAttr,
                                          nsAWritableString& aStr)
{
  if ((mDoFormat || aHasDirtyAttr) && !mPreLevel && !mColPos) {
    for (PRInt32 i = mIndent; --i >= 0; ) {
      AppendToString(kIndentStr, aStr);
    }
  }

  if ((aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::form) ||
      (aName == nsHTMLAtoms::frameset) ||
      (aName == nsHTMLAtoms::blockquote) ||
      (aName == nsHTMLAtoms::li) ||
      (aName == nsHTMLAtoms::dt) ||
      (aName == nsHTMLAtoms::dd)) {
    mIndent++;
  }
}

void
nsHTMLContentSerializer::EndIndentation(nsIAtom* aName,
                                        PRBool aHasDirtyAttr,
                                        nsAWritableString& aStr)
{
  if ((aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::li) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::form) ||
      (aName == nsHTMLAtoms::frameset)) {
    mIndent--;
  }

  if ((mDoFormat || aHasDirtyAttr) && !mPreLevel && !mColPos) {
    for (PRInt32 i = mIndent; --i >= 0; ) {
      AppendToString(kIndentStr, aStr);
    }
  }
}

// See if the string has any lines longer than longLineLen:
// if so, we presume formatting is wonky (e.g. the node has been edited)
// and we'd better rewrap the whole text node.
PRBool 
nsHTMLContentSerializer::HasLongLines(const nsString& text, PRInt32& aLastNewlineOffset)
{
  PRUint32 start=0;
  PRUint32 theLen=text.Length();
  PRBool rv = PR_FALSE;
  aLastNewlineOffset = kNotFound;
  for (start = 0; start < theLen; )
  {
    PRInt32 eol = text.FindChar('\n', PR_FALSE, start);
    if (eol < 0) {
      eol = text.Length();
    }
    else {
      aLastNewlineOffset = eol;
    }
    if (PRInt32(eol - start) > kLongLineLen)
      rv = PR_TRUE;
    start = eol+1;
  }
  return rv;
}
