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
 * Contributor(s): 
 */

#include "nsXIFConverter.h"
#include <fstream.h>

MOZ_DECL_CTOR_COUNTER(nsXIFConverter);

nsXIFConverter::nsXIFConverter(nsString& aBuffer)
  : mIndent(0),
    mBuffer(aBuffer)
{
  MOZ_COUNT_CTOR(nsXIFConverter);

  char* prolog = "<?xml version=\"1.0\"?>\n";
  char* doctype = "<!DOCTYPE xif>\n";

  mBuffer.AppendWithConversion(prolog);
  mBuffer.AppendWithConversion(doctype);
  
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

  mSelection = nsnull;
}

nsXIFConverter::~nsXIFConverter()
{
  MOZ_COUNT_DTOR(nsXIFConverter);
#ifdef DEBUG_XIF
  WriteDebugFile();
#endif
}


void nsXIFConverter::SetSelection(nsIDOMSelection* aSelection) {
  mSelection = aSelection;
  
  BeginStartTag( NS_ConvertToString("encode") );
  if (mSelection == nsnull)
  {
    AddAttribute( NS_ConvertToString("selection"), NS_ConvertToString("0") );
  }
  else
  {
    AddAttribute( NS_ConvertToString("selection"), NS_ConvertToString("1") );
  }
  FinishStartTag(NS_ConvertToString("encode"),PR_TRUE,PR_TRUE);
}




void nsXIFConverter::BeginStartTag(const nsString& aTag)
{
  // Make all element names lowercase
  nsString tag(aTag);
  tag.ToLowerCase();

  for (PRInt32 i = mIndent; --i >= 0; ) mBuffer.Append(mSpacing);
  mBuffer.Append(mLT);
  mBuffer.Append(tag);
//mIndent++;
}

void nsXIFConverter::BeginStartTag(nsIAtom* aTag)
{
  nsAutoString tag(mNULL);

  if (nsnull != aTag) 
    aTag->ToString(tag);

  BeginStartTag(tag);
}

void nsXIFConverter::AddAttribute(const nsString& aName, const nsString& aValue)
{
  // Make all attribute names lowercase
  nsAutoString name(aName);
  name.ToLowerCase();

  mBuffer.Append(mSpace);
  mBuffer.Append(name);

  mBuffer.Append(mEqual);
  mBuffer.Append(mQuote);    // XML requires quoted attributes
  mBuffer.Append(aValue);
  mBuffer.Append(mQuote);
}

void nsXIFConverter::AddHTMLAttribute(const nsString& aName, const nsString& aValue)
{
  BeginStartTag(mAttr);
  AddAttribute(mName,aName);
  AddAttribute(mValue,aValue);
  FinishStartTag(mAttr,PR_TRUE,PR_TRUE);
}


void nsXIFConverter::AddAttribute(const nsString& aName, nsIAtom* aValue)
{
  nsAutoString value(mNULL);

  if (nsnull != aValue) 
    aValue->ToString(value);

  AddAttribute(aName,value);
}

void nsXIFConverter::AddAttribute(const nsString& aName)
{
  // A convention in XML DTD's is that ATTRIBUTE
  // names are lowercase.
  nsAutoString name(aName);
  name.ToLowerCase();

  mBuffer.Append(mSpace);
  mBuffer.Append(name);
}

void nsXIFConverter::AddAttribute(nsIAtom* aName)
{
  nsAutoString name(mNULL);

  if (nsnull != aName) 
    aName->ToString(name);
  AddAttribute(name);
}


void nsXIFConverter::FinishStartTag(const nsString& aTag, PRBool aIsEmpty, PRBool aAddReturn)
{
  if (aIsEmpty)
  {
    mBuffer.Append(mSlash);  
    mBuffer.Append(mGT);  
//    mIndent--;
  }
  else
    mBuffer.Append(mGT);  
  
  if (aAddReturn == PR_TRUE)
    mBuffer.Append(mLF);
}

void nsXIFConverter::FinishStartTag(nsIAtom* aTag, PRBool aIsEmpty, PRBool aAddReturn)
{
  nsAutoString tag(mNULL);

  if (nsnull != aTag) 
    aTag->ToString(tag);

  FinishStartTag(tag,aIsEmpty,aAddReturn);
}

void nsXIFConverter::AddStartTag(const nsString& aTag, PRBool aAddReturn)
{
  BeginStartTag(aTag);
  FinishStartTag(aTag,PR_FALSE,aAddReturn);
}

void nsXIFConverter::AddStartTag(nsIAtom* aTag, PRBool aAddReturn)
{
  nsAutoString tag(mNULL);

  if (nsnull != aTag) 
    aTag->ToString(tag);
  AddStartTag(tag,aAddReturn);
}


void nsXIFConverter::AddEndTag(const nsString& aTag, PRBool aDoIndent, PRBool aDoReturn)
{
  // A convention in XML DTD's is that ELEMENT
  // names are UPPERCASE.
  nsString tag(aTag);
  tag.ToLowerCase();

//  mIndent--;
  
  if (aDoIndent == PR_TRUE)
    for (PRInt32 i = mIndent; --i >= 0; ) mBuffer.Append(mSpacing);
  mBuffer.Append(mLT);
  mBuffer.Append(mSlash);
  mBuffer.Append(tag);
  mBuffer.Append(mGT);
  if (aDoReturn == PR_TRUE)
    mBuffer.Append(mLF);
}

void nsXIFConverter::AddEndTag(nsIAtom* aTag, PRBool aDoIndent, PRBool aDoReturn)
{
  nsAutoString tag(mNULL);

  if (nsnull != aTag) 
    aTag->ToString(tag);
  AddEndTag(tag,aDoIndent,aDoReturn);
}



PRBool  nsXIFConverter::IsMarkupEntity(const PRUnichar aChar)
{
  PRBool result = PR_FALSE;
  switch (aChar)
  {
    case '<':
    case '>':
    case '&':
      result = PR_TRUE;
    break;
  }
  return result;

}

PRBool  nsXIFConverter::AddMarkupEntity(const PRUnichar aChar)
{
  nsAutoString data;
  PRBool  result = PR_TRUE;

  switch (aChar)
  {
    case '<': data.AssignWithConversion("lt"); break;
    case '>': data.AssignWithConversion("gt"); break;
    case '&': data.AssignWithConversion("amp"); break;
    default:
      result = PR_FALSE;
    break;
  }
  if (result == PR_TRUE)
  {
    BeginStartTag(mEntity);
    AddAttribute(mValue,data);
    FinishStartTag(mEntity,PR_TRUE,PR_FALSE);
  }
  return result;
}


void nsXIFConverter::AddContent(const nsString& aContent)
{
  nsString  tag(mContent);

  AddStartTag(tag,PR_FALSE);

  PRBool  startTagAdded = PR_TRUE;
  PRInt32   length = aContent.Length();
  PRUnichar ch;
  for (PRInt32 i = 0; i < length; i++)
  {
    ch = aContent[i];
    if (IsMarkupEntity(ch))
    {
      if (startTagAdded == PR_TRUE)
      {
        AddEndTag(tag,PR_FALSE);
        startTagAdded = PR_FALSE;
      }
      AddMarkupEntity(ch);
    }
    else
    {
      if (startTagAdded == PR_FALSE)
      {
        AddStartTag(tag,PR_FALSE);
        startTagAdded = PR_TRUE;
      }
      mBuffer.Append(ch);
    }
  }
  AddEndTag(tag,PR_FALSE);
}

//
// AddComment is used to add a XIF comment,
// not something that was a comment in the html;
// that would be a content comment.
//
void nsXIFConverter::AddComment(const nsString& aContent)
{
  mBuffer.Append(mBeginComment);
  mBuffer.Append(aContent);
  mBuffer.Append(mEndComment);
}

//
// AddContentComment is used to add an HTML comment,
// which will be mapped back into the right thing
// in the content sink on the other end when this is parsed.
//
void nsXIFConverter::AddContentComment(const nsString& aContent)
{
  nsString tag(mComment);
  AddStartTag(tag, PR_FALSE);
  AddContent(aContent);
  AddEndTag(tag, PR_FALSE);
}

//
// Use this method to add DOCTYPE, MARKEDSECTION, etc., that are
// not really classified as ** tags **
//
void nsXIFConverter::AddMarkupDeclaration(const nsString& aContent)
{
  nsString tag(mMarkupDeclarationOpen);
  AddStartTag(tag, PR_FALSE);
  AddContent(aContent);
  AddEndTag(tag, PR_FALSE);
}


void nsXIFConverter::BeginContainer(nsIAtom* aTag)
{
  nsString  container(mContainer);

  BeginStartTag(container);
    AddAttribute(mIsa,aTag);
  FinishStartTag(container,PR_FALSE);
}

void nsXIFConverter::EndContainer(nsIAtom* aTag)
{
  nsString  container(mContainer);

  AddEndTag(container,PR_TRUE,PR_FALSE);

  nsAutoString tag(mNULL);
  if (nsnull != aTag) 
    aTag->ToString(tag);
  AddComment(tag);

  mBuffer.Append(mLF);
}



void nsXIFConverter::BeginContainer(const nsString& aTag)
{
  nsString  container(mContainer);

  BeginStartTag(container);
    AddAttribute(mIsa,aTag);
  FinishStartTag(container,PR_FALSE,PR_FALSE);
}

void nsXIFConverter::EndContainer(const nsString& aTag)
{
  nsString  container(mContainer);

  AddEndTag(container,PR_TRUE,PR_FALSE);
  AddComment(aTag);
  mBuffer.Append(mLF);
}


void nsXIFConverter::BeginLeaf(const nsString& aTag)
{
// XXX: Complete hack to prevent the style leaf
// From being created until the style sheet work
// is redone. -- gpk 1/27/99
  if (aTag.EqualsIgnoreCase("STYLE"))
    return;


  BeginStartTag(mLeaf);
    AddAttribute(mIsa,aTag);
  FinishStartTag(mLeaf,PR_FALSE);
}

void nsXIFConverter::EndLeaf(const nsString& aTag)
{
// XXX: Complete hack to prevent the style leaf
// From being created until the style sheet work
// is redone. -- gpk 1/27/99
  if (aTag.EqualsIgnoreCase("STYLE"))
    return;


  AddEndTag(mLeaf,PR_TRUE,PR_FALSE);
  AddComment(aTag);
  mBuffer.Append(mLF);
}


void nsXIFConverter::BeginCSSStyleSheet()
{
  AddStartTag(mSheet);
}


void nsXIFConverter::EndCSSStyleSheet()
{
  AddEndTag(mSheet);
}


void nsXIFConverter::BeginCSSRule()
{
  AddStartTag(mRule);
}

void nsXIFConverter::EndCSSRule()
{
  AddEndTag(mRule);
}


void nsXIFConverter::BeginCSSSelectors()
{
  BeginStartTag(mSelector);
}

void nsXIFConverter::EndCSSSelectors()
{
  FinishStartTag(mSelector,PR_TRUE);
}



void nsXIFConverter::AddCSSSelectors(const nsString& aSelectors)
{
  AddAttribute(NS_ConvertToString("selectors"),aSelectors);
}


void nsXIFConverter::BeginCSSDeclarationList()
{
  AddStartTag(NS_ConvertToString("css_declaration_list"));
}

void nsXIFConverter::EndCSSDeclarationList()
{
  AddEndTag(NS_ConvertToString("css_declaration_list"),PR_TRUE);
}


void nsXIFConverter::BeginCSSDeclaration()
{
  BeginStartTag(NS_ConvertToString("css_declaration"));
}

void nsXIFConverter::AddCSSDeclaration(const nsString& aName, const nsString& aValue)
{
  AddAttribute(NS_ConvertToString("property"),aName);
  AddAttribute(NS_ConvertToString("value"),aValue);
}

void nsXIFConverter::EndCSSDeclaration()
{
  FinishStartTag(NS_ConvertToString("css_declaration"),PR_TRUE);
}

#ifdef DEBUG_XIF
//
// Leave a temp file for debugging purposes
//
void nsXIFConverter::WriteDebugFile() {

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
  char* s = mBuffer.ToNewUTF8String();
  if (s) {
    out << s;
    Recycle(s);
  }
  out.close();
}
#endif /* DEBUG */
