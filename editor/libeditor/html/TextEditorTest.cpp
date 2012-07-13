/* -*- Mode: C++ tab-width: 2 indent-tabs-mode: nil c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG

#include <stdio.h>

#include "TextEditorTest.h"
#include "nsDebug.h"
#include "nsEditProperty.h"
#include "nsError.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIPlaintextEditor.h"
#include "nsISelection.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"

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
  
  NS_ENSURE_TRUE(outNumTests && outNumTestsFailed, NS_ERROR_NULL_POINTER);
  
  *outNumTests = 0;
  *outNumTestsFailed = 0;
  
  result = InitDoc();
  TEST_RESULT(result);
  // shouldn't we just bail on error here?
  
  // insert some simple text
  result = mTextEditor->InsertText(NS_LITERAL_STRING("1234567890abcdefghij1234567890"));
  TEST_RESULT(result);
  (*outNumTests)++;
  if (NS_FAILED(result))
    ++(*outNumTestsFailed);
  
  // insert some more text
  result = mTextEditor->InsertText(NS_LITERAL_STRING("Moreover, I am cognizant of the interrelatedness of all communities and states.  I cannot sit idly by in Atlanta and not be concerned about what happens in Birmingham.  Injustice anywhere is a threat to justice everywhere"));
  TEST_RESULT(result);
  (*outNumTests)++;
  if (NS_FAILED(result))
    ++(*outNumTestsFailed);

  result = TestInsertBreak();
  TEST_RESULT(result);
  (*outNumTests)++;
  if (NS_FAILED(result))
    ++(*outNumTestsFailed);

  result = TestTextProperties();
  TEST_RESULT(result);
  (*outNumTests)++;
  if (NS_FAILED(result))
    ++(*outNumTestsFailed);

  // get us back to the original document
  result = mEditor->Undo(12);
  TEST_RESULT(result);

  return result;
}

nsresult TextEditorTest::InitDoc()
{
  nsresult result = mEditor->SelectAll();
  TEST_RESULT(result);
  result = mEditor->DeleteSelection(nsIEditor::eNext, nsIEditor::eStrip);
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
  // XXX This is broken, text nodes are not elements.
  nsAutoString textTag(NS_LITERAL_STRING("#text"));
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
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  bool any = false;
  bool all = false;
  bool first=false;

  const nsAFlatString& empty = EmptyString();

  result = htmlEditor->GetInlineProperty(nsEditProperty::b, empty, empty, &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(false==first, "first should be false");
  NS_ASSERTION(false==any, "any should be false");
  NS_ASSERTION(false==all, "all should be false");
  result = htmlEditor->SetInlineProperty(nsEditProperty::b, empty, empty);
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsEditProperty::b, empty, empty, &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();

  // remove the bold we just set
  printf("set the whole first text node to not bold\n");
  result = htmlEditor->RemoveInlineProperty(nsEditProperty::b, empty);
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsEditProperty::b, empty, empty, &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(false==first, "first should be false");
  NS_ASSERTION(false==any, "any should be false");
  NS_ASSERTION(false==all, "all should be false");
  mEditor->DebugDumpContent();

  // set all but the first and last character to bold
  printf("set the first text node (1, length-1) to bold and italic, and (2, length-1) to underline.\n");
  selection->Collapse(textNode, 1);
  selection->Extend(textNode, length-1);
  result = htmlEditor->SetInlineProperty(nsEditProperty::b, empty, empty);
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsEditProperty::b, empty, empty, &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();
  // make all that same text italic
  result = htmlEditor->SetInlineProperty(nsEditProperty::i, empty, empty);
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsEditProperty::i, empty, empty, &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  result = htmlEditor->GetInlineProperty(nsEditProperty::b, empty, empty, &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
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
  result = htmlEditor->SetInlineProperty(nsEditProperty::u, empty, empty);
  TEST_RESULT(result);
  result = htmlEditor->GetInlineProperty(nsEditProperty::u, empty, empty, &first, &any, &all);
  TEST_RESULT(result);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();

  return result;
}



#endif


