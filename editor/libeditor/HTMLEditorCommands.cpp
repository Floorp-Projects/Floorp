/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include <stdio.h>                      // for printf

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/EditorBase.h"         // for EditorBase
#include "mozilla/ErrorResult.h"
#include "mozilla/HTMLEditor.h"         // for HTMLEditor
#include "mozilla/HTMLEditorCommands.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsCOMPtr.h"                   // for nsCOMPtr, do_QueryInterface, etc
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsError.h"                    // for NS_OK, NS_ERROR_FAILURE, etc
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::font, etc
#include "nsAtom.h"                    // for nsAtom, etc
#include "nsIClipboard.h"               // for nsIClipboard, etc
#include "nsICommandParams.h"           // for nsICommandParams, etc
#include "nsID.h"
#include "nsIEditor.h"                  // for nsIEditor
#include "nsIHTMLEditor.h"              // for nsIHTMLEditor, etc
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsReadableUtils.h"            // for EmptyString
#include "nsString.h"                   // for nsAutoString, nsString, etc
#include "nsStringFwd.h"                // for nsString

class nsISupports;

namespace mozilla {
using dom::Element;

//prototype
static nsresult
GetListState(HTMLEditor* aHTMLEditor, bool* aMixed, nsAString& aLocalName);

//defines
#define STATE_ENABLED  "state_enabled"
#define STATE_ALL "state_all"
#define STATE_ANY "state_any"
#define STATE_MIXED "state_mixed"
#define STATE_BEGIN "state_begin"
#define STATE_END "state_end"
#define STATE_ATTRIBUTE "state_attribute"
#define STATE_DATA "state_data"


HTMLEditorCommandBase::HTMLEditorCommandBase()
{
}

NS_IMPL_ISUPPORTS(HTMLEditorCommandBase, nsIControllerCommand)


StateUpdatingCommandBase::StateUpdatingCommandBase(nsAtom* aTagName)
  : HTMLEditorCommandBase()
  , mTagName(aTagName)
{
  MOZ_ASSERT(mTagName);
}

StateUpdatingCommandBase::~StateUpdatingCommandBase()
{
}

NS_IMETHODIMP
StateUpdatingCommandBase::IsCommandEnabled(const char* aCommandName,
                                           nsISupports* refCon,
                                           bool* outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}


NS_IMETHODIMP
StateUpdatingCommandBase::DoCommand(const char* aCommandName,
                                    nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return ToggleState(htmlEditor);
}

NS_IMETHODIMP
StateUpdatingCommandBase::DoCommandParams(const char* aCommandName,
                                          nsICommandParams* aParams,
                                          nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
StateUpdatingCommandBase::GetCommandStateParams(const char* aCommandName,
                                                nsICommandParams* aParams,
                                                nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return GetCurrentState(htmlEditor, aParams);
}

NS_IMETHODIMP
PasteNoFormattingCommand::IsCommandEnabled(const char* aCommandName,
                                           nsISupports* refCon,
                                           bool* outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = false;

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }

  // This command is only implemented by nsIHTMLEditor, since
  //  pasting in a plaintext editor automatically only supplies
  //  "unformatted" text
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return htmlEditor->CanPaste(nsIClipboard::kGlobalClipboard, outCmdEnabled);
}

NS_IMETHODIMP
PasteNoFormattingCommand::DoCommand(const char* aCommandName,
                                    nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return htmlEditor->PasteNoFormatting(nsIClipboard::kGlobalClipboard);
}

NS_IMETHODIMP
PasteNoFormattingCommand::DoCommandParams(const char* aCommandName,
                                          nsICommandParams* aParams,
                                          nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
PasteNoFormattingCommand::GetCommandStateParams(const char* aCommandName,
                                                nsICommandParams* aParams,
                                                nsISupports* refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled = false;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->SetBooleanValue(STATE_ENABLED, enabled);
}

StyleUpdatingCommand::StyleUpdatingCommand(nsAtom* aTagName)
  : StateUpdatingCommandBase(aTagName)
{
}

nsresult
StyleUpdatingCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                      nsICommandParams *aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool firstOfSelectionHasProp = false;
  bool anyOfSelectionHasProp = false;
  bool allOfSelectionHasProp = false;

  nsresult rv = aHTMLEditor->GetInlineProperty(mTagName, nullptr,
                                               EmptyString(),
                                               &firstOfSelectionHasProp,
                                               &anyOfSelectionHasProp,
                                               &allOfSelectionHasProp);

  aParams->SetBooleanValue(STATE_ENABLED, NS_SUCCEEDED(rv));
  aParams->SetBooleanValue(STATE_ALL, allOfSelectionHasProp);
  aParams->SetBooleanValue(STATE_ANY, anyOfSelectionHasProp);
  aParams->SetBooleanValue(STATE_MIXED, anyOfSelectionHasProp
           && !allOfSelectionHasProp);
  aParams->SetBooleanValue(STATE_BEGIN, firstOfSelectionHasProp);
  aParams->SetBooleanValue(STATE_END, allOfSelectionHasProp);//not completely accurate
  return NS_OK;
}

nsresult
StyleUpdatingCommand::ToggleState(HTMLEditor* aHTMLEditor)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  //create some params now...
  nsresult rv;
  nsCOMPtr<nsICommandParams> params =
      do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
  if (NS_FAILED(rv) || !params)
    return rv;

  // tags "href" and "name" are special cases in the core editor
  // they are used to remove named anchor/link and shouldn't be used for insertion
  bool doTagRemoval;
  if (mTagName == nsGkAtoms::href || mTagName == nsGkAtoms::name) {
    doTagRemoval = true;
  } else {
    // check current selection; set doTagRemoval if formatting should be removed
    rv = GetCurrentState(aHTMLEditor, params);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = params->GetBooleanValue(STATE_ALL, &doTagRemoval);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (doTagRemoval) {
    // Also remove equivalent properties (bug 317093)
    if (mTagName == nsGkAtoms::b) {
      rv = aHTMLEditor->RemoveInlineProperty(nsGkAtoms::strong, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (mTagName == nsGkAtoms::i) {
      rv = aHTMLEditor->RemoveInlineProperty(nsGkAtoms::em, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (mTagName == nsGkAtoms::strike) {
      rv = aHTMLEditor->RemoveInlineProperty(nsGkAtoms::s, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = aHTMLEditor->RemoveInlineProperty(mTagName, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return rv;
  }

  // Superscript and Subscript styles are mutually exclusive
  aHTMLEditor->BeginTransaction();

  if (mTagName == nsGkAtoms::sub || mTagName == nsGkAtoms::sup) {
    rv = aHTMLEditor->RemoveInlineProperty(mTagName, nullptr);
  }
  if (NS_SUCCEEDED(rv)) {
    rv = aHTMLEditor->SetInlineProperty(mTagName, nullptr, EmptyString());
  }

  aHTMLEditor->EndTransaction();

  return rv;
}

ListCommand::ListCommand(nsAtom* aTagName)
  : StateUpdatingCommandBase(aTagName)
{
}

nsresult
ListCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                             nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool bMixed;
  nsAutoString localName;
  nsresult rv = GetListState(aHTMLEditor, &bMixed, localName);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList = mTagName->Equals(localName);
  aParams->SetBooleanValue(STATE_ALL, !bMixed && inList);
  aParams->SetBooleanValue(STATE_MIXED, bMixed);
  aParams->SetBooleanValue(STATE_ENABLED, true);
  return NS_OK;
}

nsresult
ListCommand::ToggleState(HTMLEditor* aHTMLEditor)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsICommandParams> params =
      do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
  if (NS_FAILED(rv) || !params)
    return rv;

  rv = GetCurrentState(aHTMLEditor, params);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList;
  rv = params->GetBooleanValue(STATE_ALL,&inList);
  NS_ENSURE_SUCCESS(rv, rv);

  nsDependentAtomString listType(mTagName);
  if (inList) {
    rv = aHTMLEditor->RemoveList(listType);
  } else {
    rv = aHTMLEditor->MakeOrChangeList(listType, false, EmptyString());
  }

  return rv;
}

ListItemCommand::ListItemCommand(nsAtom* aTagName)
  : StateUpdatingCommandBase(aTagName)
{
}

nsresult
ListItemCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                 nsICommandParams *aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool bMixed, bLI, bDT, bDD;
  nsresult rv = aHTMLEditor->GetListItemState(&bMixed, &bLI, &bDT, &bDD);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList = false;
  if (!bMixed) {
    if (bLI) {
      inList = mTagName == nsGkAtoms::li;
    } else if (bDT) {
      inList = mTagName == nsGkAtoms::dt;
    } else if (bDD) {
      inList = mTagName == nsGkAtoms::dd;
    }
  }

  aParams->SetBooleanValue(STATE_ALL, !bMixed && inList);
  aParams->SetBooleanValue(STATE_MIXED, bMixed);

  return NS_OK;
}

nsresult
ListItemCommand::ToggleState(HTMLEditor* aHTMLEditor)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool inList;
  // Need to use mTagName????
  nsresult rv;
  nsCOMPtr<nsICommandParams> params =
      do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
  if (NS_FAILED(rv) || !params)
    return rv;
  rv = GetCurrentState(aHTMLEditor, params);
  rv = params->GetBooleanValue(STATE_ALL,&inList);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (inList) {
    // To remove a list, first get what kind of list we're in
    bool bMixed;
    nsAutoString localName;
    rv = GetListState(aHTMLEditor, &bMixed, localName);
    NS_ENSURE_SUCCESS(rv, rv);
    if (localName.IsEmpty() || bMixed) {
      return rv;
    }
    return aHTMLEditor->RemoveList(localName);
  }

  // Set to the requested paragraph type
  //XXX Note: This actually doesn't work for "LI",
  //    but we currently don't use this for non DL lists anyway.
  // Problem: won't this replace any current block paragraph style?
  return aHTMLEditor->SetParagraphFormat(nsDependentAtomString(mTagName));
}

NS_IMETHODIMP
RemoveListCommand::IsCommandEnabled(const char* aCommandName,
                                    nsISupports* refCon,
                                    bool* outCmdEnabled)
{
  *outCmdEnabled = false;
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }

  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);

  if (!editorBase->IsSelectionEditable()) {
    return NS_OK;
  }

  // It is enabled if we are in any list type
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  bool bMixed;
  nsAutoString localName;
  nsresult rv = GetListState(htmlEditor, &bMixed, localName);
  NS_ENSURE_SUCCESS(rv, rv);

  *outCmdEnabled = bMixed || !localName.IsEmpty();
  return NS_OK;
}


NS_IMETHODIMP
RemoveListCommand::DoCommand(const char* aCommandName, nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  // This removes any list type
  return htmlEditor->RemoveList(EmptyString());
}

NS_IMETHODIMP
RemoveListCommand::DoCommandParams(const char* aCommandName,
                                   nsICommandParams* aParams,
                                   nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
RemoveListCommand::GetCommandStateParams(const char* aCommandName,
                                         nsICommandParams* aParams,
                                         nsISupports* refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

NS_IMETHODIMP
IndentCommand::IsCommandEnabled(const char* aCommandName,
                                nsISupports* refCon, bool* outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}

NS_IMETHODIMP
IndentCommand::DoCommand(const char* aCommandName, nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  return htmlEditor->Indent(NS_LITERAL_STRING("indent"));
}

NS_IMETHODIMP
IndentCommand::DoCommandParams(const char* aCommandName,
                               nsICommandParams* aParams,
                               nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
IndentCommand::GetCommandStateParams(const char* aCommandName,
                                     nsICommandParams* aParams,
                                     nsISupports* refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}


//OUTDENT

NS_IMETHODIMP
OutdentCommand::IsCommandEnabled(const char* aCommandName,
                                 nsISupports* refCon,
                                 bool* outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}

NS_IMETHODIMP
OutdentCommand::DoCommand(const char* aCommandName, nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  return htmlEditor->Indent(NS_LITERAL_STRING("outdent"));
}

NS_IMETHODIMP
OutdentCommand::DoCommandParams(const char* aCommandName,
                                nsICommandParams* aParams,
                                nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
OutdentCommand::GetCommandStateParams(const char* aCommandName,
                                      nsICommandParams* aParams,
                                      nsISupports* refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

MultiStateCommandBase::MultiStateCommandBase()
  : HTMLEditorCommandBase()
{
}

MultiStateCommandBase::~MultiStateCommandBase()
{
}

NS_IMETHODIMP
MultiStateCommandBase::IsCommandEnabled(const char* aCommandName,
                                        nsISupports* refCon,
                                        bool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  // should be disabled sometimes, like if the current selection is an image
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}

NS_IMETHODIMP
MultiStateCommandBase::DoCommand(const char* aCommandName, nsISupports* refCon)
{
#ifdef DEBUG
  printf("who is calling MultiStateCommandBase::DoCommand \
          (no implementation)? %s\n", aCommandName);
#endif

  return NS_OK;
}

NS_IMETHODIMP
MultiStateCommandBase::DoCommandParams(const char* aCommandName,
                                       nsICommandParams* aParams,
                                       nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString attribute;
  if (aParams) {
    nsAutoCString asciiAttribute;
    nsresult rv = aParams->GetCStringValue(STATE_ATTRIBUTE, asciiAttribute);
    if (NS_SUCCEEDED(rv)) {
      CopyASCIItoUTF16(asciiAttribute, attribute);
    } else {
      aParams->GetStringValue(STATE_ATTRIBUTE, attribute);
    }
  }
  return SetState(htmlEditor, attribute);
}

NS_IMETHODIMP
MultiStateCommandBase::GetCommandStateParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return GetCurrentState(htmlEditor, aParams);
}

ParagraphStateCommand::ParagraphStateCommand()
  : MultiStateCommandBase()
{
}

nsresult
ParagraphStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                       nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetParagraphState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tOutStateString;
    LossyCopyUTF16toASCII(outStateString, tOutStateString);
    aParams->SetBooleanValue(STATE_MIXED,outMixed);
    aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString);
  }
  return rv;
}

nsresult
ParagraphStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                const nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->SetParagraphFormat(newState);
}

FontFaceStateCommand::FontFaceStateCommand()
  : MultiStateCommandBase()
{
}

nsresult
FontFaceStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                      nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString outStateString;
  bool outMixed;
  nsresult rv = aHTMLEditor->GetFontFaceState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv)) {
    aParams->SetBooleanValue(STATE_MIXED,outMixed);
    aParams->SetCStringValue(STATE_ATTRIBUTE,
                             NS_ConvertUTF16toUTF8(outStateString));
  }
  return rv;
}

nsresult
FontFaceStateCommand::SetState(HTMLEditor* aHTMLEditor,
                               const nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (newState.EqualsLiteral("tt")) {
    // The old "teletype" attribute
    nsresult rv = aHTMLEditor->SetInlineProperty(nsGkAtoms::tt, nullptr,
                                                 EmptyString());
    NS_ENSURE_SUCCESS(rv, rv);
    // Clear existing font face
    return aHTMLEditor->RemoveInlineProperty(nsGkAtoms::font, nsGkAtoms::face);
  }

  // Remove any existing TT nodes
  nsresult rv = aHTMLEditor->RemoveInlineProperty(nsGkAtoms::tt, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  if (newState.IsEmpty() || newState.EqualsLiteral("normal")) {
    return aHTMLEditor->RemoveInlineProperty(nsGkAtoms::font, nsGkAtoms::face);
  }

  return aHTMLEditor->SetInlineProperty(nsGkAtoms::font, nsGkAtoms::face,
                                        newState);
}

FontSizeStateCommand::FontSizeStateCommand()
  : MultiStateCommandBase()
{
}

nsresult
FontSizeStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                      nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString outStateString;
  bool firstHas, anyHas, allHas;
  nsresult rv = aHTMLEditor->GetInlinePropertyWithAttrValue(
                               nsGkAtoms::font,
                               nsGkAtoms::size,
                               EmptyString(),
                               &firstHas, &anyHas, &allHas,
                               outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  aParams->SetBooleanValue(STATE_MIXED, anyHas && !allHas);
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString);
  aParams->SetBooleanValue(STATE_ENABLED, true);

  return rv;
}


// acceptable values for "newState" are:
//   -2
//   -1
//    0
//   +1
//   +2
//   +3
//   medium
//   normal
nsresult
FontSizeStateCommand::SetState(HTMLEditor* aHTMLEditor,
                               const nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!newState.IsEmpty() &&
      !newState.EqualsLiteral("normal") &&
      !newState.EqualsLiteral("medium")) {
    return aHTMLEditor->SetInlineProperty(nsGkAtoms::font,
                                          nsGkAtoms::size, newState);
  }

  // remove any existing font size, big or small
  nsresult rv = aHTMLEditor->RemoveInlineProperty(nsGkAtoms::font,
                                                  nsGkAtoms::size);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aHTMLEditor->RemoveInlineProperty(nsGkAtoms::big, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return aHTMLEditor->RemoveInlineProperty(nsGkAtoms::small, nullptr);
}

FontColorStateCommand::FontColorStateCommand()
  : MultiStateCommandBase()
{
}

nsresult
FontColorStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                       nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetFontColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  aParams->SetBooleanValue(STATE_MIXED, outMixed);
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult
FontColorStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                const nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (newState.IsEmpty() || newState.EqualsLiteral("normal")) {
    return aHTMLEditor->RemoveInlineProperty(nsGkAtoms::font,
                                             nsGkAtoms::color);
  }

  return aHTMLEditor->SetInlineProperty(nsGkAtoms::font, nsGkAtoms::color,
                                        newState);
}

HighlightColorStateCommand::HighlightColorStateCommand()
  : MultiStateCommandBase()
{
}

nsresult
HighlightColorStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                            nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetHighlightColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  aParams->SetBooleanValue(STATE_MIXED, outMixed);
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult
HighlightColorStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                     const nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (newState.IsEmpty() || newState.EqualsLiteral("normal")) {
    return aHTMLEditor->RemoveInlineProperty(nsGkAtoms::font,
                                             nsGkAtoms::bgcolor);
  }

  return aHTMLEditor->SetInlineProperty(nsGkAtoms::font,
                                        nsGkAtoms::bgcolor,
                                        newState);
}

NS_IMETHODIMP
HighlightColorStateCommand::IsCommandEnabled(const char* aCommandName,
                                             nsISupports* refCon,
                                             bool* outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}

BackgroundColorStateCommand::BackgroundColorStateCommand()
  : MultiStateCommandBase()
{
}

nsresult
BackgroundColorStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                             nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetBackgroundColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  aParams->SetBooleanValue(STATE_MIXED, outMixed);
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult
BackgroundColorStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                      const nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->SetBackgroundColor(newState);
}

AlignCommand::AlignCommand()
  : MultiStateCommandBase()
{
}

nsresult
AlignCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                              nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsIHTMLEditor::EAlignment firstAlign;
  bool outMixed;
  nsresult rv = aHTMLEditor->GetAlignment(&outMixed, &firstAlign);

  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString outStateString;
  switch (firstAlign) {
    default:
    case nsIHTMLEditor::eLeft:
      outStateString.AssignLiteral("left");
      break;

    case nsIHTMLEditor::eCenter:
      outStateString.AssignLiteral("center");
      break;

    case nsIHTMLEditor::eRight:
      outStateString.AssignLiteral("right");
      break;

    case nsIHTMLEditor::eJustify:
      outStateString.AssignLiteral("justify");
      break;
  }
  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  aParams->SetBooleanValue(STATE_MIXED,outMixed);
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult
AlignCommand::SetState(HTMLEditor* aHTMLEditor,
                       const nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->Align(newState);
}

AbsolutePositioningCommand::AbsolutePositioningCommand()
  : StateUpdatingCommandBase(nsGkAtoms::_empty)
{
}

NS_IMETHODIMP
AbsolutePositioningCommand::IsCommandEnabled(const char* aCommandName,
                                             nsISupports* aCommandRefCon,
                                             bool* outCmdEnabled)
{
  *outCmdEnabled = false;

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  if (!htmlEditor->IsSelectionEditable()) {
    return NS_OK;
  }
  *outCmdEnabled = htmlEditor->AbsolutePositioningEnabled();
  return NS_OK;
}

nsresult
AbsolutePositioningCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                            nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool isEnabled = aHTMLEditor->AbsolutePositioningEnabled();
  if (!isEnabled) {
    aParams->SetBooleanValue(STATE_MIXED,false);
    aParams->SetCStringValue(STATE_ATTRIBUTE, EmptyCString());
    return NS_OK;
  }

  RefPtr<Element> container =
    aHTMLEditor->GetAbsolutelyPositionedSelectionContainer();
  aParams->SetBooleanValue(STATE_MIXED,  false);
  aParams->SetCStringValue(STATE_ATTRIBUTE,
                           container ? NS_LITERAL_CSTRING("absolute") :
                                       EmptyCString());
  return NS_OK;
}

nsresult
AbsolutePositioningCommand::ToggleState(HTMLEditor* aHTMLEditor)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Element> container =
    aHTMLEditor->GetAbsolutelyPositionedSelectionContainer();
  return aHTMLEditor->SetSelectionToAbsoluteOrStatic(!container);
}


NS_IMETHODIMP
DecreaseZIndexCommand::IsCommandEnabled(const char* aCommandName,
                                        nsISupports* refCon,
                                        bool* outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  *outCmdEnabled = htmlEditor->AbsolutePositioningEnabled();
  if (!(*outCmdEnabled))
    return NS_OK;

  RefPtr<Element> positionedElement = htmlEditor->GetPositionedElement();
  *outCmdEnabled = false;
  if (!positionedElement) {
    return NS_OK;
  }

  int32_t z = htmlEditor->GetZIndex(*positionedElement);
  *outCmdEnabled = (z > 0);
  return NS_OK;
}

NS_IMETHODIMP
DecreaseZIndexCommand::DoCommand(const char* aCommandName,
                                 nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return htmlEditor->AddZIndex(-1);
}

NS_IMETHODIMP
DecreaseZIndexCommand::DoCommandParams(const char* aCommandName,
                                       nsICommandParams* aParams,
                                       nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
DecreaseZIndexCommand::GetCommandStateParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled = false;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->SetBooleanValue(STATE_ENABLED, enabled);
}

NS_IMETHODIMP
IncreaseZIndexCommand::IsCommandEnabled(const char* aCommandName,
                                        nsISupports* refCon,
                                        bool* outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  *outCmdEnabled = htmlEditor->AbsolutePositioningEnabled();
  if (!(*outCmdEnabled))
    return NS_OK;

  Element* positionedElement = htmlEditor->GetPositionedElement();
  *outCmdEnabled = (nullptr != positionedElement);
  return NS_OK;
}

NS_IMETHODIMP
IncreaseZIndexCommand::DoCommand(const char* aCommandName,
                                 nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return htmlEditor->AddZIndex(1);
}

NS_IMETHODIMP
IncreaseZIndexCommand::DoCommandParams(const char* aCommandName,
                                       nsICommandParams* aParams,
                                       nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
IncreaseZIndexCommand::GetCommandStateParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled = false;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->SetBooleanValue(STATE_ENABLED, enabled);
}

NS_IMETHODIMP
RemoveStylesCommand::IsCommandEnabled(const char* aCommandName,
                                      nsISupports* refCon,
                                      bool* outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  // test if we have any styles?
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}

NS_IMETHODIMP
RemoveStylesCommand::DoCommand(const char* aCommandName,
                               nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  return htmlEditor->RemoveAllInlineProperties();
}

NS_IMETHODIMP
RemoveStylesCommand::DoCommandParams(const char* aCommandName,
                                     nsICommandParams* aParams,
                                     nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
RemoveStylesCommand::GetCommandStateParams(const char* aCommandName,
                                           nsICommandParams* aParams,
                                           nsISupports* refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

NS_IMETHODIMP
IncreaseFontSizeCommand::IsCommandEnabled(const char* aCommandName,
                                          nsISupports* refCon,
                                          bool* outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  // test if we are at max size?
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}


NS_IMETHODIMP
IncreaseFontSizeCommand::DoCommand(const char* aCommandName,
                                   nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  return htmlEditor->IncreaseFontSize();
}

NS_IMETHODIMP
IncreaseFontSizeCommand::DoCommandParams(const char* aCommandName,
                                         nsICommandParams* aParams,
                                         nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
IncreaseFontSizeCommand::GetCommandStateParams(const char* aCommandName,
                                               nsICommandParams* aParams,
                                               nsISupports* refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

NS_IMETHODIMP
DecreaseFontSizeCommand::IsCommandEnabled(const char* aCommandName,
                                          nsISupports* refCon,
                                          bool* outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  // test if we are at min size?
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}

NS_IMETHODIMP
DecreaseFontSizeCommand::DoCommand(const char* aCommandName,
                                   nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  return htmlEditor->DecreaseFontSize();
}

NS_IMETHODIMP
DecreaseFontSizeCommand::DoCommandParams(const char* aCommandName,
                                         nsICommandParams* aParams,
                                         nsISupports* refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
DecreaseFontSizeCommand::GetCommandStateParams(const char* aCommandName,
                                               nsICommandParams* aParams,
                                               nsISupports* refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

NS_IMETHODIMP
InsertHTMLCommand::IsCommandEnabled(const char* aCommandName,
                                    nsISupports* refCon,
                                    bool* outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}

NS_IMETHODIMP
InsertHTMLCommand::DoCommand(const char* aCommandName, nsISupports* refCon)
{
  // If nsInsertHTMLCommand is called with no parameters, it was probably called with
  // an empty string parameter ''. In this case, it should act the same as the delete command
  NS_ENSURE_ARG_POINTER(refCon);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString html;
  return htmlEditor->InsertHTML(html);
}

NS_IMETHODIMP
InsertHTMLCommand::DoCommandParams(const char* aCommandName,
                                   nsICommandParams* aParams,
                                   nsISupports* refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  // Get HTML source string to insert from command params
  nsAutoString html;
  nsresult rv = aParams->GetStringValue(STATE_DATA, html);
  NS_ENSURE_SUCCESS(rv, rv);

  return htmlEditor->InsertHTML(html);
}

NS_IMETHODIMP
InsertHTMLCommand::GetCommandStateParams(const char *aCommandName,
                                         nsICommandParams *aParams,
                                         nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED, outCmdEnabled);
}

InsertTagCommand::InsertTagCommand(nsAtom* aTagName)
  : HTMLEditorCommandBase()
  , mTagName(aTagName)
{
  MOZ_ASSERT(mTagName);
}

InsertTagCommand::~InsertTagCommand()
{
}

NS_IMETHODIMP
InsertTagCommand::IsCommandEnabled(const char* aCommandName,
                                   nsISupports* refCon,
                                   bool* outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  mozilla::EditorBase* editorBase = editor->AsEditorBase();
  MOZ_ASSERT(editorBase);
  *outCmdEnabled = editorBase->IsSelectionEditable();
  return NS_OK;
}

// corresponding STATE_ATTRIBUTE is: src (img) and href (a)
NS_IMETHODIMP
InsertTagCommand::DoCommand(const char* aCmdName, nsISupports* refCon)
{
  NS_ENSURE_TRUE(mTagName == nsGkAtoms::hr, NS_ERROR_NOT_IMPLEMENTED);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> newElement;
  nsresult rv = htmlEditor->CreateElementWithDefaults(
    nsDependentAtomString(mTagName), getter_AddRefs(newElement));
  NS_ENSURE_SUCCESS(rv, rv);

  return htmlEditor->InsertElementAtSelection(newElement, true);
}

NS_IMETHODIMP
InsertTagCommand::DoCommandParams(const char *aCommandName,
                                  nsICommandParams *aParams,
                                  nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(refCon);

  // inserting an hr shouldn't have an parameters, just call DoCommand for that
  if (mTagName == nsGkAtoms::hr) {
    return DoCommand(aCommandName, refCon);
  }

  NS_ENSURE_ARG_POINTER(aParams);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  // do we have an href to use for creating link?
  nsAutoCString asciiAttribute;
  nsresult rv = aParams->GetCStringValue(STATE_ATTRIBUTE, asciiAttribute);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsAutoString attribute;
  CopyASCIItoUTF16(asciiAttribute, attribute);

  if (attribute.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // filter out tags we don't know how to insert
  nsAutoString attributeType;
  if (mTagName == nsGkAtoms::a) {
    attributeType.AssignLiteral("href");
  } else if (mTagName == nsGkAtoms::img) {
    attributeType.AssignLiteral("src");
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<Element> elem;
  rv = htmlEditor->CreateElementWithDefaults(nsDependentAtomString(mTagName),
                                             getter_AddRefs(elem));
  NS_ENSURE_SUCCESS(rv, rv);

  ErrorResult err;
  elem->SetAttribute(attributeType, attribute, err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  // do actual insertion
  if (mTagName == nsGkAtoms::a) {
    return htmlEditor->InsertLinkAroundSelection(elem);
  }
  return htmlEditor->InsertElementAtSelection(elem, true);
}

NS_IMETHODIMP
InsertTagCommand::GetCommandStateParams(const char *aCommandName,
                                        nsICommandParams *aParams,
                                        nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED, outCmdEnabled);
}


/****************************/
//HELPER METHODS
/****************************/

static nsresult
GetListState(HTMLEditor* aHTMLEditor, bool* aMixed, nsAString& aLocalName)
{
  MOZ_ASSERT(aHTMLEditor);
  MOZ_ASSERT(aMixed);

  *aMixed = false;
  aLocalName.Truncate();

  bool bOL, bUL, bDL;
  nsresult rv = aHTMLEditor->GetListState(aMixed, &bOL, &bUL, &bDL);
  NS_ENSURE_SUCCESS(rv, rv);

  if (*aMixed) {
    return NS_OK;
  }

  if (bOL) {
    aLocalName.AssignLiteral("ol");
  } else if (bUL) {
    aLocalName.AssignLiteral("ul");
  } else if (bDL) {
    aLocalName.AssignLiteral("dl");
  }
  return NS_OK;
}

} // namespace mozilla
