/* -*- Mode: C++ tab-width: 2 indent-tabs-mode: nil c-basic-offset: 2 -*- */
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

#include <stdio.h>

#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "TextEditorTest.h"
#include "nsISelection.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIEditProperty.h"
#include "nsString.h"

#ifdef NS_DEBUG

#define TEST_RESULT(r) { if (NS_FAILED(r)) {printf("FAILURE result=%X\n", r); return r; } }
#define TEST_POINTER(p) { if (!p) {printf("FAILURE null pointer\n"); return NS_ERROR_NULL_POINTER; } }

TextEditorTest::TextEditorTest()
{
  printf("constructed a TextEditorTest\n");
}

TextEditorTest::~TextEditorTest()
{
  printf("destroyed a TextEditorTest\n");
}

void TextEditorTest::Run(nsIEditor *aEditor, PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
{
  if (!aEditor) return;
  mTextEditor = do_QueryInterface(aEditor);
  mEditor = do_QueryInterface(aEditor);
  RunUnitTest(outNumTests, outNumTestsFailed);
}

nsresult TextEditorTest::RunUnitTest(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
{
  nsresult result;
  
  if (!outNumTests || !outNumTestsFailed)
    return NS_ERROR_NULL_POINTER;
  
  *outNumTests = 0;
  *outNumTestsFailed = 0;
  
  result = InitDoc();
  TEST_RESULT(result);
  // shouldn't we just bail on error here?
  
  // insert some simple text
  result = mTextEditor->InsertText(NS_LITERAL_STRING("1234567890abcdefghij1234567890"));
  TEST_RESULT(result);
  (*outNumTests)++;
  (*outNumTestsFailed) += (NS_FAILED(result) != NS_OK);
  
  // insert some more text
  result = mTextEditor->InsertText(NS_LITERAL_STRING("Moreover, I am cognizant of the interrelatedness of all communities and states.  I cannot sit idly by in Atlanta and not be concerned about what happens in Birmingham.  Injustice anywhere is a threat to justice everywhere"));
  TEST_RESULT(result);
  (*outNumTests)++;
  (*outNumTestsFailed) += (NS_FAILED(result) != NS_OK);

  result = TestInsertBreak();
  TEST_RESULT(result);
  (*outNumTests)++;
  (*outNumTestsFailed) += (NS_FAILED(result) != NS_OK);

  result = TestTextProperties();
  TEST_RESULT(result);
  (*outNumTests)++;
  (*outNumTestsFailed) += (NS_FAILED(result) != NS_OK);

  // get us back to the original document
  result = mEditor->Undo(12);
  TEST_RESULT(result);

  return result;
}

nsresult TextEditorTest::InitDoc()
{
  nsresult result = mEditor->SelectAll();
  TEST_RESULT(result);
  result = mEditor->DeleteSelection(nsIEditor::eNext);
  TEST_RESULT(result);
  return result;
}

nsresult TextEditorTest::TestInsertBreak()
{
  nsCOMPtr<nsISelection>selection;
  nsresult result = mEditor->GetSelection(getter_AddRefs(selection));
  TEST_RESULT(result);
  TEST_POINTER(selection.get());
  nsCOMPtr<nsIDOMNode>anchor;
  result = selection->GetAnchorNode(getter_AddRefs(anchor));
  TEST_RESULT(result);
  TEST_POINTER(anchor.get());
  selection->Collapse(anchor, 0);
  // insert one break
  printf("inserting a break\n");
  result = mTextEditor->InsertLineBreak();
  TEST_RESULT(result);
  mEditor->DebugDumpContent();

  // insert a second break adjacent to the first
  printf("inserting a second break\n");
  result = mTextEditor->InsertLineBreak();
  TEST_RESULT(result);
  mEditor->DebugDumpContent();
    
  return result;
}

nsresult TextEditorTest::TestTextProperties()
{
  nsCOMPtr<nsIDOMDocument>doc;
  nsresult result = mEditor->GetDocument(getter_AddRefs(doc));
  TEST_RESULT(result);
  TEST_POINTER(doc.get());
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsAutoString textTag; textTag.AssignWithConversion("__moz_text");
  result = doc->GetElementsByTagName(textTag, getter_AddRefs(nodeList));
  TEST_RESULT(result);
  TEST_POINTER(nodeList.get());
  PRUint32 count;
  nodeList->GetLength(&count);
  NS_ASSERTION(0!=count, "there are no text nodes in the document!");
  nsCOMPtr<nsIDOMNode>textNode;
  result = nodeList->Item(count-1, getter_AddRefs(textNode));
  TEST_RESULT(result);
  TEST_POINTER(textNode.get());

  // set the whole text node to bold
  printf("set the whole first text node to bold\n");
  nsCOMPtr<nsISelection>selection;
  result = mEditor->GetSelection(getter_AddRefs(selection));
  TEST_RESULT(result);
  TEST_POINTER(selection.get());
  nsCOMPtr<nsIDOMCharacterData>textData;
  textData = do_QueryInterface(textNode);
  PRUint32 length;
  textData->GetLength(&length);
  selection->Collapse(textNode, 0);
  selection->Extend(textNode, length);

  nsCOMPtr<nsIHTMLEditor> htmlEditor (do_QueryInterface(mTextEditor));
  if (!htmlEditor)
    return NS_ERROR_FAILURE;

  PRBool any = PR_FALSE;
  PRBool all = PR_FALSE;
  PRBool first=PR_FALSE;

  result = htmlEditor->GetInlineProperty(nsIEditProperty::b, nsString(), nsString(), &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(PR_FALSE==first, "first should be false");
  NS_ASSERTION(PR_FALSE==any, "any should be false");
  NS_ASSERTION(PR_FALSE==all, "all should be false");
  result = htmlEditor->SetInlineProperty(nsIEditProperty::b, nsString(), nsString());
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsIEditProperty::b, nsString(), nsString(), &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(PR_TRUE==first, "first should be true");
  NS_ASSERTION(PR_TRUE==any, "any should be true");
  NS_ASSERTION(PR_TRUE==all, "all should be true");
  mEditor->DebugDumpContent();

  // remove the bold we just set
  printf("set the whole first text node to not bold\n");
  result = htmlEditor->RemoveInlineProperty(nsIEditProperty::b, nsString());
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsIEditProperty::b, nsString(), nsString(), &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(PR_FALSE==first, "first should be false");
  NS_ASSERTION(PR_FALSE==any, "any should be false");
  NS_ASSERTION(PR_FALSE==all, "all should be false");
  mEditor->DebugDumpContent();

  // set all but the first and last character to bold
  printf("set the first text node (1, length-1) to bold and italic, and (2, length-1) to underline.\n");
  selection->Collapse(textNode, 1);
  selection->Extend(textNode, length-1);
  result = htmlEditor->SetInlineProperty(nsIEditProperty::b, nsString(), nsString());
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsIEditProperty::b, nsString(), nsString(), &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(PR_TRUE==first, "first should be true");
  NS_ASSERTION(PR_TRUE==any, "any should be true");
  NS_ASSERTION(PR_TRUE==all, "all should be true");
  mEditor->DebugDumpContent();
  // make all that same text italic
  result = htmlEditor->SetInlineProperty(nsIEditProperty::i, nsString(), nsString());
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsIEditProperty::i, nsString(), nsString(), &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(PR_TRUE==first, "first should be true");
  NS_ASSERTION(PR_TRUE==any, "any should be true");
  NS_ASSERTION(PR_TRUE==all, "all should be true");
  result = htmlEditor->GetInlineProperty(nsIEditProperty::b, nsString(), nsString(), &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(PR_TRUE==first, "first should be true");
  NS_ASSERTION(PR_TRUE==any, "any should be true");
  NS_ASSERTION(PR_TRUE==all, "all should be true");
  mEditor->DebugDumpContent();

  // make all the text underlined, except for the first 2 and last 2 characters
  result = doc->GetElementsByTagName(textTag, getter_AddRefs(nodeList));
  TEST_RESULT(result);
  TEST_POINTER(nodeList.get());
  nodeList->GetLength(&count);
  NS_ASSERTION(0!=count, "there are no text nodes in the document!");
  result = nodeList->Item(count-2, getter_AddRefs(textNode));
  TEST_RESULT(result);
  TEST_POINTER(textNode.get());
  textData = do_QueryInterface(textNode);
  textData->GetLength(&length);
  NS_ASSERTION(length==915, "wrong text node");
  selection->Collapse(textNode, 1);
  selection->Extend(textNode, length-2);
  result = htmlEditor->SetInlineProperty(nsIEditProperty::u, nsString(), nsString());
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsIEditProperty::u, nsString(), nsString(), &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(PR_TRUE==first, "first should be true");
  NS_ASSERTION(PR_TRUE==any, "any should be true");
  NS_ASSERTION(PR_TRUE==all, "all should be true");
  mEditor->DebugDumpContent();

  return result;
}



#endif


