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
#include <stdio.h>
#include "nscore.h"
#include "nsIAtom.h"
#include "nsCRT.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsITextContent.h"
#include "nsIUnicharInputStream.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsISupportsArray.h"
#include "nsDocument.h"
#include "nsIURL.h"
#include "nsIWebWidget.h"

void testAttributes(nsIHTMLContent* content) {
  nsIAtom* sBORDER = NS_NewAtom("BORDER");
  nsIAtom* sWIDTH = NS_NewAtom("WIDTH");
  nsIAtom* sHEIGHT = NS_NewAtom("HEIGHT");
  nsIAtom* sSRC = NS_NewAtom("SRC");
  nsIAtom* sBAD = NS_NewAtom("BADATTRIBUTE");
  nsString sempty("");
  nsString sfoo_gif("foo.gif");

  content->SetAttribute(sBORDER);
  content->SetAttribute(sWIDTH, nsHTMLValue(5, eHTMLUnit_Pixel));
  content->SetAttribute(sHEIGHT, sempty);
  content->SetAttribute(sSRC, sfoo_gif);

  nsHTMLValue ret;
  nsContentAttr rv;
  rv = content->GetAttribute(sBORDER, ret);
  if ((rv != eContentAttr_NoValue) || (ret.GetUnit() != eHTMLUnit_Null)) {
    printf("test 0 failed\n");
  }

  rv = content->GetAttribute(sWIDTH, ret);
  if ((rv != eContentAttr_HasValue) || (! (ret == nsHTMLValue(5, eHTMLUnit_Pixel)))) {
    printf("test 1 failed\n");
  }

  rv = content->GetAttribute(sBAD, ret);
  if (rv != eContentAttr_NotThere) {
    printf("test 2 failed\n");
  }

  // Same, except different case strings

  nsString sborder("border");
  nsString strRet;
  rv = ((nsIContent*)content)->GetAttribute(sborder, strRet);
  if (rv != eContentAttr_NoValue) {
    printf("test 3 (case comparison) failed\n");
  }

  content->UnsetAttribute(sWIDTH);

  nsISupportsArray* allNames;
  NS_NewISupportsArray(&allNames);

  content->GetAllAttributeNames(allNames);
  if (allNames->Count() != 3) {
    printf("test 5 (unset attriubte) failed\n");
  }

  PRBool borderFound = PR_FALSE,heightFound = PR_FALSE,srcFound = PR_FALSE;
  for (int n = 0; n < 3; n++) {
    const nsIAtom* ident = (const nsIAtom*)allNames->ElementAt(n);
    if (sBORDER == ident) {
      borderFound = PR_TRUE;
    }
    if (sHEIGHT == ident) {
      heightFound = PR_TRUE;
    }
    if (sSRC == ident) {
      srcFound = PR_TRUE;
    }
  }
  if (!(borderFound && heightFound && srcFound)) {
    printf("test 6 failed\n");
  }

  NS_RELEASE(allNames);

  NS_RELEASE(sBORDER);
  NS_RELEASE(sWIDTH);
  NS_RELEASE(sHEIGHT);
  NS_RELEASE(sSRC);
}

void testStrings(nsIDocument* aDoc) {
  printf("begin string tests\n");

  PRBool val;
  // regular Equals
  val = (nsString("mrString")).Equals(nsString("mrString"));
  if (PR_TRUE != val) {
    printf("test 0 failed\n");
  }
  val = (nsString("mrString")).Equals(nsString("MRString"));
  if (PR_FALSE != val) {
    printf("test 1 failed\n");
  }
  val = (nsString("mrString")).Equals(nsString("mrStri"));
  if (PR_FALSE != val) {
    printf("test 2 failed\n");
  }
  val = (nsString("mrStri")).Equals(nsString("mrString"));
  if (PR_FALSE != val) {
    printf("test 3 failed\n");
  }
  // EqualsIgnoreCase
  val = (nsString("mrString")).EqualsIgnoreCase("mrString");
  if (PR_TRUE != val) {
    printf("test 4 failed\n");
  }
  val = (nsString("mrString")).EqualsIgnoreCase("mrStrinG");
  if (PR_TRUE != val) {
    printf("test 5 failed\n");
  }
  val = (nsString("mrString")).EqualsIgnoreCase("mrStri");
  if (PR_FALSE != val) {
    printf("test 6 failed\n");
  }
  val = (nsString("mrStri")).EqualsIgnoreCase("mrString");
  if (PR_FALSE != val) {
    printf("test 7 failed\n");
  }
  // String vs. Ident
  val = (nsString("mrString")).EqualsIgnoreCase(NS_NewAtom("mrString"));
  if (PR_TRUE != val) {
    printf("test 8 failed\n");
  }
  val = (nsString("mrString")).EqualsIgnoreCase(NS_NewAtom("MRStrINg"));
  if (PR_TRUE != val) {
    printf("test 9 failed\n");
  }
  val = (nsString("mrString")).EqualsIgnoreCase(NS_NewAtom("mrStri"));
  if (PR_FALSE != val) {
    printf("test 10 failed\n");
  }
  val = (nsString("mrStri")).EqualsIgnoreCase(NS_NewAtom("mrString"));
  if (PR_FALSE != val) {
    printf("test 11 failed\n");
  }

  printf("string tests complete\n");
}

class MyDocument : public nsDocument {
public:
  MyDocument();
  NS_IMETHOD StartDocumentLoad(nsIURL *aUrl, 
                               nsIWebWidget* aWebWidget,
                               nsIStreamListener **aDocListener)
  {
    return NS_OK;
  }

protected:
  virtual ~MyDocument();
};

MyDocument::MyDocument()
{
}

MyDocument::~MyDocument()
{
}

int main(int argc, char** argv)
{
  // Create Byte2Unicode converter
  nsresult rv;
  nsIB2UConverter *converter;
  rv = NS_NewB2UConverter(&converter,NULL);
  if (NS_OK != rv) {
    printf("Could not create converter.\n");
    return -1;
  }

  // Create a unicode string
  static const char* srcStr = "This is some meaningless text about nothing at all";
  PRInt32 rv2;
  PRInt32 origSrcLen = nsCRT::strlen((char *)srcStr);
  const int BUFFER_LENGTH = 100;
  PRUnichar destStr[BUFFER_LENGTH];
  PRInt32 srcLen = origSrcLen;
  PRInt32 destLen = BUFFER_LENGTH;
  rv2 = converter->Convert(destStr,0,destLen,srcStr,0,srcLen);
  if ((NS_OK != rv2) || (srcLen < origSrcLen)) {
    printf("Failed to convert all characters to unicode.\n");
    return -1;
  }

  delete converter;

  // Create test document.
  MyDocument *myDoc = new MyDocument();

  testStrings(myDoc);

  // Create a new text content object.
  nsIHTMLContent *text;
  rv = NS_NewHTMLText(&text,destStr,destLen);
  if (NS_OK != rv) {
    printf("Could not create text content.\n");
    return -1;
  }
  NS_ASSERTION(!text->CanContainChildren(),"");
  text->SetDocument(myDoc);

#if 0
  // Query ITextContent interface
  static NS_DEFINE_IID(kITextContentIID, NS_ITEXTCONTENT_IID);
  nsITextContent* textContent;
  rv = text->QueryInterface(kITextContentIID,(void **)&textContent);
  if (NS_OK != rv) {
    printf("Created text content does not have the ITextContent interface.\n");
    return -1;
  }

  // Print the contents.
  nsAutoString stringBuf;
  textContent->GetText(stringBuf,0,textContent->GetLength());
  if (!stringBuf.Equals(nsString(destStr,destLen))) {
    printf("something wrong with the text in a text content\n");
  }
#endif

  // Create a simple container.
  nsIHTMLContent* container;
  nsIAtom* li = NS_NewAtom("LI");

  rv = NS_NewHTMLContainer(&container,li);
  if (NS_OK != rv) {
    printf("Could not create container.\n");
    return -1;
  }
  NS_ASSERTION(container->CanContainChildren(),"");
  container->SetDocument(myDoc);

  container->AppendChild(text, PR_FALSE);
  if (container->ChildCount() != 1) {
    printf("Container has wrong number of children.");
  }

  printf("begin attribute tests\n");
  testAttributes(container);
  printf("attribute tests complete\n");


  // Clean up memory.
  text->Release(); // The textContent interface.
  delete container;
  delete text;
  myDoc->Release();
  return 0;
}
