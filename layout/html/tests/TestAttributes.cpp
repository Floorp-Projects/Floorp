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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include <stdio.h>
#include "nscore.h"
#include "nsIAtom.h"
#include "nsCRT.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsITextContent.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsISupportsArray.h"
#include "nsDocument.h"
#include "nsIURL.h"
#include "nsIDOMText.h"
#include "nsINameSpaceManager.h"

void testAttributes(nsIHTMLContent* content) {
  nsIAtom* sBORDER = NS_NewAtom("border");
  nsIAtom* sHEIGHT = NS_NewAtom("height");
  nsIAtom* sSRC = NS_NewAtom("src");
  nsIAtom* sBAD = NS_NewAtom("badattribute");
  nsString sempty;
  nsString sfoo_gif(NS_LITERAL_STRING("foo.gif"));

  content->SetAttr(kNameSpaceID_None, sBORDER, EmptyString(), PR_FALSE);
  content->SetAttribute(kNameSpaceID_None, sHEIGHT, sempty, PR_FALSE);
  content->SetAttribute(kNameSpaceID_None, sSRC, sfoo_gif, PR_FALSE);

  nsHTMLValue ret;
  nsresult rv;
  rv = content->GetHTMLAttribute(sBORDER, ret);
  if (rv == NS_CONTENT_ATTR_NOT_THERE || ret.GetUnit() != eHTMLUnit_String) {
    printf("test 0 failed\n");
  }

  rv = content->GetHTMLAttribute(sBAD, ret);
  if (rv != NS_CONTENT_ATTR_NOT_THERE) {
    printf("test 2 failed\n");
  }

  content->UnsetAttribute(kNameSpaceID_None, sWIDTH, PR_FALSE);

  nsISupportsArray* allNames;
  NS_NewISupportsArray(&allNames);
  if (nsnull == allNames) return;

  PRInt32 na;
  content->GetAttributeCount(na);
  if (na != 3) {
    printf("test 5 (unset attriubte) failed\n");
  }
  PRInt32 index;
  for (index = 0; index < na; index++) {
    nsIAtom* name, *prefix = nsnull;
    PRInt32 nameSpaceID;
    content->GetAttributeNameAt(index, nameSpaceID, name, prefix);
    allNames->AppendElement(name);
    NS_RELEASE(name);
    NS_IF_RELEASE(prefix);
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
  val = (NS_ConvertASCIItoUCS2("mrString")).EqualsLiteral("mrString"); // XXXjag
  if (PR_TRUE != val) {
    printf("test 0 failed\n");
  }
  val = (NS_ConvertASCIItoUCS2("mrString")).EqualsLiteral("MRString"); // XXXjag
  if (PR_FALSE != val) {
    printf("test 1 failed\n");
  }
  val = (NS_ConvertASCIItoUCS2("mrString")).EqualsLiteral("mrStri"); // XXXjag
  if (PR_FALSE != val) {
    printf("test 2 failed\n");
  }
  val = (NS_ConvertASCIItoUCS2("mrStri")).EqualsLiteral("mrString"); // XXXjag
  if (PR_FALSE != val) {
    printf("test 3 failed\n");
  }
  // EqualsIgnoreCase
  val = (NS_ConvertASCIItoUCS2("mrString")).LowerCaseEqualsLiteral("mrstring");
  if (PR_TRUE != val) {
    printf("test 4 failed\n");
  }
  val = (NS_ConvertASCIItoUCS2("mrString")).LowerCaseEqualsLiteral("mrstring");
  if (PR_TRUE != val) {
    printf("test 5 failed\n");
  }
  val = (NS_ConvertASCIItoUCS2("mrString")).LowerCaseEqualsLiteral("mrstri");
  if (PR_FALSE != val) {
    printf("test 6 failed\n");
  }
  val = (NS_ConvertASCIItoUCS2("mrStri")).LowerCaseEqualsLiteral("mrstring");
  if (PR_FALSE != val) {
    printf("test 7 failed\n");
  }
  // String vs. Ident
  val = (NS_ConvertASCIItoUCS2("mrString")).EqualsIgnoreCase(NS_NewAtom("mrString"));
  if (PR_TRUE != val) {
    printf("test 8 failed\n");
  }
  val = (NS_ConvertASCIItoUCS2("mrString")).EqualsIgnoreCase(NS_NewAtom("MRStrINg"));
  if (PR_TRUE != val) {
    printf("test 9 failed\n");
  }
  val = (NS_ConvertASCIItoUCS2("mrString")).EqualsIgnoreCase(NS_NewAtom("mrStri"));
  if (PR_FALSE != val) {
    printf("test 10 failed\n");
  }
  val = (NS_ConvertASCIItoUCS2("mrStri")).EqualsIgnoreCase(NS_NewAtom("mrString"));
  if (PR_FALSE != val) {
    printf("test 11 failed\n");
  }

  printf("string tests complete\n");
}

class MyDocument : public nsDocument {
public:
  MyDocument();
  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener)
  {
    return NS_OK;
  }

  NS_IMETHOD    ImportNode(nsIDOMNode* aImportedNode, PRBool aDeep, nsIDOMNode** aReturn) {
    return NS_OK;
  }

  NS_IMETHOD    CreateElementNS(const nsAString& aNamespaceURI, const nsAString& aQualifiedName, nsIDOMElement** aReturn) {
    return NS_OK;
  }

  NS_IMETHOD    CreateAttributeNS(const nsAString& aNamespaceURI, const nsAString& aQualifiedName, nsIDOMAttr** aReturn) {
    return NS_OK;
  }

  NS_IMETHOD    GetElementsByTagNameNS(const nsAString& aNamespaceURI, const nsAString& aLocalName, nsIDOMNodeList** aReturn) {
    return NS_OK;
  }

  NS_IMETHOD    GetElementById(const nsAString& aElementId, nsIDOMElement** aReturn) {
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
  /* Create Byte2Unicode converter? Not anymore. The converters are not tested
  here, they have their own test code. And if you just want to use them, you 
  need a properly intialized xpcom system. This simple test program doesn't do
  that. */

  // Create a unicode string
  static const char* srcStr = "This is some meaningless text about nothing at all";
  nsresult rv;
  PRUint32 origSrcLen = nsCRT::strlen((char *)srcStr);
  const int BUFFER_LENGTH = 100;
  PRUnichar destStr[BUFFER_LENGTH];
  PRUint32 srcLen = origSrcLen;
  PRUint32 destLen = BUFFER_LENGTH;
  // hacky Ascii conversion to unicode, because we don't have a real Converter.
  for (PRUint32 i=0; i<srcLen; i++) destStr[i] = ((PRUint8)srcStr[i]);

  // Create test document.
  MyDocument *myDoc = new MyDocument();
  if (myDoc) {
    testStrings(myDoc);
  } else {
    printf("Out of memory trying to create document\n");
    return -1;
  }


  // Create a new text content object.
  nsIContent *text;
  rv = NS_NewTextNode(&text);
  if (NS_OK != rv) {
    printf("Could not create text content.\n");
    return -1;
  }

  nsIDOMText* txt = nsnull;
  text->QueryInterface(NS_GET_IID(nsIDOMText), (void**) &txt);
  nsAutoString tmp(destStr);
  txt->AppendData(tmp);
  NS_RELEASE(txt);

#if 0
  // Query ITextContent interface
  nsITextContent* textContent;
  rv = text->QueryInterface(NS_GET_IID(nsITextContent),(void **)&textContent);
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
  nsIAtom* li = NS_NewAtom("li");

  nsCOMPtr<nsINodeInfo> ni;
  myDoc->NodeInfoManager()->GetNodeInfo(li, nsnull, kNameSpaceID_None,
                                        getter_AddRefs(ni));

  rv = NS_NewHTMLLIElement(&container,ni);
  if (NS_OK != rv) {
    printf("Could not create container.\n");
    return -1;
  }
  container->SetDocument(myDoc, PR_FALSE, PR_TRUE);

  container->AppendChildTo(text, PR_FALSE, PR_FALSE);
  PRInt32 nk;
  container->ChildCount(nk);
  if (nk != 1) {
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
