/* -*- Mode: C++ tab-width: 2 indent-tabs-mode: nil c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG

#include "TextEditorTest.h"

#include <stdio.h>

#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
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

#define TEST_RESULT(r) { if (NS_FAILED(r)) {printf("FAILURE result=%X\n", static_cast<uint32_t>(r)); return r; } }
#define TEST_POINTER(p) { if (!p) {printf("FAILURE null pointer\n"); return NS_ERROR_NULL_POINTER; } }

TextEditorTest::TextEditorTest()
{
  printf("constructed a TextEditorTest\n");
}

TextEditorTest::~TextEditorTest()
{
  printf("destroyed a TextEditorTest\n");
}

void TextEditorTest::Run(nsIEditor *aEditor, int32_t *outNumTests, int32_t *outNumTestsFailed)
{
  if (!aEditor) return;
  mTextEditor = do_QueryInterface(aEditor);
  mEditor = do_QueryInterface(aEditor);
  RunUnitTest(outNumTests, outNumTestsFailed);
}

nsresult TextEditorTest::RunUnitTest(int32_t *outNumTests, int32_t *outNumTestsFailed)
{
  NS_ENSURE_TRUE(outNumTests && outNumTestsFailed, NS_ERROR_NULL_POINTER);

  *outNumTests = 0;
  *outNumTestsFailed = 0;

  nsresult rv = InitDoc();
  TEST_RESULT(rv);
  // shouldn't we just bail on error here?

  // insert some simple text
  rv = mTextEditor->InsertText(NS_LITERAL_STRING("1234567890abcdefghij1234567890"));
  TEST_RESULT(rv);
  (*outNumTests)++;
  if (NS_FAILED(rv)) {
    ++(*outNumTestsFailed);
  }

  // insert some more text
  rv = mTextEditor->InsertText(NS_LITERAL_STRING("Moreover, I am cognizant of the interrelatedness of all communities and states.  I cannot sit idly by in Atlanta and not be concerned about what happens in Birmingham.  Injustice anywhere is a threat to justice everywhere"));
  TEST_RESULT(rv);
  (*outNumTests)++;
  if (NS_FAILED(rv)) {
    ++(*outNumTestsFailed);
  }

  rv = TestInsertBreak();
  TEST_RESULT(rv);
  (*outNumTests)++;
  if (NS_FAILED(rv)) {
    ++(*outNumTestsFailed);
  }

  rv = TestTextProperties();
  TEST_RESULT(rv);
  (*outNumTests)++;
  if (NS_FAILED(rv)) {
    ++(*outNumTestsFailed);
  }

  // get us back to the original document
  rv = mEditor->Undo(12);
  TEST_RESULT(rv);

  return rv;
}

nsresult TextEditorTest::InitDoc()
{
  nsresult rv = mEditor->SelectAll();
  TEST_RESULT(rv);
  rv = mEditor->DeleteSelection(nsIEditor::eNext, nsIEditor::eStrip);
  TEST_RESULT(rv);
  return rv;
}

nsresult TextEditorTest::TestInsertBreak()
{
  nsCOMPtr<nsISelection>selection;
  nsresult rv = mEditor->GetSelection(getter_AddRefs(selection));
  TEST_RESULT(rv);
  TEST_POINTER(selection.get());
  nsCOMPtr<nsIDOMNode>anchor;
  rv = selection->GetAnchorNode(getter_AddRefs(anchor));
  TEST_RESULT(rv);
  TEST_POINTER(anchor.get());
  selection->Collapse(anchor, 0);
  // insert one break
  printf("inserting a break\n");
  rv = mTextEditor->InsertLineBreak();
  TEST_RESULT(rv);
  mEditor->DebugDumpContent();

  // insert a second break adjacent to the first
  printf("inserting a second break\n");
  rv = mTextEditor->InsertLineBreak();
  TEST_RESULT(rv);
  mEditor->DebugDumpContent();

  return rv;
}

nsresult TextEditorTest::TestTextProperties()
{
  nsCOMPtr<nsIDOMDocument>doc;
  nsresult rv = mEditor->GetDocument(getter_AddRefs(doc));
  TEST_RESULT(rv);
  TEST_POINTER(doc.get());
  nsCOMPtr<nsIDOMNodeList>nodeList;
  // XXX This is broken, text nodes are not elements.
  nsAutoString textTag(NS_LITERAL_STRING("#text"));
  rv = doc->GetElementsByTagName(textTag, getter_AddRefs(nodeList));
  TEST_RESULT(rv);
  TEST_POINTER(nodeList.get());
  uint32_t count;
  nodeList->GetLength(&count);
  NS_ASSERTION(0!=count, "there are no text nodes in the document!");
  nsCOMPtr<nsIDOMNode>textNode;
  rv = nodeList->Item(count - 1, getter_AddRefs(textNode));
  TEST_RESULT(rv);
  TEST_POINTER(textNode.get());

  // set the whole text node to bold
  printf("set the whole first text node to bold\n");
  nsCOMPtr<nsISelection>selection;
  rv = mEditor->GetSelection(getter_AddRefs(selection));
  TEST_RESULT(rv);
  TEST_POINTER(selection.get());
  nsCOMPtr<nsIDOMCharacterData>textData;
  textData = do_QueryInterface(textNode);
  uint32_t length;
  textData->GetLength(&length);
  selection->Collapse(textNode, 0);
  selection->Extend(textNode, length);

  nsCOMPtr<nsIHTMLEditor> htmlEditor (do_QueryInterface(mTextEditor));
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  bool any = false;
  bool all = false;
  bool first=false;

  const nsString& empty = EmptyString();

  rv = htmlEditor->GetInlineProperty(nsGkAtoms::b, empty, empty, &first,
                                     &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(false==first, "first should be false");
  NS_ASSERTION(false==any, "any should be false");
  NS_ASSERTION(false==all, "all should be false");
  rv = htmlEditor->SetInlineProperty(nsGkAtoms::b, empty, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(nsGkAtoms::b, empty, empty, &first,
                                     &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();

  // remove the bold we just set
  printf("set the whole first text node to not bold\n");
  rv = htmlEditor->RemoveInlineProperty(nsGkAtoms::b, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(nsGkAtoms::b, empty, empty, &first,
                                     &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(false==first, "first should be false");
  NS_ASSERTION(false==any, "any should be false");
  NS_ASSERTION(false==all, "all should be false");
  mEditor->DebugDumpContent();

  // set all but the first and last character to bold
  printf("set the first text node (1, length-1) to bold and italic, and (2, length-1) to underline.\n");
  selection->Collapse(textNode, 1);
  selection->Extend(textNode, length-1);
  rv = htmlEditor->SetInlineProperty(nsGkAtoms::b, empty, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(nsGkAtoms::b, empty, empty, &first,
                                     &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();
  // make all that same text italic
  rv = htmlEditor->SetInlineProperty(nsGkAtoms::i, empty, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(nsGkAtoms::i, empty, empty, &first,
                                     &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  rv = htmlEditor->GetInlineProperty(nsGkAtoms::b, empty, empty, &first,
                                     &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();

  // make all the text underlined, except for the first 2 and last 2 characters
  rv = doc->GetElementsByTagName(textTag, getter_AddRefs(nodeList));
  TEST_RESULT(rv);
  TEST_POINTER(nodeList.get());
  nodeList->GetLength(&count);
  NS_ASSERTION(0!=count, "there are no text nodes in the document!");
  rv = nodeList->Item(count-2, getter_AddRefs(textNode));
  TEST_RESULT(rv);
  TEST_POINTER(textNode.get());
  textData = do_QueryInterface(textNode);
  textData->GetLength(&length);
  NS_ASSERTION(length==915, "wrong text node");
  selection->Collapse(textNode, 1);
  selection->Extend(textNode, length-2);
  rv = htmlEditor->SetInlineProperty(nsGkAtoms::u, empty, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(nsGkAtoms::u, empty, empty, &first,
                                     &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();

  return rv;
}

#endif
