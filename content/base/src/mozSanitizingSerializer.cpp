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
 * The Original Code is mozilla.org HTML Sanitizer code.
 *
 * The Initial Developer of the Original Code is
 * Ben Bucksch <mozilla@bucksch.org>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Netscape
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
 * A serializer and content sink that removes potentially insecure or
 * otherwise dangerous or offending HTML (eg for display of HTML
 * e-mail attachments or something).
 */

/* I used nsPlaintextSerializer as base for this class. I don't understand
   all of the functions in the beginning. Possible that I fail to do
   something or do something useless.
   I am not proud about the implementation here at all.
   Feel free to fix it :-).
*/

#include "mozSanitizingSerializer.h"
#include "nsIServiceManager.h"
#include "nsIDOMElement.h"
#include "nsTextFragment.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "plstr.h"
#include "nsIProperties.h"
#include "nsUnicharUtils.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::dom;

static inline PRUnichar* escape(const nsString& source)
{
  return nsEscapeHTML2(source.get(), source.Length()); 
}

/* XXX: |printf|s in some error conditions. They are intended as information
   for the user, because they complain about malformed pref values.
   Not sure, if popping up dialog boxes is the right thing for such code
   (and if so, how to do it).
 */

#define TEXT_REMOVED "&lt;Text removed&gt;"
#define TEXT_BREAKER "|"

nsresult NS_NewSanitizingHTMLSerializer(nsIContentSerializer** aSerializer)
{
  mozSanitizingHTMLSerializer* it = new mozSanitizingHTMLSerializer();
  NS_ADDREF(it);
  *aSerializer = it;
  return NS_OK;
}

mozSanitizingHTMLSerializer::mozSanitizingHTMLSerializer()
  : mSkipLevel(0),
    mAllowedTags(30) // Just some initial buffer size
{
  mOutputString = nsnull;
}

mozSanitizingHTMLSerializer::~mozSanitizingHTMLSerializer()
{
#ifdef DEBUG_BenB
  printf("Output:\n%s\n", NS_LossyConvertUTF16toASCII(*mOutputString).get());
#endif
  mAllowedTags.Enumerate(ReleaseProperties);
}

//<copy from="xpcom/ds/nsProperties.cpp">
bool
mozSanitizingHTMLSerializer::ReleaseProperties(nsHashKey* key, void* data,
                                               void* closure)
{
  nsIProperties* prop = (nsIProperties*)data;
  NS_IF_RELEASE(prop);
  return PR_TRUE;
}
//</copy>

NS_IMPL_ISUPPORTS4(mozSanitizingHTMLSerializer,
                   nsIContentSerializer,
                   nsIContentSink,
                   nsIHTMLContentSink,
                   mozISanitizingHTMLSerializer)


NS_IMETHODIMP 
mozSanitizingHTMLSerializer::Init(PRUint32 aFlags, PRUint32 dummy,
                                  const char* aCharSet, bool aIsCopying,
                                  bool aIsWholeDocument)
{
  NS_ENSURE_TRUE(nsContentUtils::GetParserService(), NS_ERROR_UNEXPECTED);

  return NS_OK;
}

NS_IMETHODIMP
mozSanitizingHTMLSerializer::Initialize(nsAString* aOutString,
                                        PRUint32 aFlags,
                                        const nsAString& allowedTags)
{
  nsresult rv = Init(aFlags, 0, nsnull, PR_FALSE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX This is wrong. It violates XPCOM string ownership rules.
  // We're only getting away with this because instances of this
  // class are restricted to single function scope.
  // (Comment copied from nsPlaintextSerializer)
  mOutputString = aOutString;

  ParsePrefs(allowedTags);

  return NS_OK;
}

// This is not used within the class, but maybe called from somewhere else?
NS_IMETHODIMP
mozSanitizingHTMLSerializer::Flush(nsAString& aStr)
{
#ifdef DEBUG_BenB
  printf("Flush: -%s-", NS_LossyConvertUTF16toASCII(aStr).get());
#endif
  Write(aStr);
  return NS_OK;
}

NS_IMETHODIMP
mozSanitizingHTMLSerializer::AppendDocumentStart(nsIDocument *aDocument,
                                                 nsAString& aStr)
{
  return NS_OK;
}

void
mozSanitizingHTMLSerializer::Write(const nsAString& aString)
{
  mOutputString->Append(aString);
}


NS_IMETHODIMP
mozSanitizingHTMLSerializer::IsEnabled(PRInt32 aTag, bool* aReturn)
{
  *aReturn = PR_FALSE;
  return NS_OK;
}


/**
 * Returns true, if the id represents a container
 */
bool
mozSanitizingHTMLSerializer::IsContainer(PRInt32 aId)
{
  bool isContainer = false;

  nsIParserService* parserService = nsContentUtils::GetParserService();
  if (parserService) {
    parserService->IsContainer(aId, isContainer);
  }

  return isContainer;
}


/* XXX I don't really know, what these functions do, but they seem to be
   needed ;-). Mostly copied from nsPlaintextSerializer. */
/* akk says:
   "I wonder if the sanitizing class could inherit from nsHTMLSerializer,
   so that at least these methods that none of us understand only have to be
   written once?" */

// static
PRInt32
mozSanitizingHTMLSerializer::GetIdForContent(nsIContent* aContent)
{
  if (!aContent->IsHTML()) {
    return eHTMLTag_unknown;
  }

  nsIParserService* parserService = nsContentUtils::GetParserService();

  return parserService ? parserService->HTMLAtomTagToId(aContent->Tag()) :
                         eHTMLTag_unknown;
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::AppendText(nsIContent* aText,
                                        PRInt32 aStartOffset,
                                        PRInt32 aEndOffset, 
                                        nsAString& aStr)
{
  nsresult rv = NS_OK;

  mOutputString = &aStr;

  nsAutoString linebuffer;
  rv = DoAddLeaf(eHTMLTag_text, linebuffer);

  return rv;
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::AppendElementStart(Element* aElement,
                                                Element* aOriginalElement,
                                                nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mElement = aElement;

  mOutputString = &aStr;

  PRInt32 id = GetIdForContent(mElement);

  bool isContainer = IsContainer(id);

  nsresult rv;
  if (isContainer) {
    rv = DoOpenContainer(id);
  }
  else {
    rv = DoAddLeaf(id, EmptyString());
  }

  mElement = nsnull;
  mOutputString = nsnull;

  return rv;
} 
 
NS_IMETHODIMP 
mozSanitizingHTMLSerializer::AppendElementEnd(Element* aElement,
                                              nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mElement = aElement;

  mOutputString = &aStr;

  PRInt32 id = GetIdForContent(mElement);

  bool isContainer = IsContainer(id);

  nsresult rv = NS_OK;
  if (isContainer) {
    rv = DoCloseContainer(id);
  }

  mElement = nsnull;
  mOutputString = nsnull;

  return rv;
}

NS_IMETHODIMP
mozSanitizingHTMLSerializer::OpenContainer(const nsIParserNode& aNode)
{
  PRInt32 type = aNode.GetNodeType();

  mParserNode = const_cast<nsIParserNode *>(&aNode);
  return DoOpenContainer(type);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::CloseContainer(const nsHTMLTag aTag)
{
  return DoCloseContainer(aTag);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::AddLeaf(const nsIParserNode& aNode)
{
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  const nsAString& text = aNode.GetText();

  mParserNode = const_cast<nsIParserNode*>(&aNode);
  return DoAddLeaf(type, text);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::AddDocTypeDecl(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::SetDocumentCharset(nsACString& aCharset)
{
  // No idea, if this works - it isn't invoked by |TestOutput|.
  Write(NS_LITERAL_STRING("\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=")
        /* Danger: breaking the line within the string literal, like
           "foo"\n"bar", breaks win32! */
        + nsAdoptingString(escape(NS_ConvertASCIItoUTF16(aCharset)))
        + NS_LITERAL_STRING("\">\n"));
  return NS_OK;
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::OpenHead()
{
  // XXX We don't have a parser node here, is it okay to ignore this?
  // return OpenContainer(aNode);
  return NS_OK;
}

// Here comes the actual code...

nsresult
mozSanitizingHTMLSerializer::DoOpenContainer(PRInt32 aTag)
{
  eHTMLTags type = (eHTMLTags)aTag;

  if (mSkipLevel == 0 && IsAllowedTag(type))
  {
    nsIParserService* parserService = nsContentUtils::GetParserService();
    if (!parserService)
      return NS_ERROR_OUT_OF_MEMORY;
    const PRUnichar* tag_name = parserService->HTMLIdToStringTag(aTag);
    NS_ENSURE_TRUE(tag_name, NS_ERROR_INVALID_POINTER);

    Write(NS_LITERAL_STRING("<") + nsDependentString(tag_name));

    // Attributes
    if (mParserNode)
    {
      PRInt32 count = mParserNode->GetAttributeCount();
      for (PRInt32 i = 0; i < count; i++)
      {
        const nsAString& key = mParserNode->GetKeyAt(i);
        if(IsAllowedAttribute(type, key))
        {
          // Ensure basic sanity of value
          nsAutoString value(mParserNode->GetValueAt(i));
                    // SanitizeAttrValue() modifies |value|
          if (NS_SUCCEEDED(SanitizeAttrValue(type, key, value)))
          {
            // Write out
            Write(NS_LITERAL_STRING(" "));
            Write(key); // I get an infinive loop with | + key + | !!!
            Write(NS_LITERAL_STRING("=\"") + value + NS_LITERAL_STRING("\""));
          }
        }
      }
    }

    Write(NS_LITERAL_STRING(">"));
  }
  else if (mSkipLevel != 0 || type == eHTMLTag_script || type == eHTMLTag_style)
    ++mSkipLevel;
  else
    Write(NS_LITERAL_STRING(" "));

  return NS_OK;

}

nsresult
mozSanitizingHTMLSerializer::DoCloseContainer(PRInt32 aTag)
{
  eHTMLTags type = (eHTMLTags)aTag;

  if (mSkipLevel == 0 && IsAllowedTag(type)) {
    nsIParserService* parserService = nsContentUtils::GetParserService();
    if (!parserService)
      return NS_ERROR_OUT_OF_MEMORY;
    const PRUnichar* tag_name = parserService->HTMLIdToStringTag(aTag);
    NS_ENSURE_TRUE(tag_name, NS_ERROR_INVALID_POINTER);

    Write(NS_LITERAL_STRING("</") + nsDependentString(tag_name)
          + NS_LITERAL_STRING(">"));
  }
  else if (mSkipLevel == 0)
    Write(NS_LITERAL_STRING(" "));
  else
    --mSkipLevel;

  return NS_OK;
}

nsresult
mozSanitizingHTMLSerializer::DoAddLeaf(PRInt32 aTag,
                                       const nsAString& aText)
{
  if (mSkipLevel != 0)
    return NS_OK;

  eHTMLTags type = (eHTMLTags)aTag;

  nsresult rv = NS_OK;

  if (type == eHTMLTag_whitespace ||
      type == eHTMLTag_newline)
  {
    Write(aText); // sure to be safe?
  }
  else if (type == eHTMLTag_text)
  {
    nsAutoString text(aText);
    if(NS_SUCCEEDED(SanitizeTextNode(text)))
      Write(text);
    else
      Write(NS_LITERAL_STRING(TEXT_REMOVED)); // Does not happen (yet)
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (type == eHTMLTag_entity)
  {
    Write(NS_LITERAL_STRING("&"));
    Write(aText); // sure to be safe?
    // using + operator here might give an infinitive loop, see above.
    // not adding ";", because Gecko delivers that as part of |aText| (freaky)
  }
  else
  {
    DoOpenContainer(type);
  }

  return rv;
}


/**
   Similar to SanitizeAttrValue.
 */
nsresult
mozSanitizingHTMLSerializer::SanitizeTextNode(nsString& aText /*inout*/)
{
  aText.Adopt(escape(aText));
  return NS_OK;
}

/**
   Ensures basic sanity of attribute value.
   This function also (tries to :-( ) makes sure, that no
   unwanted / dangerous URLs appear in the document
   (like javascript: and data:).

   Pass the value as |aValue| arg. It will be modified in-place.

   If the value is not allowed at all, we return with NS_ERROR_ILLEGAL_VALUE.
   In that case, do not use the |aValue|, but output nothing.
 */
nsresult
mozSanitizingHTMLSerializer::SanitizeAttrValue(nsHTMLTag aTag,
                                               const nsAString& anAttrName,
                                               nsString& aValue /*inout*/)
{
  /* First, cut the attribute to 1000 chars.
     Attributes with values longer than 1000 chars seem bogus,
     considering that we don't support any JS. The longest attributes
     I can think of are URLs, and URLs with 1000 chars are likely to be
     bogus, too. */
  aValue = Substring(aValue, 0, 1000);
  //aValue.Truncate(1000); //-- this cuts half of the document !!?!!

  aValue.Adopt(escape(aValue));

  /* Check some known bad stuff. Add more!
     I don't care too much, if it happens to trigger in some innocent cases
     (like <img alt="Statistical data: Mortage rates and newspapers">) -
     security first. */
  if (aValue.Find("javascript:") != kNotFound ||
      aValue.Find("data:") != kNotFound ||
      aValue.Find("base64") != kNotFound)
    return NS_ERROR_ILLEGAL_VALUE;

  // Check img src scheme
  if (aTag == eHTMLTag_img && 
      anAttrName.LowerCaseEqualsLiteral("src"))
  {
    nsresult rv;
    nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString scheme;
    rv = ioService->ExtractScheme(NS_LossyConvertUTF16toASCII(aValue), scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!scheme.Equals("cid", nsCaseInsensitiveCStringComparator()))
      return NS_ERROR_ILLEGAL_VALUE;
  }

#ifdef DEBUG_BenB
  printf("attribute value for %s: -%s-\n",
         NS_LossyConvertUTF16toASCII(anAttrName).get(),
         NS_LossyConvertUTF16toASCII(aValue).get());
#endif

  return NS_OK;
}

/**
 */
bool
mozSanitizingHTMLSerializer::IsAllowedTag(nsHTMLTag aTag)
{

  nsPRUint32Key tag_key(aTag);
#ifdef DEBUG_BenB
  printf("IsAllowedTag %d: %s\n",
         aTag,
         mAllowedTags.Exists(&tag_key)?"yes":"no");
#endif
  return mAllowedTags.Exists(&tag_key);
}


/**
 */
bool
mozSanitizingHTMLSerializer::IsAllowedAttribute(nsHTMLTag aTag,
                                             const nsAString& anAttributeName)
{
#ifdef DEBUG_BenB
  printf("IsAllowedAttribute %d, -%s-\n",
         aTag,
         NS_LossyConvertUTF16toASCII(anAttributeName).get());
#endif
  nsresult rv;

  nsPRUint32Key tag_key(aTag);
  nsIProperties* attr_bag = (nsIProperties*)mAllowedTags.Get(&tag_key);
  NS_ENSURE_TRUE(attr_bag, PR_FALSE);

  bool allowed;
  nsAutoString attr(anAttributeName);
  ToLowerCase(attr);
  rv = attr_bag->Has(NS_LossyConvertUTF16toASCII(attr).get(),
                     &allowed);
  if (NS_FAILED(rv))
    return PR_FALSE;

#ifdef DEBUG_BenB
  printf(" Allowed: %s\n", allowed?"yes":"no");
#endif
  return allowed;
}


/**
   aPref is a long string, which holds an exhaustive list of allowed tags
   and attributes. All other tags and attributes will be removed.

   aPref has the format
   "html head body ul ol li a(href,name,title) img(src,alt,title) #text"
   i.e.
   - tags are separated by whitespace
   - the attribute list follows the tag directly in brackets
   - the attributes are separated by commas.

   There is no way to express further restrictions, like "no text inside the
   <head> element". This is so to considerably reduce the complexity of the
   pref and this implementation.

   Update: Akk told me that I might be able to use DTD classes. Later(TM)...
 */
nsresult
mozSanitizingHTMLSerializer::ParsePrefs(const nsAString& aPref)
{
  char* pref = ToNewCString(aPref);
  char* tags_lasts;
  for (char* iTag = PL_strtok_r(pref, " ", &tags_lasts);
       iTag;
       iTag = PL_strtok_r(NULL, " ", &tags_lasts))
  {
    ParseTagPref(nsCAutoString(iTag));
  }
  delete[] pref;

  return NS_OK;
}


/**
   Parses e.g. "a(href,title)" (but not several tags at once).
 */
nsresult
mozSanitizingHTMLSerializer::ParseTagPref(const nsCAutoString& tagpref)
{
  nsIParserService* parserService = nsContentUtils::GetParserService();
  if (!parserService)
    return NS_ERROR_OUT_OF_MEMORY;

  // Parsing tag
  PRInt32 bracket = tagpref.FindChar('(');
  if (bracket == 0)
  {
    printf(" malformed pref: %s\n", tagpref.get());
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  nsAutoString tag;
  CopyUTF8toUTF16(StringHead(tagpref, bracket), tag);

  // Create key
  PRInt32 tag_id = parserService->HTMLStringTagToId(tag);
  if (tag_id == eHTMLTag_userdefined)
  {
    printf(" unknown tag <%s>, won't add.\n",
           NS_ConvertUTF16toUTF8(tag).get());
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }
  nsPRUint32Key tag_key(tag_id);

  if (mAllowedTags.Exists(&tag_key))
  {
    printf(" duplicate tag: %s\n", NS_ConvertUTF16toUTF8(tag).get());
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }
  if (bracket == kNotFound)
    /* There are no attributes in the pref. So, allow none; only the tag
       itself */
  {
    mAllowedTags.Put(&tag_key, 0);
  }
  else
  {
    // Attributes

    // where is the macro for non-fatal errors in opt builds?
    if(tagpref[tagpref.Length() - 1] != ')' ||
       tagpref.Length() < PRUint32(bracket) + 3)
    {
      printf(" malformed pref: %s\n", tagpref.get());
      return NS_ERROR_CANNOT_CONVERT_DATA;
    }
    nsCOMPtr<nsIProperties> attr_bag =
                                 do_CreateInstance(NS_PROPERTIES_CONTRACTID);
    NS_ENSURE_TRUE(attr_bag, NS_ERROR_INVALID_POINTER);
    nsCAutoString attrList;
    attrList.Append(Substring(tagpref,
                              bracket + 1,
                              tagpref.Length() - 2 - bracket));
    char* attrs_lasts;
    for (char* iAttr = PL_strtok_r(attrList.BeginWriting(),
                                   ",", &attrs_lasts);
         iAttr;
         iAttr = PL_strtok_r(NULL, ",", &attrs_lasts))
    {
      attr_bag->Set(iAttr, 0);
    }

    nsIProperties* attr_bag_raw = attr_bag;
    NS_ADDREF(attr_bag_raw);
    mAllowedTags.Put(&tag_key, attr_bag_raw);
  }

  return NS_OK;
}

/*
  might be useful:
  htmlparser/public/nsHTMLTokens.h for tag categories
*/
