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

// Removes potentially insecure or offending HTML

/* I used nsPlaintextSerializer as base for this class. I don't understand
   all of the functions in the beginning. Possible that I fail to do
   something or do something useless.
   I am not proud about the implementation here at all.
   Feel free to fix it :-).
*/

#include "mozSanitizingSerializer.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "plstr.h"
#include "nsIProperties.h"
#include "nsUnicharUtils.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsEscape.h"

//#define DEBUG_BenB

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
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(it);
  *aSerializer = it;
  return NS_OK;
}

mozSanitizingHTMLSerializer::mozSanitizingHTMLSerializer()
  : mAllowedTags(30) // Just some initial buffer size
{
  mOutputString = nsnull;
}

mozSanitizingHTMLSerializer::~mozSanitizingHTMLSerializer()
{
#ifdef DEBUG_BenB
  printf("Output:\n%s\n", NS_LossyConvertUCS2toASCII(*mOutputString).get());
#endif
  mAllowedTags.Enumerate(ReleaseProperties);
}

//<copy from="xpcom/ds/nsProperties.cpp">
PRBool PR_CALLBACK 
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
                                  const char* aCharSet, PRBool aIsCopying)
{
  NS_ENSURE_TRUE(nsContentUtils::GetParserServiceWeakRef(),
                 NS_ERROR_UNEXPECTED);

  return NS_OK;
}

NS_IMETHODIMP
mozSanitizingHTMLSerializer::Initialize(nsAString* aOutString,
                                        PRUint32 aFlags,
                                        const nsAString& allowedTags)
{
  nsresult rv = Init(aFlags, 0, nsnull, PR_FALSE);
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
  printf("Flush: -%s-", NS_LossyConvertUCS2toASCII(aStr).get());
#endif
  Write(aStr);
  return NS_OK;
}

NS_IMETHODIMP
mozSanitizingHTMLSerializer::AppendDocumentStart(nsIDOMDocument *aDocument,
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
mozSanitizingHTMLSerializer::IsEnabled(PRInt32 aTag, PRBool* aReturn)
{
  *aReturn = PR_FALSE;
  return NS_OK;
}


/**
 * Returns true, if the id represents a container
 */
PRBool
mozSanitizingHTMLSerializer::IsContainer(PRInt32 aId)
{
  PRBool isContainer = PR_FALSE;

  nsIParserService* parserService = nsContentUtils::GetParserServiceWeakRef();
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
  if (!aContent->IsContentOfType(nsIContent::eHTML)) {
    return eHTMLTag_unknown;
  }

  nsIParserService* parserService = nsContentUtils::GetParserServiceWeakRef();

  PRInt32 id;
  nsresult rv = parserService->HTMLAtomTagToId(aContent->Tag(), &id);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Can't map HTML tag to id!");

  return id;
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::AppendText(nsIDOMText* aText, 
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
mozSanitizingHTMLSerializer::AppendElementStart(nsIDOMElement *aElement,
                                                PRBool aHasChildren,
                                                nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mContent = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(mContent, NS_ERROR_FAILURE);

  mOutputString = &aStr;

  PRInt32 id = GetIdForContent(mContent);

  PRBool isContainer = IsContainer(id);

  nsresult rv;
  if (isContainer) {
    rv = DoOpenContainer(id);
  }
  else {
    rv = DoAddLeaf(id, EmptyString());
  }

  mContent = 0;
  mOutputString = nsnull;

  return rv;
} 
 
NS_IMETHODIMP 
mozSanitizingHTMLSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                              nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mContent = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(mContent, NS_ERROR_FAILURE);

  mOutputString = &aStr;

  nsresult rv = NS_OK;
  PRInt32 id = GetIdForContent(mContent);

  PRBool isContainer = IsContainer(id);

  if (isContainer) {
    rv = DoCloseContainer(id);
  }

  mContent = 0;
  mOutputString = nsnull;

  return rv;
}

NS_IMETHODIMP
mozSanitizingHTMLSerializer::OpenContainer(const nsIParserNode& aNode)
{
  PRInt32 type = aNode.GetNodeType();

  mParserNode = NS_CONST_CAST(nsIParserNode *, &aNode);
  return DoOpenContainer(type);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::CloseContainer(const nsHTMLTag aTag)
{
  // XXX Why do we need this?
  // mParserNode = NS_CONST_CAST(nsIParserNode*, &aNode);
  return DoCloseContainer(aTag);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::AddHeadContent(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  if (eHTMLTag_whitespace == type || 
      eHTMLTag_newline == type    ||
      eHTMLTag_text == type       ||
      eHTMLTag_entity == type) {
    rv = AddLeaf(aNode);
  }
  else {
    rv = OpenContainer(aNode);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = CloseContainer(type);
  }
  return rv;
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::AddLeaf(const nsIParserNode& aNode)
{
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  const nsAString& text = aNode.GetText();

  mParserNode = NS_CONST_CAST(nsIParserNode*, &aNode);
  return DoAddLeaf(type, text);
}

NS_IMETHODIMP
mozSanitizingHTMLSerializer::OpenHTML(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::CloseHTML()
{
  return CloseContainer(eHTMLTag_html);
}

NS_IMETHODIMP
mozSanitizingHTMLSerializer::SetTitle(const nsString& aValue)
{
  if (IsAllowedTag(eHTMLTag_title))
  {
    // See bug 195020 for a good reason to output the tags.
    // It will make sure we have a closing tag, and a
    // missing </title> tag won't result in everything
    // being eaten up as the title.
    Write(NS_LITERAL_STRING("<title>"));
    Write(nsAdoptingString(escape(aValue)));
    Write(NS_LITERAL_STRING("</title>"));
  }
  return NS_OK;
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
  Write(NS_LITERAL_STRING("\n<meta http-equiv=\"Context-Type\" content=\"text/html; charset=")
        /* Danger: breaking the line within the string literal, like
           "foo"\n"bar", breaks win32! */
        + nsAdoptingString(escape(NS_ConvertASCIItoUCS2(aCharset)))
        + NS_LITERAL_STRING("\">\n"));
  return NS_OK;
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::OpenHead(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::CloseHead()
{
  return CloseContainer(eHTMLTag_head);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::OpenBody(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::CloseBody()
{
  return CloseContainer(eHTMLTag_body);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::OpenForm(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::CloseForm()
{
  return CloseContainer(eHTMLTag_form);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::OpenMap(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::CloseMap()
{
  return CloseContainer(eHTMLTag_map);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::OpenFrameset(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
mozSanitizingHTMLSerializer::CloseFrameset()
{
  return CloseContainer(eHTMLTag_frameset);
}


// Here comes the actual code...

nsresult
mozSanitizingHTMLSerializer::DoOpenContainer(PRInt32 aTag)
{
  eHTMLTags type = (eHTMLTags)aTag;

  if (IsAllowedTag(type))
  {
    nsIParserService* parserService =
      nsContentUtils::GetParserServiceWeakRef();
    if (!parserService)
      return NS_ERROR_OUT_OF_MEMORY;
    const PRUnichar* tag_name;
    parserService->HTMLIdToStringTag(aTag, &tag_name);
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
  else
    Write(NS_LITERAL_STRING(" "));

  return NS_OK;

}

nsresult
mozSanitizingHTMLSerializer::DoCloseContainer(PRInt32 aTag)
{
  eHTMLTags type = (eHTMLTags)aTag;

  if (IsAllowedTag(type)) {
    nsIParserService* parserService =
      nsContentUtils::GetParserServiceWeakRef();
    if (!parserService)
      return NS_ERROR_OUT_OF_MEMORY;
    const PRUnichar* tag_name;
    parserService->HTMLIdToStringTag(aTag, &tag_name);
    NS_ENSURE_TRUE(tag_name, NS_ERROR_INVALID_POINTER);

    Write(NS_LITERAL_STRING("</") + nsDependentString(tag_name)
          + NS_LITERAL_STRING(">"));
  }
  else
    Write(NS_LITERAL_STRING(" "));

  return NS_OK;
}

nsresult
mozSanitizingHTMLSerializer::DoAddLeaf(PRInt32 aTag,
                                       const nsAString& aText)
{
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
    rv = ioService->ExtractScheme(NS_LossyConvertUCS2toASCII(aValue), scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!scheme.Equals("cid", nsCaseInsensitiveCStringComparator()))
      return NS_ERROR_ILLEGAL_VALUE;
  }

#ifdef DEBUG_BenB
  printf("attribute value for %s: -%s-\n",
         NS_LossyConvertUCS2toASCII(anAttrName).get(),
         NS_LossyConvertUCS2toASCII(aValue).get());
#endif

  return NS_OK;
}

/**
 */
PRBool
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
PRBool
mozSanitizingHTMLSerializer::IsAllowedAttribute(nsHTMLTag aTag,
                                             const nsAString& anAttributeName)
{
#ifdef DEBUG_BenB
  printf("IsAllowedAttribute %d, -%s-\n",
         aTag,
         NS_LossyConvertUCS2toASCII(anAttributeName).get());
#endif
  nsresult rv;

  nsPRUint32Key tag_key(aTag);
  nsIProperties* attr_bag = (nsIProperties*)mAllowedTags.Get(&tag_key);
  NS_ENSURE_TRUE(attr_bag, PR_FALSE);

  PRBool allowed;
  nsAutoString attr(anAttributeName);
  ToLowerCase(attr);
  rv = attr_bag->Has(NS_LossyConvertUCS2toASCII(attr).get(),
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
  nsIParserService* parserService = nsContentUtils::GetParserServiceWeakRef();
  if (!parserService)
    return NS_ERROR_OUT_OF_MEMORY;

  // Parsing tag
  PRInt32 bracket = tagpref.Find("(");
  nsCAutoString tag = tagpref;
  if (bracket != kNotFound)
    tag.Truncate(bracket);
  if (tag.Equals(""))
  {
    printf(" malformed pref: %s\n", tagpref.get());
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  // Create key
  NS_ConvertASCIItoUCS2 tag_widestr(tag);
  PRInt32 tag_id;
  parserService->HTMLStringTagToId(tag_widestr, &tag_id);
  if (tag_id == eHTMLTag_userdefined ||
      tag_id == eHTMLTag_unknown)
  {
    printf(" unknown tag <%s>, won't add.\n", tag.get());
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }
  nsPRUint32Key tag_key(tag_id);

  if (mAllowedTags.Exists(&tag_key))
  {
    printf(" duplicate tag: %s\n", tag.get());
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
