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
#include "nsAtom.h"                   // for nsAtom, nsStaticAtom, etc
#include "nsCommandParams.h"          // for nsCommandParams, etc
#include "nsComponentManagerUtils.h"  // for do_CreateInstance
#include "nsGkAtoms.h"                // for nsGkAtoms, nsGkAtoms::font, etc
#include "nsIClipboard.h"             // for nsIClipboard, etc
#include "nsIEditingSession.h"
#include "nsIPrincipal.h"     // for nsIPrincipal
#include "nsLiteralString.h"  // for NS_LITERAL_STRING
#include "nsReadableUtils.h"  // for EmptyString
#include "nsString.h"         // for nsAutoString, nsString, etc
#include "nsStringFwd.h"      // for nsString

class nsISupports;

namespace mozilla {
using dom::Element;

// prototype
MOZ_CAN_RUN_SCRIPT
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

/*****************************************************************************
 * mozilla::StateUpdatingCommandBase
 *****************************************************************************/

bool StateUpdatingCommandBase::IsCommandEnabled(Command aCommand,
                                                TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  if (!htmlEditor->IsSelectionEditable()) {
    return false;
  }
  if (aCommand == Command::FormatAbsolutePosition) {
    return htmlEditor->IsAbsolutePositionEditorEnabled();
  }
  return true;
}

nsresult StateUpdatingCommandBase::DoCommand(Command aCommand,
                                             TextEditor& aTextEditor,
                                             nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  nsAtom* tagName = GetTagName(aCommand);
  if (NS_WARN_IF(!tagName)) {
    return NS_ERROR_UNEXPECTED;
  }
  return ToggleState(MOZ_KnownLive(tagName), MOZ_KnownLive(htmlEditor),
                     aPrincipal);
}

nsresult StateUpdatingCommandBase::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  if (!aTextEditor) {
    return NS_OK;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  nsAtom* tagName = GetTagName(aCommand);
  if (NS_WARN_IF(!tagName)) {
    return NS_ERROR_UNEXPECTED;
  }
  return GetCurrentState(MOZ_KnownLive(tagName), MOZ_KnownLive(htmlEditor),
                         aParams);
}

/*****************************************************************************
 * mozilla::PasteNoFormattingCommand
 *****************************************************************************/

StaticRefPtr<PasteNoFormattingCommand> PasteNoFormattingCommand::sInstance;

bool PasteNoFormattingCommand::IsCommandEnabled(Command aCommand,
                                                TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  return htmlEditor->CanPaste(nsIClipboard::kGlobalClipboard);
}

nsresult PasteNoFormattingCommand::DoCommand(Command aCommand,
                                             TextEditor& aTextEditor,
                                             nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  // Known live because we hold a ref above in "editor"
  nsresult rv = MOZ_KnownLive(htmlEditor)
                    ->PasteNoFormattingAsAction(nsIClipboard::kGlobalClipboard,
                                                aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "PasteNoFormattingAsAction() failed");
  return rv;
}

nsresult PasteNoFormattingCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::StyleUpdatingCommand
 *****************************************************************************/

StaticRefPtr<StyleUpdatingCommand> StyleUpdatingCommand::sInstance;

nsresult StyleUpdatingCommand::GetCurrentState(nsAtom* aTagName,
                                               HTMLEditor* aHTMLEditor,
                                               nsCommandParams& aParams) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool firstOfSelectionHasProp = false;
  bool anyOfSelectionHasProp = false;
  bool allOfSelectionHasProp = false;

  nsresult rv = aHTMLEditor->GetInlineProperty(
      aTagName, nullptr, EmptyString(), &firstOfSelectionHasProp,
      &anyOfSelectionHasProp, &allOfSelectionHasProp);

  aParams.SetBool(STATE_ENABLED, NS_SUCCEEDED(rv));
  aParams.SetBool(STATE_ALL, allOfSelectionHasProp);
  aParams.SetBool(STATE_ANY, anyOfSelectionHasProp);
  aParams.SetBool(STATE_MIXED, anyOfSelectionHasProp && !allOfSelectionHasProp);
  aParams.SetBool(STATE_BEGIN, firstOfSelectionHasProp);
  aParams.SetBool(STATE_END, allOfSelectionHasProp);  // not completely accurate
  return NS_OK;
}

nsresult StyleUpdatingCommand::ToggleState(nsAtom* aTagName,
                                           HTMLEditor* aHTMLEditor,
                                           nsIPrincipal* aPrincipal) const {
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
    nsresult rv = GetCurrentState(aTagName, aHTMLEditor, *params);
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
          *nsGkAtoms::strong, nullptr, aPrincipal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (aTagName == nsGkAtoms::i) {
      nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(
          *nsGkAtoms::em, nullptr, aPrincipal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (aTagName == nsGkAtoms::strike) {
      nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(
          *nsGkAtoms::s, nullptr, aPrincipal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(*aTagName, nullptr,
                                                            aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "RemoveInlinePropertyAsAction() failed");
    return rv;
  }

  nsresult rv = aHTMLEditor->SetInlinePropertyAsAction(
      *aTagName, nullptr, EmptyString(), aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetInlinePropertyAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::ListCommand
 *****************************************************************************/

StaticRefPtr<ListCommand> ListCommand::sInstance;

nsresult ListCommand::GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                                      nsCommandParams& aParams) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool bMixed;
  nsAutoString localName;
  nsresult rv = GetListState(aHTMLEditor, &bMixed, localName);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList = aTagName->Equals(localName);
  aParams.SetBool(STATE_ALL, !bMixed && inList);
  aParams.SetBool(STATE_MIXED, bMixed);
  aParams.SetBool(STATE_ENABLED, true);
  return NS_OK;
}

nsresult ListCommand::ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                                  nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  RefPtr<nsCommandParams> params = new nsCommandParams();
  rv = GetCurrentState(aTagName, aHTMLEditor, *params);
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
    rv = aHTMLEditor->RemoveListAsAction(listType, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "RemoveListAsAction() failed");
    return rv;
  }

  rv = aHTMLEditor->MakeOrChangeListAsAction(
      *aTagName, EmptyString(), HTMLEditor::SelectAllOfCurrentList::No,
      aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "MakeOrChangeListAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::ListItemCommand
 *****************************************************************************/

StaticRefPtr<ListItemCommand> ListItemCommand::sInstance;

nsresult ListItemCommand::GetCurrentState(nsAtom* aTagName,
                                          HTMLEditor* aHTMLEditor,
                                          nsCommandParams& aParams) const {
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

  aParams.SetBool(STATE_ALL, !bMixed && inList);
  aParams.SetBool(STATE_MIXED, bMixed);

  return NS_OK;
}

nsresult ListItemCommand::ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                                      nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aTagName) || NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Need to use aTagName????
  RefPtr<nsCommandParams> params = new nsCommandParams();
  GetCurrentState(aTagName, aHTMLEditor, *params);
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
    rv = aHTMLEditor->RemoveListAsAction(localName, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "RemoveListAsAction() failed");
    return rv;
  }

  // Set to the requested paragraph type
  // XXX Note: This actually doesn't work for "LI",
  //    but we currently don't use this for non DL lists anyway.
  // Problem: won't this replace any current block paragraph style?
  nsresult rv = aHTMLEditor->SetParagraphFormatAsAction(
      nsDependentAtomString(aTagName), aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetParagraphFormatAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::RemoveListCommand
 *****************************************************************************/

StaticRefPtr<RemoveListCommand> RemoveListCommand::sInstance;

bool RemoveListCommand::IsCommandEnabled(Command aCommand,
                                         TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }

  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }

  if (!htmlEditor->IsSelectionEditable()) {
    return false;
  }

  // It is enabled if we are in any list type
  bool bMixed;
  nsAutoString localName;
  nsresult rv = GetListState(MOZ_KnownLive(htmlEditor), &bMixed, localName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return bMixed || !localName.IsEmpty();
}

nsresult RemoveListCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                      nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_OK;
  }
  // This removes any list type
  nsresult rv =
      MOZ_KnownLive(htmlEditor)->RemoveListAsAction(EmptyString(), aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "RemoveListAsAction() failed");
  return rv;
}

nsresult RemoveListCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::IndentCommand
 *****************************************************************************/

StaticRefPtr<IndentCommand> IndentCommand::sInstance;

bool IndentCommand::IsCommandEnabled(Command aCommand,
                                     TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  return htmlEditor->IsSelectionEditable();
}

nsresult IndentCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                  nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_OK;
  }
  nsresult rv = MOZ_KnownLive(htmlEditor)->IndentAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "IndentAsAction() failed");
  return rv;
}

nsresult IndentCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::OutdentCommand
 *****************************************************************************/

StaticRefPtr<OutdentCommand> OutdentCommand::sInstance;

bool OutdentCommand::IsCommandEnabled(Command aCommand,
                                      TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  return htmlEditor->IsSelectionEditable();
}

nsresult OutdentCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                   nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_OK;
  }
  nsresult rv = MOZ_KnownLive(htmlEditor)->OutdentAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "OutdentAsAction() failed");
  return rv;
}

nsresult OutdentCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::MultiStateCommandBase
 *****************************************************************************/

bool MultiStateCommandBase::IsCommandEnabled(Command aCommand,
                                             TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  // should be disabled sometimes, like if the current selection is an image
  return htmlEditor->IsSelectionEditable();
}

nsresult MultiStateCommandBase::DoCommand(Command aCommand,
                                          TextEditor& aTextEditor,
                                          nsIPrincipal* aPrincipal) const {
  NS_WARNING(
      "who is calling MultiStateCommandBase::DoCommand (no implementation)?");
  return NS_OK;
}

nsresult MultiStateCommandBase::DoCommandParam(Command aCommand,
                                               const nsAString& aStringParam,
                                               TextEditor& aTextEditor,
                                               nsIPrincipal* aPrincipal) const {
  NS_WARNING_ASSERTION(aCommand != Command::FormatJustify,
                       "Command::FormatJustify should be used only for "
                       "IsCommandEnabled() and GetCommandStateParams()");
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = SetState(MOZ_KnownLive(htmlEditor), aStringParam, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetState() failed");
  return rv;
}

nsresult MultiStateCommandBase::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  if (!aTextEditor) {
    return NS_OK;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return GetCurrentState(MOZ_KnownLive(htmlEditor), aParams);
}

/*****************************************************************************
 * mozilla::ParagraphStateCommand
 *****************************************************************************/

StaticRefPtr<ParagraphStateCommand> ParagraphStateCommand::sInstance;

nsresult ParagraphStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsCommandParams& aParams) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetParagraphState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tOutStateString;
    LossyCopyUTF16toASCII(outStateString, tOutStateString);
    aParams.SetBool(STATE_MIXED, outMixed);
    aParams.SetCString(STATE_ATTRIBUTE, tOutStateString);
  }
  return rv;
}

nsresult ParagraphStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                         const nsAString& aNewState,
                                         nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv = aHTMLEditor->SetParagraphFormatAsAction(aNewState, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetParagraphFormatAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::FontFaceStateCommand
 *****************************************************************************/

StaticRefPtr<FontFaceStateCommand> FontFaceStateCommand::sInstance;

nsresult FontFaceStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                               nsCommandParams& aParams) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString outStateString;
  bool outMixed;
  nsresult rv = aHTMLEditor->GetFontFaceState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv)) {
    aParams.SetBool(STATE_MIXED, outMixed);
    aParams.SetCString(STATE_ATTRIBUTE, NS_ConvertUTF16toUTF8(outStateString));
  }
  return rv;
}

nsresult FontFaceStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                        const nsAString& aNewState,
                                        nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aNewState.EqualsLiteral("tt")) {
    // The old "teletype" attribute
    nsresult rv = aHTMLEditor->SetInlinePropertyAsAction(
        *nsGkAtoms::tt, nullptr, EmptyString(), aPrincipal);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Clear existing font face
    rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::font,
                                                   nsGkAtoms::face, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "RemoveInlinePropertyAsAction() failed");
    return rv;
  }

  // Remove any existing TT nodes
  nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::tt,
                                                          nullptr, aPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aNewState.IsEmpty() || aNewState.EqualsLiteral("normal")) {
    rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::font,
                                                   nsGkAtoms::face, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "RemoveInlinePropertyAsAction() failed");
    return rv;
  }

  rv = aHTMLEditor->SetInlinePropertyAsAction(*nsGkAtoms::font, nsGkAtoms::face,
                                              aNewState, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetInlinePropertyAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::FontSizeStateCommand
 *****************************************************************************/

StaticRefPtr<FontSizeStateCommand> FontSizeStateCommand::sInstance;

nsresult FontSizeStateCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                               nsCommandParams& aParams) const {
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
  aParams.SetBool(STATE_MIXED, anyHas && !allHas);
  aParams.SetCString(STATE_ATTRIBUTE, tOutStateString);
  aParams.SetBool(STATE_ENABLED, true);

  return rv;
}

// acceptable values for "aNewState" are:
//   -2
//   -1
//    0
//   +1
//   +2
//   +3
//   medium
//   normal
nsresult FontSizeStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                        const nsAString& aNewState,
                                        nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aNewState.IsEmpty() && !aNewState.EqualsLiteral("normal") &&
      !aNewState.EqualsLiteral("medium")) {
    nsresult rv = aHTMLEditor->SetInlinePropertyAsAction(
        *nsGkAtoms::font, nsGkAtoms::size, aNewState, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "SetInlinePropertyAsAction() failed");
    return rv;
  }

  // remove any existing font size, big or small
  nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(
      *nsGkAtoms::font, nsGkAtoms::size, aPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::big, nullptr,
                                                 aPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHTMLEditor->RemoveInlinePropertyAsAction(*nsGkAtoms::small, nullptr,
                                                 aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "RemoveInlinePropertyAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::FontColorStateCommand
 *****************************************************************************/

StaticRefPtr<FontColorStateCommand> FontColorStateCommand::sInstance;

nsresult FontColorStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsCommandParams& aParams) const {
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
  aParams.SetBool(STATE_MIXED, outMixed);
  aParams.SetCString(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult FontColorStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                         const nsAString& aNewState,
                                         nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aNewState.IsEmpty() || aNewState.EqualsLiteral("normal")) {
    nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(
        *nsGkAtoms::font, nsGkAtoms::color, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "RemoveInlinePropertyAsAction() failed");
    return rv;
  }

  nsresult rv = aHTMLEditor->SetInlinePropertyAsAction(
      *nsGkAtoms::font, nsGkAtoms::color, aNewState, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetInlinePropertyAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::HighlightColorStateCommand
 *****************************************************************************/

StaticRefPtr<HighlightColorStateCommand> HighlightColorStateCommand::sInstance;

nsresult HighlightColorStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsCommandParams& aParams) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetHighlightColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  aParams.SetBool(STATE_MIXED, outMixed);
  aParams.SetCString(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult HighlightColorStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                              const nsAString& aNewState,
                                              nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aNewState.IsEmpty() || aNewState.EqualsLiteral("normal")) {
    nsresult rv = aHTMLEditor->RemoveInlinePropertyAsAction(
        *nsGkAtoms::font, nsGkAtoms::bgcolor, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "RemoveInlinePropertyAsAction() failed");
    return rv;
  }

  nsresult rv = aHTMLEditor->SetInlinePropertyAsAction(
      *nsGkAtoms::font, nsGkAtoms::bgcolor, aNewState, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetInlinePropertyAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::BackgroundColorStateCommand
 *****************************************************************************/

StaticRefPtr<BackgroundColorStateCommand>
    BackgroundColorStateCommand::sInstance;

nsresult BackgroundColorStateCommand::GetCurrentState(
    HTMLEditor* aHTMLEditor, nsCommandParams& aParams) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = aHTMLEditor->GetBackgroundColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tOutStateString;
  LossyCopyUTF16toASCII(outStateString, tOutStateString);
  aParams.SetBool(STATE_MIXED, outMixed);
  aParams.SetCString(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult BackgroundColorStateCommand::SetState(HTMLEditor* aHTMLEditor,
                                               const nsAString& aNewState,
                                               nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv = aHTMLEditor->SetBackgroundColorAsAction(aNewState, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetBackgroundColorAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::AlignCommand
 *****************************************************************************/

StaticRefPtr<AlignCommand> AlignCommand::sInstance;

nsresult AlignCommand::GetCurrentState(HTMLEditor* aHTMLEditor,
                                       nsCommandParams& aParams) const {
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
  aParams.SetBool(STATE_MIXED, outMixed);
  aParams.SetCString(STATE_ATTRIBUTE, tOutStateString);
  return NS_OK;
}

nsresult AlignCommand::SetState(HTMLEditor* aHTMLEditor,
                                const nsAString& aNewState,
                                nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv = aHTMLEditor->AlignAsAction(aNewState, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AlignAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::AbsolutePositioningCommand
 *****************************************************************************/

StaticRefPtr<AbsolutePositioningCommand> AbsolutePositioningCommand::sInstance;

nsresult AbsolutePositioningCommand::GetCurrentState(
    nsAtom* aTagName, HTMLEditor* aHTMLEditor, nsCommandParams& aParams) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aHTMLEditor->IsAbsolutePositionEditorEnabled()) {
    aParams.SetBool(STATE_MIXED, false);
    aParams.SetCString(STATE_ATTRIBUTE, EmptyCString());
    return NS_OK;
  }

  RefPtr<Element> container =
      aHTMLEditor->GetAbsolutelyPositionedSelectionContainer();
  aParams.SetBool(STATE_MIXED, false);
  aParams.SetCString(STATE_ATTRIBUTE, container ? NS_LITERAL_CSTRING("absolute")
                                                : EmptyCString());
  return NS_OK;
}

nsresult AbsolutePositioningCommand::ToggleState(
    nsAtom* aTagName, HTMLEditor* aHTMLEditor, nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Element> container =
      aHTMLEditor->GetAbsolutelyPositionedSelectionContainer();
  nsresult rv = aHTMLEditor->SetSelectionToAbsoluteOrStaticAsAction(!container,
                                                                    aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "SetSelectionToAbsoluteOrStaticAsAction() failed");
  return rv;
}

/*****************************************************************************
 * mozilla::DecreaseZIndexCommand
 *****************************************************************************/

StaticRefPtr<DecreaseZIndexCommand> DecreaseZIndexCommand::sInstance;

bool DecreaseZIndexCommand::IsCommandEnabled(Command aCommand,
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

nsresult DecreaseZIndexCommand::DoCommand(Command aCommand,
                                          TextEditor& aTextEditor,
                                          nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = MOZ_KnownLive(htmlEditor)->AddZIndexAsAction(-1, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddZIndexAsAction() failed");
  return rv;
}

nsresult DecreaseZIndexCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::IncreaseZIndexCommand
 *****************************************************************************/

StaticRefPtr<IncreaseZIndexCommand> IncreaseZIndexCommand::sInstance;

bool IncreaseZIndexCommand::IsCommandEnabled(Command aCommand,
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

nsresult IncreaseZIndexCommand::DoCommand(Command aCommand,
                                          TextEditor& aTextEditor,
                                          nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = MOZ_KnownLive(htmlEditor)->AddZIndexAsAction(1, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddZIndexAsAction() failed");
  return rv;
}

nsresult IncreaseZIndexCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::RemoveStylesCommand
 *****************************************************************************/

StaticRefPtr<RemoveStylesCommand> RemoveStylesCommand::sInstance;

bool RemoveStylesCommand::IsCommandEnabled(Command aCommand,
                                           TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  // test if we have any styles?
  return htmlEditor->IsSelectionEditable();
}

nsresult RemoveStylesCommand::DoCommand(Command aCommand,
                                        TextEditor& aTextEditor,
                                        nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_OK;
  }
  nsresult rv =
      MOZ_KnownLive(htmlEditor)->RemoveAllInlinePropertiesAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "RemoveAllInlinePropertiesAsAction() failed");
  return rv;
}

nsresult RemoveStylesCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::IncreaseFontSizeCommand
 *****************************************************************************/

StaticRefPtr<IncreaseFontSizeCommand> IncreaseFontSizeCommand::sInstance;

bool IncreaseFontSizeCommand::IsCommandEnabled(Command aCommand,
                                               TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  // test if we are at max size?
  return htmlEditor->IsSelectionEditable();
}

nsresult IncreaseFontSizeCommand::DoCommand(Command aCommand,
                                            TextEditor& aTextEditor,
                                            nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_OK;
  }
  nsresult rv = MOZ_KnownLive(htmlEditor)->IncreaseFontSizeAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "IncreaseFontSizeAsAction() failed");
  return rv;
}

nsresult IncreaseFontSizeCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::DecreaseFontSizeCommand
 *****************************************************************************/

StaticRefPtr<DecreaseFontSizeCommand> DecreaseFontSizeCommand::sInstance;

bool DecreaseFontSizeCommand::IsCommandEnabled(Command aCommand,
                                               TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  // test if we are at min size?
  return htmlEditor->IsSelectionEditable();
}

nsresult DecreaseFontSizeCommand::DoCommand(Command aCommand,
                                            TextEditor& aTextEditor,
                                            nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_OK;
  }
  nsresult rv = MOZ_KnownLive(htmlEditor)->DecreaseFontSizeAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DecreaseFontSizeAsAction() failed");
  return rv;
}

nsresult DecreaseFontSizeCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::InsertHTMLCommand
 *****************************************************************************/

StaticRefPtr<InsertHTMLCommand> InsertHTMLCommand::sInstance;

bool InsertHTMLCommand::IsCommandEnabled(Command aCommand,
                                         TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  return htmlEditor->IsSelectionEditable();
}

nsresult InsertHTMLCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                      nsIPrincipal* aPrincipal) const {
  // If InsertHTMLCommand is called with no parameters, it was probably called
  // with an empty string parameter ''. In this case, it should act the same as
  // the delete command
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv =
      MOZ_KnownLive(htmlEditor)->InsertHTMLAsAction(EmptyString(), aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "InsertHTMLAsAction() failed");
  return rv;
}

nsresult InsertHTMLCommand::DoCommandParam(Command aCommand,
                                           const nsAString& aStringParam,
                                           TextEditor& aTextEditor,
                                           nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(aStringParam.IsVoid())) {
    return NS_ERROR_INVALID_ARG;
  }

  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv =
      MOZ_KnownLive(htmlEditor)->InsertHTMLAsAction(aStringParam, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "InsertHTMLAsAction() failed");
  return rv;
}

nsresult InsertHTMLCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/*****************************************************************************
 * mozilla::InsertTagCommand
 *****************************************************************************/

StaticRefPtr<InsertTagCommand> InsertTagCommand::sInstance;

bool InsertTagCommand::IsCommandEnabled(Command aCommand,
                                        TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }
  return htmlEditor->IsSelectionEditable();
}

// corresponding STATE_ATTRIBUTE is: src (img) and href (a)
nsresult InsertTagCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                     nsIPrincipal* aPrincipal) const {
  nsAtom* tagName = GetTagName(aCommand);
  if (NS_WARN_IF(tagName != nsGkAtoms::hr)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> newElement =
      MOZ_KnownLive(htmlEditor)
          ->CreateElementWithDefaults(MOZ_KnownLive(*tagName));
  if (NS_WARN_IF(!newElement)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv =
      MOZ_KnownLive(htmlEditor)
          ->InsertElementAtSelectionAsAction(newElement, true, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "InsertElementAtSelectionAsAction() failed");
  return rv;
}

nsresult InsertTagCommand::DoCommandParam(Command aCommand,
                                          const nsAString& aStringParam,
                                          TextEditor& aTextEditor,
                                          nsIPrincipal* aPrincipal) const {
  MOZ_ASSERT(aCommand != Command::InsertHorizontalRule);

  if (NS_WARN_IF(aStringParam.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }
  nsAtom* tagName = GetTagName(aCommand);
  if (NS_WARN_IF(!tagName)) {
    return NS_ERROR_UNEXPECTED;
  }

  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
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
      MOZ_KnownLive(htmlEditor)
          ->CreateElementWithDefaults(MOZ_KnownLive(*tagName));
  if (NS_WARN_IF(!newElement)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult err;
  newElement->SetAttr(attribute, aStringParam, err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  // do actual insertion
  if (tagName == nsGkAtoms::a) {
    nsresult rv =
        MOZ_KnownLive(htmlEditor)
            ->InsertLinkAroundSelectionAsAction(newElement, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "InsertLinkAroundSelectionAsAction() failed");
    return rv;
  }

  nsresult rv =
      MOZ_KnownLive(htmlEditor)
          ->InsertElementAtSelectionAsAction(newElement, true, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "InsertElementAtSelectionAsAction() failed");
  return rv;
}

nsresult InsertTagCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
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
