/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>  // for printf

#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc
#include "mozilla/EditorBase.h"  // for EditorBase
#include "mozilla/ErrorResult.h"
#include "mozilla/HTMLEditor.h"  // for HTMLEditor
#include "mozilla/HTMLEditorCommands.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsCommandParams.h"          // for nsCommandParams, etc
#include "nsCOMPtr.h"                 // for nsCOMPtr, do_QueryInterface, etc
#include "nsComponentManagerUtils.h"  // for do_CreateInstance
#include "nsDebug.h"                  // for NS_ENSURE_TRUE, etc
#include "nsError.h"                  // for NS_OK, NS_ERROR_FAILURE, etc
#include "nsGkAtoms.h"                // for nsGkAtoms, nsGkAtoms::font, etc
#include "nsAtom.h"                   // for nsAtom, etc
#include "nsIClipboard.h"             // for nsIClipboard, etc
#include "nsID.h"
#include "nsIEditor.h"        // for nsIEditor
#include "nsIHTMLEditor.h"    // for nsIHTMLEditor, etc
#include "nsLiteralString.h"  // for NS_LITERAL_STRING
#include "nsReadableUtils.h"  // for EmptyString
#include "nsString.h"         // for nsAutoString, nsString, etc
#include "nsStringFwd.h"      // for nsString

class nsISupports;

namespace mozilla {
using dom::Element;

// prototype
static nsresult GetListState(HTMLEditor* aHTMLEditor, bool* aMixed,
                             nsAString& aLocalName);

// defines
#define STATE_ENABLED "state_enabled"
#define STATE_ALL "state_all"
#define STATE_ANY "state_any"
#define STATE_MIXED "state_mixed"
#define STATE_BEGIN "state_begin"
#define STATE_END "state_end"
#define STATE_ATTRIBUTE "state_attribute"
#define STATE_DATA "state_data"

HTMLEditorCommandBase::HTMLEditorCommandBase() {}

NS_IMPL_ISUPPORTS(HTMLEditorCommandBase, nsIControllerCommand)

StateUpdatingCommandBase::StateUpdatingCommandBase(nsAtom* aTagName)
    : HTMLEditorCommandBase(), mTagName(aTagName) {
  MOZ_ASSERT(mTagName);
}

StateUpdatingCommandBase::~StateUpdatingCommandBase() {}

NS_IMETHODIMP
StateUpdatingCommandBase::IsCommandEnabled(const char* aCommandName,
                                           nsISupports* refCon,
                                           bool* outCmdEnabled) {
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
                                    nsISupports* refCon) {
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
                                          nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
StateUpdatingCommandBase::GetCommandStateParams(const char* aCommandName,
                                                nsICommandParams* aParams,
                                                nsISupports* refCon) {
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
                                           bool* outCmdEnabled) {
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
                                    nsISupports* refCon) {
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  // Known live because we hold a ref above in "editor"
  return MOZ_KnownLive(htmlEditor)
      ->PasteNoFormatting(nsIClipboard::kGlobalClipboard);
}

NS_IMETHODIMP
PasteNoFormattingCommand::DoCommandParams(const char* aCommandName,
                                          nsICommandParams* aParams,
                                          nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
PasteNoFormattingCommand::GetCommandStateParams(const char* aCommandName,
                                                nsICommandParams* aParams,
                                                nsISupports* refCon) {
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled = false;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, enabled);
}

StyleUpdatingCommand::StyleUpdatingCommand(nsAtom* aTagName)
    : StateUpdatingCommandBase(aTagName) {}

nsresult StyleUpdatingCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                               nsICommandParams* aParams) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool firstOfSelectionHasProp = false;
  bool anyOfSelectionHasProp = false;
  bool allOfSelectionHasProp = false;

  nsresult rv = aHTMLEditor->GetInlineProperty(
      mTagName, nullptr, EmptyString(), &firstOfSelectionHasProp,
      &anyOfSelectionHasProp, &allOfSelectionHasProp);

  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_ENABLED, NS_SUCCEEDED(rv));
  params->SetBool(STATE_ALL, allOfSelectionHasProp);
  params->SetBool(STATE_ANY, anyOfSelectionHasProp);
  params->SetBool(STATE_MIXED, anyOfSelectionHasProp && !allOfSelectionHasProp);
  params->SetBool(STATE_BEGIN, firstOfSelectionHasProp);
  params->SetBool(STATE_END, allOfSelectionHasProp);  // not completely accurate
  return NS_OK;
}

nsresult StyleUpdatingCommand::ToggleState(HTMLEditor* aHTMLEditor) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<nsCommandParams> params = new nsCommandParams();

  // tags "href" and "name" are special cases in the core editor
  // they are used to remove named anchor/link and shouldn't be used for
  // insertion
  bool doTagRemoval;
  if (mTagName == nsGkAtoms::href || mTagName == nsGkAtoms::name) {
    doTagRemoval = true;
  } else {
    // check current selection; set doTagRemoval if formatting should be removed
    nsresult rv = GetCurrentState(aHTMLEditor, params);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    ErrorResult error;
    doTagRemoval = params->GetBool(STATE_ALL, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  if (doTagRemoval) {
    // Also remove equivalent properties (bug 317093)
    // XXX Why don't we make the following two transactions as an atomic
    //     transaction?  If the element is <b>, <i> or <strike>, user
    //     needs to undo twice.
    if (mTagName == nsGkAtoms::b) {
      nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(
          *nsGkAtoms::strong, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (mTagName == nsGkAtoms::i) {
      nsresult rv =
          aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::em, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (mTagName == nsGkAtoms::strike) {
      nsresult rv =
          aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::s, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(*mTagName, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  nsresult rv =
      aHTMLEditor->SetInlinePropertyAsAction(*mTagName, nullptr, EmptyString());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

ListCommand::ListCommand(nsAtom* aTagName)
    : StateUpdatingCommandBase(aTagName) {}

nsresult ListCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                      nsICommandParams* aParams) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool bMixed;
  nsAutoString localName;
  nsresult rv = GetListState(aHTMLEditor, &bMixed, localName);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList = mTagName->Equals(localName);
  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_ALL, !bMixed && inList);
  params->SetBool(STATE_MIXED, bMixed);
  params->SetBool(STATE_ENABLED, true);
  return NS_OK;
}

nsresult ListCommand::ToggleState(HTMLEditor* aHTMLEditor) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  RefPtr<nsCommandParams> params = new nsCommandParams();
  rv = GetCurrentState(aHTMLEditor, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ErrorResult error;
  bool inList = params->GetBool(STATE_ALL, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  nsDependentAtomString listType(mTagName);
  if (inList) {
    rv = aHTMLEditor->RemoveList(listType);
  } else {
    rv = aHTMLEditor->MakeOrChangeList(listType, false, EmptyString());
  }

  return rv;
}

ListItemCommand::ListItemCommand(nsAtom* aTagName)
    : StateUpdatingCommandBase(aTagName) {}

nsresult ListItemCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                          nsICommandParams* aParams) {
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

  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_ALL, !bMixed && inList);
  params->SetBool(STATE_MIXED, bMixed);

  return NS_OK;
}

nsresult ListItemCommand::ToggleState(HTMLEditor* aHTMLEditor) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Need to use mTagName????
  RefPtr<nsCommandParams> params = new nsCommandParams();
  GetCurrentState(aHTMLEditor, params);
  ErrorResult error;
  bool inList = params->GetBool(STATE_ALL, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  if (inList) {
    // To remove a list, first get what kind of list we're in
    bool bMixed;
    nsAutoString localName;
    nsresult rv = GetListState(aHTMLEditor, &bMixed, localName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (localName.IsEmpty() || bMixed) {
      return NS_OK;
    }
    return aHTMLEditor->RemoveList(localName);
  }

  // Set to the requested paragraph type
  // XXX Note: This actually doesn't work for "LI",
  //    but we currently don't use this for non DL lists anyway.
  // Problem: won't this replace any current block paragraph style?
  return aHTMLEditor->SetParagraphFormat(nsDependentAtomString(mTagName));
}

NS_IMETHODIMP
RemoveListCommand::IsCommandEnabled(const char* aCommandName,
                                    nsISupports* refCon, bool* outCmdEnabled) {
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
RemoveListCommand::DoCommand(const char* aCommandName, nsISupports* refCon) {
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
                                   nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
RemoveListCommand::GetCommandStateParams(const char* aCommandName,
                                         nsICommandParams* aParams,
                                         nsISupports* refCon) {
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, outCmdEnabled);
}

NS_IMETHODIMP
IndentCommand::IsCommandEnabled(const char* aCommandName, nsISupports* refCon,
                                bool* outCmdEnabled) {
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
IndentCommand::DoCommand(const char* aCommandName, nsISupports* refCon) {
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  nsresult rv = htmlEditor->IndentAsAction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
IndentCommand::DoCommandParams(const char* aCommandName,
                               nsICommandParams* aParams, nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
IndentCommand::GetCommandStateParams(const char* aCommandName,
                                     nsICommandParams* aParams,
                                     nsISupports* refCon) {
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, outCmdEnabled);
}

// OUTDENT

NS_IMETHODIMP
OutdentCommand::IsCommandEnabled(const char* aCommandName, nsISupports* refCon,
                                 bool* outCmdEnabled) {
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
OutdentCommand::DoCommand(const char* aCommandName, nsISupports* refCon) {
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  nsresult rv = htmlEditor->OutdentAsAction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
OutdentCommand::DoCommandParams(const char* aCommandName,
                                nsICommandParams* aParams,
                                nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
OutdentCommand::GetCommandStateParams(const char* aCommandName,
                                      nsICommandParams* aParams,
                                      nsISupports* refCon) {
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, outCmdEnabled);
}

MultiStateCommandBase::MultiStateCommandBase() : HTMLEditorCommandBase() {}

MultiStateCommandBase::~MultiStateCommandBase() {}

NS_IMETHODIMP
MultiStateCommandBase::IsCommandEnabled(const char* aCommandName,
                                        nsISupports* refCon,
                                        bool* outCmdEnabled) {
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
MultiStateCommandBase::DoCommand(const char* aCommandName,
                                 nsISupports* refCon) {
#ifdef DEBUG
  printf(
      "who is calling MultiStateCommandBase::DoCommand \
          (no implementation)? %s\n",
      aCommandName);
#endif

  return NS_OK;
}

NS_IMETHODIMP
MultiStateCommandBase::DoCommandParams(const char* aCommandName,
                                       nsICommandParams* aParams,
                                       nsISupports* refCon) {
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
    nsCommandParams* params = aParams->AsCommandParams();
    nsAutoCString asciiAttribute;
    nsresult rv = params->GetCString(STATE_ATTRIBUTE, asciiAttribute);
    if (NS_SUCCEEDED(rv)) {
      CopyASCIItoUTF16(asciiAttribute, attribute);
    } else {
      params->GetString(STATE_ATTRIBUTE, attribute);
    }
  }
  return SetState(htmlEditor, attribute);
}

NS_IMETHODIMP
MultiStateCommandBase::GetCommandStateParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* refCon) {
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

ParagraphStateCommand::ParagraphStateCommand() : MultiStateCommandBase() {}

nsresult ParagraphStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                                nsICommandParams* aParams) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetParagraphState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tOutStateString;
    LossyCopyUTF16toASCII(outStateString, tOutStateString);
    nsCommandParams* params = aParams->AsCommandParams();
    params->SetBool(STATE_MIXED, outMixed);
    params->SetCString(STATE_ATTRIBUTE, tOutStateString);
  }
  return rv;
}

nsresult ParagraphStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                         const nsString& newState) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->SetParagraphFormat(newState);
}

FontFaceStateCommand::FontFaceStateCommand() : MultiStateCommandBase() {}

nsresult FontFaceStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                               nsICommandParams* aParams) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString outStateString;
  bool outMixed;
  nsresult rv = aHTMLEditor->GetFontFaceState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv)) {
    nsCommandParams* params = aParams->AsCommandParams();
    params->SetBool(STATE_MIXED, outMixed);
    params->SetCString(STATE_ATTRIBUTE, NS_ConvertUTF16toUTF8(outStateString));
  }
  return rv;
}

nsresult FontFaceStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                        const nsString& newState) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (newState.EqualsLiteral("tt")) {
    // The old "teletype" attribute
    nsresult rv = aHTMLEditor->SetInlinePropertyAsAction(
        *nsGkAtoms::tt, nullptr, EmptyString());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Clear existing font face
    rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::font,
                                                   nsGkAtoms::face);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // Remove any existing TT nodes
  nsresult rv =
      aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::tt, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (newState.IsEmpty() || newState.EqualsLiteral("normal")) {
    rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::font,
                                                   nsGkAtoms::face);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  rv = aHTMLEditor->SetInlinePropertyAsAction(*nsGkAtoms::font, nsGkAtoms::face,
                                              newState);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

FontSizeStateCommand::FontSizeStateCommand() : MultiStateCommandBase() {}

nsresult FontSizeStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                               nsICommandParams* aParams) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString outStateString;
  bool firstHas, anyHas, allHas;
  nsresult rv = aHTMLEditor->GetInlinePropertyWithAttrValue(
      nsGkAtoms::font, nsGkAtoms::size, EmptyString(), &firstHas, &anyHas,
      &allHas, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_MIXED, anyHas && !allHas);
  params->SetCString(STATE_ATTRIBUTE, tOutStateString);
  params->SetBool(STATE_ENABLED, true);

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
nsresult FontSizeStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                        const nsString& newState) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!newState.IsEmpty() && !newState.EqualsLiteral("normal") &&
      !newState.EqualsLiteral("medium")) {
    nsresult rv = aHTMLEditor->SetInlinePropertyAsAction(
        *nsGkAtoms::font, nsGkAtoms::size, newState);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // remove any existing font size, big or small
  nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::font,
                                                          nsGkAtoms::size);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::big, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::small, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

FontColorStateCommand::FontColorStateCommand() : MultiStateCommandBase() {}

nsresult FontColorStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                                nsICommandParams* aParams) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetFontColorState(&outMixed, outStateString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_MIXED, outMixed);
  params->SetCString(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult FontColorStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                         const nsString& newState) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (newState.IsEmpty() || newState.EqualsLiteral("normal")) {
    nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::font,
                                                            nsGkAtoms::color);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  nsresult rv = aHTMLEditor->SetInlinePropertyAsAction(
      *nsGkAtoms::font, nsGkAtoms::color, newState);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

HighlightColorStateCommand::HighlightColorStateCommand()
    : MultiStateCommandBase() {}

nsresult HighlightColorStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsICommandParams* aParams) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetHighlightColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_MIXED, outMixed);
  params->SetCString(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult HighlightColorStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                              const nsString& newState) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (newState.IsEmpty() || newState.EqualsLiteral("normal")) {
    nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::font,
                                                            nsGkAtoms::bgcolor);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  nsresult rv = aHTMLEditor->SetInlinePropertyAsAction(
      *nsGkAtoms::font, nsGkAtoms::bgcolor, newState);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HighlightColorStateCommand::IsCommandEnabled(const char* aCommandName,
                                             nsISupports* refCon,
                                             bool* outCmdEnabled) {
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
    : MultiStateCommandBase() {}

nsresult BackgroundColorStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsICommandParams* aParams) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetBackgroundColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_MIXED, outMixed);
  params->SetCString(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult BackgroundColorStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                               const nsString& newState) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->SetBackgroundColor(newState);
}

AlignCommand::AlignCommand() : MultiStateCommandBase() {}

nsresult AlignCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                       nsICommandParams* aParams) {
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
  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_MIXED, outMixed);
  params->SetCString(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult AlignCommand::SetState(HTMLEditor* aHTMLEditor,
                                const nsString& newState) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->Align(newState);
}

AbsolutePositioningCommand::AbsolutePositioningCommand()
    : StateUpdatingCommandBase(nsGkAtoms::_empty) {}

NS_IMETHODIMP
AbsolutePositioningCommand::IsCommandEnabled(const char* aCommandName,
                                             nsISupports* aCommandRefCon,
                                             bool* aOutCmdEnabled) {
  *aOutCmdEnabled = false;

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
  *aOutCmdEnabled = htmlEditor->IsAbsolutePositionEditorEnabled();
  return NS_OK;
}

nsresult AbsolutePositioningCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsICommandParams* aParams) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCommandParams* params = aParams->AsCommandParams();
  if (!aHTMLEditor->IsAbsolutePositionEditorEnabled()) {
    params->SetBool(STATE_MIXED, false);
    params->SetCString(STATE_ATTRIBUTE, EmptyCString());
    return NS_OK;
  }

  RefPtr<Element> container =
      aHTMLEditor->GetAbsolutelyPositionedSelectionContainer();
  params->SetBool(STATE_MIXED, false);
  params->SetCString(STATE_ATTRIBUTE, container ? NS_LITERAL_CSTRING("absolute")
                                                : EmptyCString());
  return NS_OK;
}

nsresult AbsolutePositioningCommand::ToggleState(HTMLEditor* aHTMLEditor) {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Element> container =
      aHTMLEditor->GetAbsolutelyPositionedSelectionContainer();
  return aHTMLEditor->SetSelectionToAbsoluteOrStatic(!container);
}

NS_IMETHODIMP
DecreaseZIndexCommand::IsCommandEnabled(const char* aCommandName,
                                        nsISupports* aRefCon,
                                        bool* aOutCmdEnabled) {
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aRefCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  if (!htmlEditor->IsAbsolutePositionEditorEnabled()) {
    *aOutCmdEnabled = false;
    return NS_OK;
  }

  RefPtr<Element> positionedElement = htmlEditor->GetPositionedElement();
  if (!positionedElement) {
    *aOutCmdEnabled = false;
    return NS_OK;
  }

  int32_t z = htmlEditor->GetZIndex(*positionedElement);
  *aOutCmdEnabled = (z > 0);
  return NS_OK;
}

NS_IMETHODIMP
DecreaseZIndexCommand::DoCommand(const char* aCommandName,
                                 nsISupports* refCon) {
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
                                       nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
DecreaseZIndexCommand::GetCommandStateParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* refCon) {
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled = false;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, enabled);
}

NS_IMETHODIMP
IncreaseZIndexCommand::IsCommandEnabled(const char* aCommandName,
                                        nsISupports* aRefCon,
                                        bool* aOutCmdEnabled) {
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aRefCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  if (!htmlEditor->IsAbsolutePositionEditorEnabled()) {
    *aOutCmdEnabled = false;
    return NS_OK;
  }

  *aOutCmdEnabled = !!htmlEditor->GetPositionedElement();
  return NS_OK;
}

NS_IMETHODIMP
IncreaseZIndexCommand::DoCommand(const char* aCommandName,
                                 nsISupports* refCon) {
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
                                       nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
IncreaseZIndexCommand::GetCommandStateParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* refCon) {
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled = false;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, enabled);
}

NS_IMETHODIMP
RemoveStylesCommand::IsCommandEnabled(const char* aCommandName,
                                      nsISupports* refCon,
                                      bool* outCmdEnabled) {
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
RemoveStylesCommand::DoCommand(const char* aCommandName, nsISupports* refCon) {
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
                                     nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
RemoveStylesCommand::GetCommandStateParams(const char* aCommandName,
                                           nsICommandParams* aParams,
                                           nsISupports* refCon) {
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, outCmdEnabled);
}

NS_IMETHODIMP
IncreaseFontSizeCommand::IsCommandEnabled(const char* aCommandName,
                                          nsISupports* refCon,
                                          bool* outCmdEnabled) {
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
                                   nsISupports* refCon) {
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
                                         nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
IncreaseFontSizeCommand::GetCommandStateParams(const char* aCommandName,
                                               nsICommandParams* aParams,
                                               nsISupports* refCon) {
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, outCmdEnabled);
}

NS_IMETHODIMP
DecreaseFontSizeCommand::IsCommandEnabled(const char* aCommandName,
                                          nsISupports* refCon,
                                          bool* outCmdEnabled) {
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
                                   nsISupports* refCon) {
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
                                         nsISupports* refCon) {
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
DecreaseFontSizeCommand::GetCommandStateParams(const char* aCommandName,
                                               nsICommandParams* aParams,
                                               nsISupports* refCon) {
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, outCmdEnabled);
}

NS_IMETHODIMP
InsertHTMLCommand::IsCommandEnabled(const char* aCommandName,
                                    nsISupports* refCon, bool* outCmdEnabled) {
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
InsertHTMLCommand::DoCommand(const char* aCommandName, nsISupports* refCon) {
  // If nsInsertHTMLCommand is called with no parameters, it was probably called
  // with an empty string parameter ''. In this case, it should act the same as
  // the delete command
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
                                   nsISupports* refCon) {
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
  nsresult rv = aParams->AsCommandParams()->GetString(STATE_DATA, html);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return htmlEditor->InsertHTML(html);
}

NS_IMETHODIMP
InsertHTMLCommand::GetCommandStateParams(const char* aCommandName,
                                         nsICommandParams* aParams,
                                         nsISupports* refCon) {
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, outCmdEnabled);
}

InsertTagCommand::InsertTagCommand(nsAtom* aTagName)
    : HTMLEditorCommandBase(), mTagName(aTagName) {
  MOZ_ASSERT(mTagName);
}

InsertTagCommand::~InsertTagCommand() {}

NS_IMETHODIMP
InsertTagCommand::IsCommandEnabled(const char* aCommandName,
                                   nsISupports* refCon, bool* outCmdEnabled) {
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
InsertTagCommand::DoCommand(const char* aCmdName, nsISupports* refCon) {
  NS_ENSURE_TRUE(mTagName == nsGkAtoms::hr, NS_ERROR_NOT_IMPLEMENTED);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> newElement = htmlEditor->CreateElementWithDefaults(*mTagName);
  if (NS_WARN_IF(!newElement)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = htmlEditor->InsertElementAtSelection(newElement, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
InsertTagCommand::DoCommandParams(const char* aCommandName,
                                  nsICommandParams* aParams,
                                  nsISupports* refCon) {
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

  // Don't use nsAutoString here because nsCommandParams stores string member
  // with nsString*.  Therefore, nsAutoString always needs to copy the storage
  // but nsString may avoid it.
  nsString value;
  nsresult rv = aParams->AsCommandParams()->GetString(STATE_ATTRIBUTE, value);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(value.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }

  // filter out tags we don't know how to insert
  nsAtom* attribute = nullptr;
  if (mTagName == nsGkAtoms::a) {
    attribute = nsGkAtoms::href;
  } else if (mTagName == nsGkAtoms::img) {
    attribute = nsGkAtoms::src;
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<Element> newElement = htmlEditor->CreateElementWithDefaults(*mTagName);
  if (NS_WARN_IF(!newElement)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult err;
  newElement->SetAttr(attribute, value, err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  // do actual insertion
  if (mTagName == nsGkAtoms::a) {
    rv = htmlEditor->InsertLinkAroundSelection(newElement);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  rv = htmlEditor->InsertElementAtSelection(newElement, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
InsertTagCommand::GetCommandStateParams(const char* aCommandName,
                                        nsICommandParams* aParams,
                                        nsISupports* refCon) {
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->AsCommandParams()->SetBool(STATE_ENABLED, outCmdEnabled);
}

/****************************/
// HELPER METHODS
/****************************/

static nsresult GetListState(HTMLEditor* aHTMLEditor, bool* aMixed,
                             nsAString& aLocalName) {
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

}  // namespace mozilla
