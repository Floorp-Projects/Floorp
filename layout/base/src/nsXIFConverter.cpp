/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#include "nsCOMPtr.h"
#include "nsXIFConverter.h"
#include <fstream.h>
#include "nsIDOMSelection.h"
#include "nsReadableUtils.h"

MOZ_DECL_CTOR_COUNTER(nsXIFConverter);

NS_IMPL_ISUPPORTS(nsXIFConverter, NS_GET_IID(nsIXIFConverter))


nsresult
NS_NewXIFConverter(nsIXIFConverter **aResult)
{
  *aResult = (nsIXIFConverter *)new nsXIFConverter;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsXIFConverter::nsXIFConverter()
  : mIndent(0)
{
  MOZ_COUNT_CTOR(nsXIFConverter);
  NS_INIT_REFCNT();
  
  mInScript = PR_FALSE;
}

nsXIFConverter::~nsXIFConverter()
{
  MOZ_COUNT_DTOR(nsXIFConverter);
#ifdef DEBUG_XIF
  WriteDebugFile();
#endif
}


NS_IMETHODIMP
nsXIFConverter::Init(nsAWritableString& aBuffer)
{
  // XXX This is wrong. It violates XPCOM string ownership rules.
  // We're only getting away with this because instances of this
  // class are restricted to single function scope.
  mBuffer = &aBuffer;
  NS_ENSURE_ARG_POINTER(mBuffer);

  mBuffer->Append(NS_LITERAL_STRING("<?xml version=\"1.0\"?>\n"));
  mBuffer->Append(NS_LITERAL_STRING("<!DOCTYPE xif>\n"));
  
  mAttr.AssignWithConversion("attr");
  mName.AssignWithConversion("name");
  mValue.AssignWithConversion("value");
  
  mContent.AssignWithConversion("content");
  mComment.AssignWithConversion("comment");
  mContainer.AssignWithConversion("container");
  mLeaf.AssignWithConversion("leaf");
  mIsa.AssignWithConversion("isa");
  mEntity.AssignWithConversion("entity");

  mSelector.AssignWithConversion("css_selector");
  mRule.AssignWithConversion("css_rule");
  mSheet.AssignWithConversion("css_stylesheet");

  mNULL.AssignWithConversion("?NULL");
  mSpacing.AssignWithConversion("  ");
  mSpace.AssignWithConversion((char)' ');
  mLT.AssignWithConversion((char)'<');
  mGT.AssignWithConversion((char)'>');
  mLF.AssignWithConversion((char)'\n');
  mSlash.AssignWithConversion((char)'/');
  mBeginComment.AssignWithConversion("<!--");
  mEndComment.AssignWithConversion("-->");
  mQuote.AssignWithConversion((char)'\"');
  mEqual.AssignWithConversion((char)'=');
  mMarkupDeclarationOpen.AssignWithConversion("markup_declaration");
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::SetSelection(nsIDOMSelection* aSelection) 
{
  NS_ENSURE_ARG_POINTER(aSelection);
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  mSelectionWeak = getter_AddRefs( NS_GetWeakReference(aSelection) );
  
  BeginStartTag( NS_ConvertToString("encode") );
  if (!mSelectionWeak)
  {
    AddAttribute( NS_ConvertToString("selection"), NS_ConvertToString("0") );
  }
  else
  {
    AddAttribute( NS_ConvertToString("selection"), NS_ConvertToString("1") );
  }
  FinishStartTag(NS_ConvertToString("encode"),PR_TRUE,PR_TRUE);
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::GetSelection(nsIDOMSelection** aSelection)
{ 
  NS_ENSURE_ARG_POINTER(aSelection);
  nsCOMPtr<nsIDOMSelection> sel =  do_QueryReferent(mSelectionWeak);
  NS_IF_ADDREF(*aSelection = sel);
  return *aSelection?NS_OK :NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsXIFConverter::BeginStartTag(const nsAReadableString& aTag)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  // Make all element names lowercase
  nsAutoString tag(aTag);
  tag.ToLowerCase();

  for (PRInt32 i = mIndent; --i >= 0; ) 
    mBuffer->Append(mSpacing);
  mBuffer->Append(mLT);
  mBuffer->Append(tag);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::BeginStartTag(nsIAtom* aTag)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString tag; tag.Assign(mNULL);

  if (nsnull != aTag) 
    aTag->ToString(tag);

  BeginStartTag(tag);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::AddAttribute(const nsAReadableString& aName, const nsAReadableString& aValue)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  // Make all attribute names lowercase
  nsAutoString name(aName);
  name.ToLowerCase();

  mBuffer->Append(mSpace);
  mBuffer->Append(name);

  mBuffer->Append(mEqual);
  mBuffer->Append(mQuote);    // XML requires quoted attributes
  mBuffer->Append(aValue);
  mBuffer->Append(mQuote);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::AddHTMLAttribute(const nsAReadableString& aName, const nsAReadableString& aValue)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  BeginStartTag(mAttr);
  AddAttribute(mName,aName);
  AddAttribute(mValue,aValue);
  FinishStartTag(mAttr,PR_TRUE,PR_TRUE);
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::AddAttribute(const nsAReadableString& aName, nsIAtom* aValue)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString value(mNULL);

  if (nsnull != aValue) 
    aValue->ToString(value);

  AddAttribute(aName,value);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::AddAttribute(const nsAReadableString& aName)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  // A convention in XML DTD's is that ATTRIBUTE
  // names are lowercase.
  nsAutoString name(aName);
  name.ToLowerCase();

  mBuffer->Append(mSpace);
  mBuffer->Append(name);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::AddAttribute(nsIAtom* aName)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString name(mNULL);

  if (nsnull != aName) 
    aName->ToString(name);
  AddAttribute(name);
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::FinishStartTag(const nsAReadableString& aTag, PRBool aIsEmpty, PRBool aAddReturn)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  if (aIsEmpty)
  {
    mBuffer->Append(mSlash);  
    mBuffer->Append(mGT);  
//    mIndent--;
  }
  else
    mBuffer->Append(mGT);  
  
  if (aAddReturn == PR_TRUE)
    mBuffer->Append(mLF);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::FinishStartTag(nsIAtom* aTag, PRBool aIsEmpty, PRBool aAddReturn)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString tag(mNULL);

  if (nsnull != aTag) 
    aTag->ToString(tag);

  FinishStartTag(tag,aIsEmpty,aAddReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::AddStartTag(const nsAReadableString& aTag, PRBool aAddReturn)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  BeginStartTag(aTag);
  FinishStartTag(aTag,PR_FALSE,aAddReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::AddStartTag(nsIAtom* aTag, PRBool aAddReturn)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString tag(mNULL);

  if (nsnull != aTag) 
    aTag->ToString(tag);
  AddStartTag(tag,aAddReturn);
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::AddEndTag(const nsAReadableString& aTag, PRBool aDoIndent, PRBool aDoReturn)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  // A convention in XML DTD's is that ELEMENT
  // names are UPPERCASE.
  nsAutoString tag(aTag);
  tag.ToLowerCase();

//  mIndent--;
  
  if (aDoIndent == PR_TRUE)
    for (PRInt32 i = mIndent; --i >= 0; ) mBuffer->Append(mSpacing);
  mBuffer->Append(mLT);
  mBuffer->Append(mSlash);
  mBuffer->Append(tag);
  mBuffer->Append(mGT);
  if (aDoReturn == PR_TRUE)
    mBuffer->Append(mLF);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::AddEndTag(nsIAtom* aTag, PRBool aDoIndent, PRBool aDoReturn)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString tag(mNULL);

  if (nsnull != aTag) 
    aTag->ToString(tag);
  AddEndTag(tag,aDoIndent,aDoReturn);
  return NS_OK;
}



NS_IMETHODIMP
nsXIFConverter::IsMarkupEntity(const PRUnichar aChar, PRBool *aReturn)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aReturn);
  // If we're in a script, pass entities straight through without conversion:
  if (mInScript)
    return PR_FALSE;

  *aReturn = PR_FALSE;
  switch (aChar)
  {
    case '<':
    case '>':
    case '&':
      *aReturn = PR_TRUE;
    break;
  }
  return NS_OK;

}

NS_IMETHODIMP
nsXIFConverter::AddMarkupEntity(const PRUnichar aChar, PRBool *aReturn)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aReturn);
  nsAutoString data;

  switch (aChar)
  {
    case '<': data.AssignWithConversion("lt"); break;
    case '>': data.AssignWithConversion("gt"); break;
    case '&': data.AssignWithConversion("amp"); break;
    default:
      *aReturn = PR_FALSE;
    break;
  }
  if (*aReturn == PR_TRUE)
  {
    BeginStartTag(mEntity);
    AddAttribute(mValue,data);
    FinishStartTag(mEntity,PR_TRUE,PR_FALSE);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::AddContent(const nsAReadableString& aContent)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString  tag(mContent);

  AddStartTag(tag,PR_FALSE);

  PRBool  startTagAdded = PR_TRUE;
  PRInt32   length = aContent.Length();
  PRUnichar ch;
  PRBool boolVal(PR_FALSE);
  for (PRInt32 i = 0; i < length; i++)
  {
    ch = aContent[i];
    IsMarkupEntity(ch, &boolVal);
    if (boolVal)
    {
      if (startTagAdded == PR_TRUE)
      {
        AddEndTag(tag,PR_FALSE);
        startTagAdded = PR_FALSE;
      }
      AddMarkupEntity(ch, &boolVal);
    }
    else
    {
      if (startTagAdded == PR_FALSE)
      {
        AddStartTag(tag,PR_FALSE);
        startTagAdded = PR_TRUE;
      }
      mBuffer->Append(ch);
    }
  }
  AddEndTag(tag,PR_FALSE);
  return NS_OK;
}

//
// AddComment is used to add a XIF comment,
// not something that was a comment in the html;
// that would be a content comment.
//
NS_IMETHODIMP
nsXIFConverter::AddComment(const nsAReadableString& aContent)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  mBuffer->Append(mBeginComment);
  mBuffer->Append(aContent);
  mBuffer->Append(mEndComment);
  return NS_OK;
}

//
// AddContentComment is used to add an HTML comment,
// which will be mapped back into the right thing
// in the content sink on the other end when this is parsed.
//
NS_IMETHODIMP
nsXIFConverter::AddContentComment(const nsAReadableString& aContent)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString tag(mComment);
  AddStartTag(tag, PR_FALSE);
  AddContent(aContent);
  AddEndTag(tag, PR_FALSE);
  return NS_OK;
}

//
// Use this method to add DOCTYPE, MARKEDSECTION, etc., that are
// not really classified as ** tags **
//
NS_IMETHODIMP
nsXIFConverter::AddMarkupDeclaration(const nsAReadableString& aContent)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString tag(mMarkupDeclarationOpen);
  AddStartTag(tag, PR_FALSE);
  AddContent(aContent);
  AddEndTag(tag, PR_FALSE);
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::BeginContainer(nsIAtom* aTag)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString container(mContainer);

  BeginStartTag(container);
    AddAttribute(mIsa,aTag);
  FinishStartTag(container,PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::EndContainer(nsIAtom* aTag)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString  container(mContainer);

  AddEndTag(container,PR_TRUE,PR_FALSE);

  nsAutoString tag(mNULL);
  if (nsnull != aTag) 
    aTag->ToString(tag);
  AddComment(tag);

  mBuffer->Append(mLF);
  return NS_OK;
}



NS_IMETHODIMP
nsXIFConverter::BeginContainer(const nsAReadableString& aTag)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString  container(mContainer);

  BeginStartTag(container);
    AddAttribute(mIsa,aTag);
  FinishStartTag(container,PR_FALSE,PR_FALSE);

  // Remember if we're inside a script tag:
  if (aTag.Equals(NS_LITERAL_STRING("script")) || 
      aTag.Equals(NS_LITERAL_STRING("style")))
    mInScript = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::EndContainer(const nsAReadableString& aTag)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  nsAutoString  container(mContainer);

  AddEndTag(container,PR_TRUE,PR_FALSE);
  AddComment(aTag);
  mBuffer->Append(mLF);

  // Remember if we're exiting a script tag:
  if (aTag.Equals(NS_LITERAL_STRING("script")) || 
      aTag.Equals(NS_LITERAL_STRING("style")))
    mInScript = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::BeginLeaf(const nsAReadableString& aTag)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
// XXX: Complete hack to prevent the style leaf
// From being created until the style sheet work
// is redone. -- gpk 1/27/99
  if (nsCRT::strcasecmp(nsPromiseFlatString(aTag), NS_LITERAL_STRING("STYLE")) == 0)
    return NS_OK;


  BeginStartTag(mLeaf);
    AddAttribute(mIsa,aTag);
  FinishStartTag(mLeaf,PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::EndLeaf(const nsAReadableString& aTag)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
// XXX: Complete hack to prevent the style leaf
// From being created until the style sheet work
// is redone. -- gpk 1/27/99
  if (nsCRT::strcasecmp(nsPromiseFlatString(aTag), NS_LITERAL_STRING("STYLE")) == 0)
    return NS_OK;


  AddEndTag(mLeaf,PR_TRUE,PR_FALSE);
  AddComment(aTag);
  mBuffer->Append(mLF);
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::BeginCSSStyleSheet()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  AddStartTag(mSheet);
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::EndCSSStyleSheet()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  AddEndTag(mSheet);
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::BeginCSSRule()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  AddStartTag(mRule);
  return NS_OK;
}

NS_IMETHODIMP
nsXIFConverter::EndCSSRule()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  AddEndTag(mRule);
  return NS_OK;
}


NS_IMETHODIMP
nsXIFConverter::BeginCSSSelectors()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  BeginStartTag(mSelector);
  return NS_OK;
}

NS_IMETHODIMP nsXIFConverter::EndCSSSelectors()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  FinishStartTag(mSelector,PR_TRUE);
  return NS_OK;
}



NS_IMETHODIMP nsXIFConverter::AddCSSSelectors(const nsAReadableString& aSelectors)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  AddAttribute(NS_ConvertToString("selectors"),aSelectors);
  return NS_OK;
}


NS_IMETHODIMP nsXIFConverter::BeginCSSDeclarationList()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  AddStartTag(NS_ConvertToString("css_declaration_list"));
  return NS_OK;
}

NS_IMETHODIMP nsXIFConverter::EndCSSDeclarationList()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  AddEndTag(NS_ConvertToString("css_declaration_list"),PR_TRUE);
  return NS_OK;
}


NS_IMETHODIMP nsXIFConverter::BeginCSSDeclaration()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  BeginStartTag(NS_ConvertToString("css_declaration"));
  return NS_OK;
}

NS_IMETHODIMP nsXIFConverter::AddCSSDeclaration(const nsAReadableString& aName, const nsAReadableString& aValue)
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  AddAttribute(NS_ConvertToString("property"),aName);
  AddAttribute(NS_ConvertToString("value"),aValue);
  return NS_OK;
}

NS_IMETHODIMP nsXIFConverter::EndCSSDeclaration()
{
  NS_ENSURE_TRUE(mBuffer,NS_ERROR_NOT_INITIALIZED);
  FinishStartTag(NS_ConvertToString("css_declaration"),PR_TRUE);
  return NS_OK;
}

#ifdef DEBUG_XIF
//
// Leave a temp file for debugging purposes
//
NS_IMETHODIMP nsXIFConverter::WriteDebugFile() {

#if defined(WIN32)
  const char* filename="c:\\temp\\xif.xif";

#elif defined(XP_UNIX) || defined(XP_BEOS)
  const char* filename="/tmp/xif.xif";

#elif defined(XP_MAC)
  // XXX Someone please write some Mac debugging code here!
  const char* filename="xif.xif";
#else
  const char* filename="xif.xif";

#endif

  ofstream out(filename);
  char* s = ToNewUTF8String(mBuffer);
  if (s) {
    out << s;
    Recycle(s);
  }
  out.close();
  return NS_OK;
}
#else
NS_IMETHODIMP nsXIFConverter::WriteDebugFile()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#endif /* DEBUG */
