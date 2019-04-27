/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorCommands.h"

#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc
#include "mozilla/EditorBase.h"  // for EditorBase
#include "mozilla/ErrorResult.h"
#include "mozilla/HTMLEditor.h"  // for HTMLEditor
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsCommandParams.h"          // for nsCommandParams, etc
#include "nsComponentManagerUtils.h"  // for do_CreateInstance
#include "nsGkAtoms.h"                // for nsGkAtoms, nsGkAtoms::font, etc
#include "nsAtom.h"                   // for nsAtom, etc
#include "nsIClipboard.h"             // for nsIClipboard, etc
#include "nsIEditingSession.h"
#include "nsLiteralString.h"  // for NS_LITERAL_STRING
#include "nsReadableUtils.h"  // for EmptyString
#include "nsString.h"         // for nsAutoString, nsString, etc
#include "nsStringFwd.h"      // for nsString

class nsISupports;

namespace mozilla {
using dom::Element;

// prototype
MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
    static nsresult
    GetListState(HTMLEditor* aHTMLEditor, bool* aMixed, nsAString& aLocalName);

// defines
#define STATE_ENABLED "state_enabled"
#define STATE_ALL "state_all"
#define STATE_ANY "state_any"
#define STATE_MIXED "state_mixed"
#define STATE_BEGIN "state_begin"
#define STATE_END "state_end"
#define STATE_ATTRIBUTE "state_attribute"
#define STATE_DATA "state_data"

/*****************************************************************************
 * mozilla::StateUpdatingCommandBase
 *****************************************************************************/

nsRefPtrHashtable<nsCharPtrHashKey, nsAtom>
    StateUpdatingCommandBase::sTagNameTable;

bool StateUpdatingCommandBase::IsCommandEnabled(const char* aCommandName,
                                                TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  if (!aTextEditor->IsSelectionEditable()) {
    return false;
  }
  if (!strcmp(aCommandName, "cmd_absPos")) {
    HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
    return htmlEditor && htmlEditor->IsAbsolutePositionEditorEnabled();
  }
  return true;
}

nsresult StateUpdatingCommandBase::DoCommand(const char* aCommandName,
                                             TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<nsAtom> tagName = TagName(aCommandName);
  if (NS_WARN_IF(!tagName)) {
    return NS_ERROR_UNEXPECTED;
  }
  return ToggleState(tagName, MOZ_KnownLive(htmlEditor));
}

nsresult StateUpdatingCommandBase::DoCommandParams(
    const char* aCommandName, nsCommandParams* aParams,
    TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult StateUpdatingCommandBase::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  if (!aTextEditor) {
    return NS_OK;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<nsAtom> tagName = TagName(aCommandName);
  if (NS_WARN_IF(!tagName)) {
    return NS_ERROR_UNEXPECTED;
  }
  return GetCurrentState(tagName, htmlEditor, &aParams);
}

/*****************************************************************************
 * mozilla::PasteNoFormattingCommand
 *****************************************************************************/

StaticRefPtr<PasteNoFormattingCommand> PasteNoFormattingCommand::sInstance;

bool PasteNoFormattingCommand::IsCommandEnabled(const char* aCommandName,
                                                TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }

  // This command is only implemented by nsIHTMLEditor, since
  //  pasting in a plaintext editor automatically only supplies
  //  "unformatted" text
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  return htmlEditor->CanPaste(nsIClipboard::kGlobalClipboard);
}

nsresult PasteNoFormattingCommand::DoCommand(const char* aCommandName,
                                             TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  // Known live because we hold a ref above in "editor"
  return MOZ_KnownLive(htmlEditor)
      ->PasteNoFormatting(nsIClipboard::kGlobalClipboard);
}

nsresult PasteNoFormattingCommand::DoCommandParams(
    const char* aCommandName, nsCommandParams* aParams,
    TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult PasteNoFormattingCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::StyleUpdatingCommand
 *****************************************************************************/

StaticRefPtr<StyleUpdatingCommand> StyleUpdatingCommand::sInstance;

nsresult StyleUpdatingCommand::GetCurrentState(
    nsAtom* aTagName, HTMLEditor* aHTMLEditor,
    nsICommandParams* aParams) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool firstOfSelectionHasProp = false;
  bool anyOfSelectionHasProp = false;
  bool allOfSelectionHasProp = false;

  nsresult rv = aHTMLEditor->GetInlineProperty(
      aTagName, nullptr, EmptyString(), &firstOfSelectionHasProp,
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

nsresult StyleUpdatingCommand::ToggleState(nsAtom* aTagName,
                                           HTMLEditor* aHTMLEditor) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<nsCommandParams> params = new nsCommandParams();

  // tags "href" and "name" are special cases in the core editor
  // they are used to remove named anchor/link and shouldn't be used for
  // insertion
  bool doTagRemoval;
  if (aTagName == nsGkAtoms::href || aTagName == nsGkAtoms::name) {
    doTagRemoval = true;
  } else {
    // check current selection; set doTagRemoval if formatting should be removed
    nsresult rv = GetCurrentState(aTagName, aHTMLEditor, params);
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
    if (aTagName == nsGkAtoms::b) {
      nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(
          *nsGkAtoms::strong, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (aTagName == nsGkAtoms::i) {
      nsresult rv =
          aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::em, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (aTagName == nsGkAtoms::strike) {
      nsresult rv =
          aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::s, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(*aTagName, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  nsresult rv =
      aHTMLEditor->SetInlinePropertyAsAction(*aTagName, nullptr, EmptyString());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

/*****************************************************************************
 * mozilla::ListCommand
 *****************************************************************************/

StaticRefPtr<ListCommand> ListCommand::sInstance;

nsresult ListCommand::GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                                      nsICommandParams* aParams) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool bMixed;
  nsAutoString localName;
  nsresult rv = GetListState(aHTMLEditor, &bMixed, localName);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList = aTagName->Equals(localName);
  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_ALL, !bMixed && inList);
  params->SetBool(STATE_MIXED, bMixed);
  params->SetBool(STATE_ENABLED, true);
  return NS_OK;
}

nsresult ListCommand::ToggleState(nsAtom* aTagName,
                                  HTMLEditor* aHTMLEditor) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  RefPtr<nsCommandParams> params = new nsCommandParams();
  rv = GetCurrentState(aTagName, aHTMLEditor, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ErrorResult error;
  bool inList = params->GetBool(STATE_ALL, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  nsDependentAtomString listType(aTagName);
  if (inList) {
    rv = aHTMLEditor->RemoveList(listType);
  } else {
    rv = aHTMLEditor->MakeOrChangeList(listType, false, EmptyString());
  }

  return rv;
}

/*****************************************************************************
 * mozilla::ListItemCommand
 *****************************************************************************/

StaticRefPtr<ListItemCommand> ListItemCommand::sInstance;

nsresult ListItemCommand::GetCurrentState(nsAtom* aTagName,
                                          HTMLEditor* aHTMLEditor,
                                          nsICommandParams* aParams) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool bMixed, bLI, bDT, bDD;
  nsresult rv = aHTMLEditor->GetListItemState(&bMixed, &bLI, &bDT, &bDD);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList = false;
  if (!bMixed) {
    if (bLI) {
      inList = aTagName == nsGkAtoms::li;
    } else if (bDT) {
      inList = aTagName == nsGkAtoms::dt;
    } else if (bDD) {
      inList = aTagName == nsGkAtoms::dd;
    }
  }

  nsCommandParams* params = aParams->AsCommandParams();
  params->SetBool(STATE_ALL, !bMixed && inList);
  params->SetBool(STATE_MIXED, bMixed);

  return NS_OK;
}

nsresult ListItemCommand::ToggleState(nsAtom* aTagName,
                                      HTMLEditor* aHTMLEditor) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Need to use aTagName????
  RefPtr<nsCommandParams> params = new nsCommandParams();
  GetCurrentState(aTagName, aHTMLEditor, params);
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
  return aHTMLEditor->SetParagraphFormat(nsDependentAtomString(aTagName));
}

/*****************************************************************************
 * mozilla::RemoveListCommand
 *****************************************************************************/

StaticRefPtr<RemoveListCommand> RemoveListCommand::sInstance;

bool RemoveListCommand::IsCommandEnabled(const char* aCommandName,
                                         TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }

  if (!aTextEditor->IsSelectionEditable()) {
    return false;
  }

  // It is enabled if we are in any list type
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return false;
  }

  bool bMixed;
  nsAutoString localName;
  nsresult rv = GetListState(MOZ_KnownLive(htmlEditor), &bMixed, localName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return bMixed || !localName.IsEmpty();
}

nsresult RemoveListCommand::DoCommand(const char* aCommandName,
                                      TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  // This removes any list type
  return htmlEditor->RemoveList(EmptyString());
}

nsresult RemoveListCommand::DoCommandParams(const char* aCommandName,
                                            nsCommandParams* aParams,
                                            TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult RemoveListCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::IndentCommand
 *****************************************************************************/

StaticRefPtr<IndentCommand> IndentCommand::sInstance;

bool IndentCommand::IsCommandEnabled(const char* aCommandName,
                                     TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult IndentCommand::DoCommand(const char* aCommandName,
                                  TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  nsresult rv = htmlEditor->IndentAsAction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult IndentCommand::DoCommandParams(const char* aCommandName,
                                        nsCommandParams* aParams,
                                        TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult IndentCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::OutdentCommand
 *****************************************************************************/

StaticRefPtr<OutdentCommand> OutdentCommand::sInstance;

bool OutdentCommand::IsCommandEnabled(const char* aCommandName,
                                      TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult OutdentCommand::DoCommand(const char* aCommandName,
                                   TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  nsresult rv = htmlEditor->OutdentAsAction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult OutdentCommand::DoCommandParams(const char* aCommandName,
                                         nsCommandParams* aParams,
                                         TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult OutdentCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::MultiStateCommandBase
 *****************************************************************************/

bool MultiStateCommandBase::IsCommandEnabled(const char* aCommandName,
                                             TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  // should be disabled sometimes, like if the current selection is an image
  return aTextEditor->IsSelectionEditable();
}

nsresult MultiStateCommandBase::DoCommand(const char* aCommandName,
                                          TextEditor& aTextEditor) const {
  NS_WARNING(
      "who is calling MultiStateCommandBase::DoCommand (no implementation)?");
  return NS_OK;
}

nsresult MultiStateCommandBase::DoCommandParams(const char* aCommandName,
                                                nsCommandParams* aParams,
                                                TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString attribute;
  if (aParams) {
    nsAutoCString asciiAttribute;
    nsresult rv = aParams->GetCString(STATE_ATTRIBUTE, asciiAttribute);
    if (NS_SUCCEEDED(rv)) {
      CopyASCIItoUTF16(asciiAttribute, attribute);
    } else {
      aParams->GetString(STATE_ATTRIBUTE, attribute);
    }
  }
  return SetState(MOZ_KnownLive(htmlEditor), attribute);
}

nsresult MultiStateCommandBase::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  if (!aTextEditor) {
    return NS_OK;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return GetCurrentState(MOZ_KnownLive(htmlEditor), &aParams);
}

/*****************************************************************************
 * mozilla::ParagraphStateCommand
 *****************************************************************************/

StaticRefPtr<ParagraphStateCommand> ParagraphStateCommand::sInstance;

nsresult ParagraphStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsICommandParams* aParams) const {
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
                                         const nsString& newState) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->SetParagraphFormat(newState);
}

/*****************************************************************************
 * mozilla::FontFaceStateCommand
 *****************************************************************************/

StaticRefPtr<FontFaceStateCommand> FontFaceStateCommand::sInstance;

nsresult FontFaceStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsICommandParams* aParams) const {
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
                                        const nsString& newState) const {
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

/*****************************************************************************
 * mozilla::FontSizeStateCommand
 *****************************************************************************/

StaticRefPtr<FontSizeStateCommand> FontSizeStateCommand::sInstance;

nsresult FontSizeStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsICommandParams* aParams) const {
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
                                        const nsString& newState) const {
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

/*****************************************************************************
 * mozilla::FontColorStateCommand
 *****************************************************************************/

StaticRefPtr<FontColorStateCommand> FontColorStateCommand::sInstance;

nsresult FontColorStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsICommandParams* aParams) const {
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
                                         const nsString& newState) const {
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

/*****************************************************************************
 * mozilla::HighlightColorStateCommand
 *****************************************************************************/

StaticRefPtr<HighlightColorStateCommand> HighlightColorStateCommand::sInstance;

nsresult HighlightColorStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsICommandParams* aParams) const {
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
                                              const nsString& newState) const {
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

/*****************************************************************************
 * mozilla::BackgroundColorStateCommand
 *****************************************************************************/

StaticRefPtr<BackgroundColorStateCommand>
    BackgroundColorStateCommand::sInstance;

nsresult BackgroundColorStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsICommandParams* aParams) const {
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
                                               const nsString& newState) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->SetBackgroundColor(newState);
}

/*****************************************************************************
 * mozilla::AlignCommand
 *****************************************************************************/

StaticRefPtr<AlignCommand> AlignCommand::sInstance;

nsresult AlignCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                       nsICommandParams* aParams) const {
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
                                const nsString& newState) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->Align(newState);
}

/*****************************************************************************
 * mozilla::AbsolutePositioningCommand
 *****************************************************************************/

StaticRefPtr<AbsolutePositioningCommand> AbsolutePositioningCommand::sInstance;

nsresult AbsolutePositioningCommand::GetCurrentState(
    nsAtom* aTagName, HTMLEditor* aHTMLEditor,
    nsICommandParams* aParams) const {
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

nsresult AbsolutePositioningCommand::ToggleState(
    nsAtom* aTagName, HTMLEditor* aHTMLEditor) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Element> container =
      aHTMLEditor->GetAbsolutelyPositionedSelectionContainer();
  return aHTMLEditor->SetSelectionToAbsoluteOrStatic(!container);
}

/*****************************************************************************
 * mozilla::DecreaseZIndexCommand
 *****************************************************************************/

StaticRefPtr<DecreaseZIndexCommand> DecreaseZIndexCommand::sInstance;

bool DecreaseZIndexCommand::IsCommandEnabled(const char* aCommandName,
                                             TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  if (!htmlEditor->IsAbsolutePositionEditorEnabled()) {
    return false;
  }
  RefPtr<Element> positionedElement = htmlEditor->GetPositionedElement();
  if (!positionedElement) {
    return false;
  }
  return htmlEditor->GetZIndex(*positionedElement) > 0;
}

nsresult DecreaseZIndexCommand::DoCommand(const char* aCommandName,
                                          TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return htmlEditor->AddZIndex(-1);
}

nsresult DecreaseZIndexCommand::DoCommandParams(const char* aCommandName,
                                                nsCommandParams* aParams,
                                                TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult DecreaseZIndexCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::IncreaseZIndexCommand
 *****************************************************************************/

StaticRefPtr<IncreaseZIndexCommand> IncreaseZIndexCommand::sInstance;

bool IncreaseZIndexCommand::IsCommandEnabled(const char* aCommandName,
                                             TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  if (!htmlEditor->IsAbsolutePositionEditorEnabled()) {
    return false;
  }
  return !!htmlEditor->GetPositionedElement();
}

nsresult IncreaseZIndexCommand::DoCommand(const char* aCommandName,
                                          TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return htmlEditor->AddZIndex(1);
}

nsresult IncreaseZIndexCommand::DoCommandParams(const char* aCommandName,
                                                nsCommandParams* aParams,
                                                TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult IncreaseZIndexCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::RemoveStylesCommand
 *****************************************************************************/

StaticRefPtr<RemoveStylesCommand> RemoveStylesCommand::sInstance;

bool RemoveStylesCommand::IsCommandEnabled(const char* aCommandName,
                                           TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  // test if we have any styles?
  return aTextEditor->IsSelectionEditable();
}

nsresult RemoveStylesCommand::DoCommand(const char* aCommandName,
                                        TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  return MOZ_KnownLive(htmlEditor)->RemoveAllInlineProperties();
}

nsresult RemoveStylesCommand::DoCommandParams(const char* aCommandName,
                                              nsCommandParams* aParams,
                                              TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult RemoveStylesCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::IncreaseFontSizeCommand
 *****************************************************************************/

StaticRefPtr<IncreaseFontSizeCommand> IncreaseFontSizeCommand::sInstance;

bool IncreaseFontSizeCommand::IsCommandEnabled(const char* aCommandName,
                                               TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  // test if we are at max size?
  return aTextEditor->IsSelectionEditable();
}

nsresult IncreaseFontSizeCommand::DoCommand(const char* aCommandName,
                                            TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  return MOZ_KnownLive(htmlEditor)->IncreaseFontSize();
}

nsresult IncreaseFontSizeCommand::DoCommandParams(
    const char* aCommandName, nsCommandParams* aParams,
    TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult IncreaseFontSizeCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::DecreaseFontSizeCommand
 *****************************************************************************/

StaticRefPtr<DecreaseFontSizeCommand> DecreaseFontSizeCommand::sInstance;

bool DecreaseFontSizeCommand::IsCommandEnabled(const char* aCommandName,
                                               TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  // test if we are at min size?
  return aTextEditor->IsSelectionEditable();
}

nsresult DecreaseFontSizeCommand::DoCommand(const char* aCommandName,
                                            TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;
  }
  return MOZ_KnownLive(htmlEditor)->DecreaseFontSize();
}

nsresult DecreaseFontSizeCommand::DoCommandParams(
    const char* aCommandName, nsCommandParams* aParams,
    TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult DecreaseFontSizeCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::InsertHTMLCommand
 *****************************************************************************/

StaticRefPtr<InsertHTMLCommand> InsertHTMLCommand::sInstance;

bool InsertHTMLCommand::IsCommandEnabled(const char* aCommandName,
                                         TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult InsertHTMLCommand::DoCommand(const char* aCommandName,
                                      TextEditor& aTextEditor) const {
  // If nsInsertHTMLCommand is called with no parameters, it was probably called
  // with an empty string parameter ''. In this case, it should act the same as
  // the delete command
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString html;
  return MOZ_KnownLive(htmlEditor)->InsertHTML(html);
}

nsresult InsertHTMLCommand::DoCommandParams(const char* aCommandName,
                                            nsCommandParams* aParams,
                                            TextEditor& aTextEditor) const {
  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }

  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  // Get HTML source string to insert from command params
  nsAutoString html;
  nsresult rv = aParams->GetString(STATE_DATA, html);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return MOZ_KnownLive(htmlEditor)->InsertHTML(html);
}

nsresult InsertHTMLCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * mozilla::InsertTagCommand
 *****************************************************************************/

StaticRefPtr<InsertTagCommand> InsertTagCommand::sInstance;
nsRefPtrHashtable<nsCharPtrHashKey, nsAtom> InsertTagCommand::sTagNameTable;

bool InsertTagCommand::IsCommandEnabled(const char* aCommandName,
                                        TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

// corresponding STATE_ATTRIBUTE is: src (img) and href (a)
nsresult InsertTagCommand::DoCommand(const char* aCmdName,
                                     TextEditor& aTextEditor) const {
  RefPtr<nsAtom> tagName = TagName(aCmdName);
  if (NS_WARN_IF(tagName != nsGkAtoms::hr)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> newElement =
      MOZ_KnownLive(htmlEditor)->CreateElementWithDefaults(*tagName);
  if (NS_WARN_IF(!newElement)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv =
      MOZ_KnownLive(htmlEditor)->InsertElementAtSelection(newElement, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult InsertTagCommand::DoCommandParams(const char* aCommandName,
                                           nsCommandParams* aParams,
                                           TextEditor& aTextEditor) const {
  // inserting an hr shouldn't have an parameters, just call DoCommand for that
  if (!strcmp(aCommandName, "cmd_insertHR")) {
    return DoCommand(aCommandName, aTextEditor);
  }

  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }
  RefPtr<nsAtom> tagName = TagName(aCommandName);
  if (NS_WARN_IF(!tagName)) {
    return NS_ERROR_UNEXPECTED;
  }

  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  // Don't use nsAutoString here because nsCommandParams stores string member
  // with nsString*.  Therefore, nsAutoString always needs to copy the storage
  // but nsString may avoid it.
  nsString value;
  nsresult rv = aParams->GetString(STATE_ATTRIBUTE, value);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(value.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }

  // filter out tags we don't know how to insert
  nsAtom* attribute = nullptr;
  if (tagName == nsGkAtoms::a) {
    attribute = nsGkAtoms::href;
  } else if (tagName == nsGkAtoms::img) {
    attribute = nsGkAtoms::src;
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<Element> newElement =
      MOZ_KnownLive(htmlEditor)->CreateElementWithDefaults(*tagName);
  if (NS_WARN_IF(!newElement)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult err;
  newElement->SetAttr(attribute, value, err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  // do actual insertion
  if (tagName == nsGkAtoms::a) {
    rv = MOZ_KnownLive(htmlEditor)->InsertLinkAroundSelection(newElement);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  rv = MOZ_KnownLive(htmlEditor)->InsertElementAtSelection(newElement, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult InsertTagCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/*****************************************************************************
 * Helper methods
 *****************************************************************************/

static nsresult GetListState(HTMLEditor* aHTMLEditor, bool* aMixed,
                             nsAString& aLocalName)
    MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
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
