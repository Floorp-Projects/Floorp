/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsXIFConverter.h"
#include <fstream.h>


nsXIFConverter::nsXIFConverter(nsString& aBuffer) :
  mBuffer(aBuffer), 
  mIndent(0)
{
  char* prolog = "<?xml version=\"1.0\"?>\n";
  char* doctype = "<!DOCTYPE xif>\n";

  mBuffer.Append(prolog);
  mBuffer.Append(doctype);
  
  mAttr = "attr";
  mName = "name";
  mValue = "value";
  
  mContent = "content";
  mComment = "comment";
  mContainer = "container";
  mLeaf = "leaf";
  mIsa = "isa";
  mEntity = "entity";

  mSelector = "css_selector";
  mRule = "css_rule";
  mSheet = "css_stylesheet";

  mNULL = "?NULL";
  mSpacing = "  ";
  mSpace = (char)' ';
  mLT = (char)'<';
  mGT = (char)'>';
  mLF = (char)'\n';
  mSlash = (char)'/';
  mBeginComment = "<!--";
  mEndComment = "-->";
  mQuote =(char)'\"';
  mEqual =(char)'=';

  mSelection = nsnull;
}

nsXIFConverter::~nsXIFConverter()
{
}


void nsXIFConverter::SetSelection(nsIDOMSelection* aSelection) {
  mSelection = aSelection;
  
  BeginStartTag("encode");
  if (mSelection == nsnull)
  {
    AddAttribute("selection","0");
  }
  else
  {
    AddAttribute("selection","1");
  }
  FinishStartTag("encode",PR_TRUE,PR_TRUE);
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
    case '<': data = "lt"; break;
    case '>': data = "gt"; break;
    case '&': data = "amp"; break;
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

void nsXIFConverter::AddComment(const nsString& aContent)
{

  mBuffer.Append(mBeginComment);
    mBuffer.Append(aContent);
  mBuffer.Append(mEndComment);
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
  FinishStartTag(container,PR_FALSE);
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
  AddAttribute(nsString("selectors"),aSelectors);
}


void nsXIFConverter::BeginCSSDeclarationList()
{
  AddStartTag(nsString("css_declaration_list"));
}

void nsXIFConverter::EndCSSDeclarationList()
{
  AddEndTag(nsString("css_declaration_list"),PR_TRUE);
}


void nsXIFConverter::BeginCSSDeclaration()
{
  BeginStartTag(nsString("css_declaration"));
}

void nsXIFConverter::AddCSSDeclaration(const nsString& aName, const nsString& aValue)
{
  AddAttribute(nsString("property"),aName);
  AddAttribute(nsString("value"),aValue);
}

void nsXIFConverter::EndCSSDeclaration()
{
  FinishStartTag(nsString("css_declaration"),PR_TRUE);
}

void nsXIFConverter::Write()
{


#if defined(WIN32)
  const char* filename="c:\\temp\\xif.html";
  ofstream out(filename);

  char* s = mBuffer.ToNewCString();
  out << s;
  out.close();
  delete[] s;
#endif
}
