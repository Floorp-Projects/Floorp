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
#include "nsIDocument.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsINodeList.h"
#include "nsIPlaintextEditor.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "mozilla/dom/Selection.h"

using mozilla::dom::Selection;

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
  RefPtr<Selection>selection;
  nsresult rv = mEditor->GetSelection(getter_AddRefs(selection));
  TEST_RESULT(rv);
  TEST_POINTER(selection.get());
  nsCOMPtr<nsINode> anchor = selection->GetAnchorNode();
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
  nsCOMPtr<nsIDocument> doc = mEditor->AsEditorBase()->GetDocument();
  TEST_POINTER(doc.get());
  // XXX This is broken, text nodes are not elements.
  nsAutoString textTag(NS_LITERAL_STRING("#text"));
  nsCOMPtr<nsINodeList>nodeList = doc->GetElementsByTagName(textTag);
  TEST_POINTER(nodeList.get());
  uint32_t count = nodeList->Length();
  NS_ASSERTION(0 != count, "there are no text nodes in the document!");
  nsCOMPtr<nsINode>textNode = nodeList->Item(count - 1);
  TEST_POINTER(textNode.get());

  // set the whole text node to bold
  printf("set the whole first text node to bold\n");
  RefPtr<Selection>selection;
  nsresult rv = mEditor->GetSelection(getter_AddRefs(selection));
  TEST_RESULT(rv);
  TEST_POINTER(selection.get());
  uint32_t length = textNode->Length();
  selection->Collapse(textNode, 0);
  selection->Extend(textNode, length);

  nsCOMPtr<nsIHTMLEditor> htmlEditor (do_QueryInterface(mTextEditor));
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  bool any = false;
  bool all = false;
  bool first=false;

  const nsString& empty = EmptyString();

  NS_NAMED_LITERAL_STRING(b, "b");
  NS_NAMED_LITERAL_STRING(i, "i");
  NS_NAMED_LITERAL_STRING(u, "u");

  rv = htmlEditor->GetInlineProperty(b, empty, empty, &first, &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(false==first, "first should be false");
  NS_ASSERTION(false==any, "any should be false");
  NS_ASSERTION(false==all, "all should be false");
  rv = htmlEditor->SetInlineProperty(b, empty, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(b, empty, empty, &first, &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();

  // remove the bold we just set
  printf("set the whole first text node to not bold\n");
  rv = htmlEditor->RemoveInlineProperty(b, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(b, empty, empty, &first, &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(false==first, "first should be false");
  NS_ASSERTION(false==any, "any should be false");
  NS_ASSERTION(false==all, "all should be false");
  mEditor->DebugDumpContent();

  // set all but the first and last character to bold
  printf("set the first text node (1, length-1) to bold and italic, and (2, length-1) to underline.\n");
  selection->Collapse(textNode, 1);
  selection->Extend(textNode, length-1);
  rv = htmlEditor->SetInlineProperty(b, empty, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(b, empty, empty, &first, &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();
  // make all that same text italic
  rv = htmlEditor->SetInlineProperty(i, empty, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(i, empty, empty, &first, &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  rv = htmlEditor->GetInlineProperty(b, empty, empty, &first, &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();

  // make all the text underlined, except for the first 2 and last 2 characters
  nodeList = doc->GetElementsByTagName(textTag);
  TEST_POINTER(nodeList.get());
  count = nodeList->Length();
  NS_ASSERTION(0!=count, "there are no text nodes in the document!");
  textNode = nodeList->Item(count-2);
  TEST_POINTER(textNode.get());
  length = textNode->Length();
  NS_ASSERTION(length==915, "wrong text node");
  selection->Collapse(textNode, 1);
  selection->Extend(textNode, length-2);
  rv = htmlEditor->SetInlineProperty(u, empty, empty);
  TEST_RESULT(rv);
  rv = htmlEditor->GetInlineProperty(u, empty, empty, &first, &any, &all);
  TEST_RESULT(rv);
  NS_ASSERTION(true==first, "first should be true");
  NS_ASSERTION(true==any, "any should be true");
  NS_ASSERTION(true==all, "all should be true");
  mEditor->DebugDumpContent();

  return rv;
}

#endif
