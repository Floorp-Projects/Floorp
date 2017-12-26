/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include <stdio.h>                      // for printf

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/EditorBase.h"         // for EditorBase
#include "mozilla/HTMLEditor.h"         // for HTMLEditor
#include "nsAString.h"
#include "nsCOMPtr.h"                   // for nsCOMPtr, do_QueryInterface, etc
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsComposerCommands.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsError.h"                    // for NS_OK, NS_ERROR_FAILURE, etc
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::font, etc
#include "nsAtom.h"                    // for nsAtom, etc
#include "nsIClipboard.h"               // for nsIClipboard, etc
#include "nsICommandParams.h"           // for nsICommandParams, etc
#include "nsID.h"
#include "nsIDOMElement.h"              // for nsIDOMElement
#include "nsIEditor.h"                  // for nsIEditor
#include "nsIHTMLEditor.h"              // for nsIHTMLEditor, etc
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsReadableUtils.h"            // for EmptyString
#include "nsString.h"                   // for nsAutoString, nsString, etc
#include "nsStringFwd.h"                // for nsString

class nsISupports;

//prototype
nsresult GetListState(mozilla::HTMLEditor* aHTMLEditor,
                      bool* aMixed,
                      nsAString& aLocalName);
nsresult RemoveOneProperty(mozilla::HTMLEditor* aHTMLEditor,
                           const nsAString& aProp);
nsresult RemoveTextProperty(mozilla::HTMLEditor* aHTMLEditor,
                            const nsAString& aProp);
nsresult SetTextProperty(mozilla::HTMLEditor* aHTMLEditor,
                         const nsAString& aProp);


//defines
#define STATE_ENABLED  "state_enabled"
#define STATE_ALL "state_all"
#define STATE_ANY "state_any"
#define STATE_MIXED "state_mixed"
#define STATE_BEGIN "state_begin"
#define STATE_END "state_end"
#define STATE_ATTRIBUTE "state_attribute"
#define STATE_DATA "state_data"


nsBaseComposerCommand::nsBaseComposerCommand()
{
}

NS_IMPL_ISUPPORTS(nsBaseComposerCommand, nsIControllerCommand)


nsBaseStateUpdatingCommand::nsBaseStateUpdatingCommand(nsAtom* aTagName)
: nsBaseComposerCommand()
, mTagName(aTagName)
{
  MOZ_ASSERT(mTagName);
}

nsBaseStateUpdatingCommand::~nsBaseStateUpdatingCommand()
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsBaseStateUpdatingCommand, nsBaseComposerCommand)

NS_IMETHODIMP
nsBaseStateUpdatingCommand::IsCommandEnabled(const char *aCommandName,
                                             nsISupports *refCon,
                                             bool *outCmdEnabled)
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
nsBaseStateUpdatingCommand::DoCommand(const char *aCommandName,
                                      nsISupports *refCon)
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
nsBaseStateUpdatingCommand::DoCommandParams(const char *aCommandName,
                                            nsICommandParams *aParams,
                                            nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsBaseStateUpdatingCommand::GetCommandStateParams(const char *aCommandName,
                                                  nsICommandParams *aParams,
                                                  nsISupports *refCon)
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
nsPasteNoFormattingCommand::IsCommandEnabled(const char * aCommandName,
                                             nsISupports *refCon,
                                             bool *outCmdEnabled)
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
nsPasteNoFormattingCommand::DoCommand(const char *aCommandName,
                                      nsISupports *refCon)
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
nsPasteNoFormattingCommand::DoCommandParams(const char *aCommandName,
                                            nsICommandParams *aParams,
                                            nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsPasteNoFormattingCommand::GetCommandStateParams(const char *aCommandName,
                                                  nsICommandParams *aParams,
                                                  nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled = false;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->SetBooleanValue(STATE_ENABLED, enabled);
}

nsStyleUpdatingCommand::nsStyleUpdatingCommand(nsAtom* aTagName)
: nsBaseStateUpdatingCommand(aTagName)
{
}

nsresult
nsStyleUpdatingCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
nsStyleUpdatingCommand::ToggleState(mozilla::HTMLEditor* aHTMLEditor)
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
      rv = RemoveTextProperty(aHTMLEditor, NS_LITERAL_STRING("strong"));
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (mTagName == nsGkAtoms::i) {
      rv = RemoveTextProperty(aHTMLEditor, NS_LITERAL_STRING("em"));
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (mTagName == nsGkAtoms::strike) {
      rv = RemoveTextProperty(aHTMLEditor, NS_LITERAL_STRING("s"));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = RemoveTextProperty(aHTMLEditor, nsDependentAtomString(mTagName));
  } else {
    // Superscript and Subscript styles are mutually exclusive
    aHTMLEditor->BeginTransaction();

    nsDependentAtomString tagName(mTagName);
    if (mTagName == nsGkAtoms::sub || mTagName == nsGkAtoms::sup) {
      rv = RemoveTextProperty(aHTMLEditor, tagName);
    }
    if (NS_SUCCEEDED(rv))
      rv = SetTextProperty(aHTMLEditor, tagName);

    aHTMLEditor->EndTransaction();
  }

  return rv;
}

nsListCommand::nsListCommand(nsAtom* aTagName)
: nsBaseStateUpdatingCommand(aTagName)
{
}

nsresult
nsListCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
nsListCommand::ToggleState(mozilla::HTMLEditor* aHTMLEditor)
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

nsListItemCommand::nsListItemCommand(nsAtom* aTagName)
: nsBaseStateUpdatingCommand(aTagName)
{
}

nsresult
nsListItemCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
nsListItemCommand::ToggleState(mozilla::HTMLEditor* aHTMLEditor)
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
nsRemoveListCommand::IsCommandEnabled(const char * aCommandName,
                                      nsISupports *refCon,
                                      bool *outCmdEnabled)
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
nsRemoveListCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
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
nsRemoveListCommand::DoCommandParams(const char *aCommandName,
                                     nsICommandParams *aParams,
                                     nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsRemoveListCommand::GetCommandStateParams(const char *aCommandName,
                                           nsICommandParams *aParams,
                                           nsISupports *refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

NS_IMETHODIMP
nsIndentCommand::IsCommandEnabled(const char * aCommandName,
                                  nsISupports *refCon, bool *outCmdEnabled)
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
nsIndentCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
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
nsIndentCommand::DoCommandParams(const char *aCommandName,
                                 nsICommandParams *aParams,
                                 nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsIndentCommand::GetCommandStateParams(const char *aCommandName,
                                       nsICommandParams *aParams,
                                       nsISupports *refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}


//OUTDENT

NS_IMETHODIMP
nsOutdentCommand::IsCommandEnabled(const char * aCommandName,
                                   nsISupports *refCon,
                                   bool *outCmdEnabled)
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
nsOutdentCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
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
nsOutdentCommand::DoCommandParams(const char *aCommandName,
                                  nsICommandParams *aParams,
                                  nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsOutdentCommand::GetCommandStateParams(const char *aCommandName,
                                        nsICommandParams *aParams,
                                        nsISupports *refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

nsMultiStateCommand::nsMultiStateCommand()
: nsBaseComposerCommand()
{
}

nsMultiStateCommand::~nsMultiStateCommand()
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsMultiStateCommand, nsBaseComposerCommand)

NS_IMETHODIMP
nsMultiStateCommand::IsCommandEnabled(const char * aCommandName,
                                      nsISupports *refCon,
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
nsMultiStateCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
{
#ifdef DEBUG
  printf("who is calling nsMultiStateCommand::DoCommand \
          (no implementation)? %s\n", aCommandName);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsMultiStateCommand::DoCommandParams(const char *aCommandName,
                                     nsICommandParams *aParams,
                                     nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    return NS_OK;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString tString;
  if (aParams) {
    nsCString s;
    nsresult rv = aParams->GetCStringValue(STATE_ATTRIBUTE, getter_Copies(s));
    if (NS_SUCCEEDED(rv))
      CopyASCIItoUTF16(s, tString);
    else
      aParams->GetStringValue(STATE_ATTRIBUTE, tString);
  }
  return SetState(htmlEditor, tString);
}

NS_IMETHODIMP
nsMultiStateCommand::GetCommandStateParams(const char *aCommandName,
                                           nsICommandParams *aParams,
                                           nsISupports *refCon)
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

nsParagraphStateCommand::nsParagraphStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsParagraphStateCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
    aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  }
  return rv;
}

nsresult
nsParagraphStateCommand::SetState(mozilla::HTMLEditor* aHTMLEditor,
                                  nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->SetParagraphFormat(newState);
}

nsFontFaceStateCommand::nsFontFaceStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsFontFaceStateCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
    aParams->SetCStringValue(STATE_ATTRIBUTE, NS_ConvertUTF16toUTF8(outStateString).get());
  }
  return rv;
}

nsresult
nsFontFaceStateCommand::SetState(mozilla::HTMLEditor* aHTMLEditor,
                                 nsString& newState)
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

nsFontSizeStateCommand::nsFontSizeStateCommand()
  : nsMultiStateCommand()
{
}

nsresult
nsFontSizeStateCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
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
nsFontSizeStateCommand::SetState(mozilla::HTMLEditor* aHTMLEditor,
                                 nsString& newState)
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

nsFontColorStateCommand::nsFontColorStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsFontColorStateCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  return NS_OK;
}

nsresult
nsFontColorStateCommand::SetState(mozilla::HTMLEditor* aHTMLEditor,
                                  nsString& newState)
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

nsHighlightColorStateCommand::nsHighlightColorStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsHighlightColorStateCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  return NS_OK;
}

nsresult
nsHighlightColorStateCommand::SetState(mozilla::HTMLEditor* aHTMLEditor,
                                       nsString& newState)
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
nsHighlightColorStateCommand::IsCommandEnabled(const char * aCommandName,
                                               nsISupports *refCon,
                                               bool *outCmdEnabled)
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

nsBackgroundColorStateCommand::nsBackgroundColorStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsBackgroundColorStateCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  return NS_OK;
}

nsresult
nsBackgroundColorStateCommand::SetState(mozilla::HTMLEditor* aHTMLEditor,
                                        nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->SetBackgroundColor(newState);
}

nsAlignCommand::nsAlignCommand()
: nsMultiStateCommand()
{
}

nsresult
nsAlignCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
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
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  return NS_OK;
}

nsresult
nsAlignCommand::SetState(mozilla::HTMLEditor* aHTMLEditor,
                         nsString& newState)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aHTMLEditor->Align(newState);
}

nsAbsolutePositioningCommand::nsAbsolutePositioningCommand()
: nsBaseStateUpdatingCommand(nsGkAtoms::_empty)
{
}

NS_IMETHODIMP
nsAbsolutePositioningCommand::IsCommandEnabled(const char * aCommandName,
                                               nsISupports *aCommandRefCon,
                                               bool *outCmdEnabled)
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
nsAbsolutePositioningCommand::GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                              nsICommandParams* aParams)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool isEnabled = aHTMLEditor->AbsolutePositioningEnabled();
  if (!isEnabled) {
    aParams->SetBooleanValue(STATE_MIXED,false);
    aParams->SetCStringValue(STATE_ATTRIBUTE, "");
    return NS_OK;
  }

  nsCOMPtr<nsINode> container;
  nsresult rv =
    aHTMLEditor->GetAbsolutelyPositionedSelectionContainer(
                   getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString outStateString;
  if (container) {
    outStateString.AssignLiteral("absolute");
  }

  aParams->SetBooleanValue(STATE_MIXED,false);
  aParams->SetCStringValue(STATE_ATTRIBUTE, NS_ConvertUTF16toUTF8(outStateString).get());
  return NS_OK;
}

nsresult
nsAbsolutePositioningCommand::ToggleState(mozilla::HTMLEditor* aHTMLEditor)
{
  if (NS_WARN_IF(!aHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsINode> container;
  nsresult rv =
    aHTMLEditor->GetAbsolutelyPositionedSelectionContainer(
                   getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, rv);

  return aHTMLEditor->AbsolutePositionSelection(!container);
}


NS_IMETHODIMP
nsDecreaseZIndexCommand::IsCommandEnabled(const char * aCommandName,
                                          nsISupports *refCon,
                                          bool *outCmdEnabled)
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

  int32_t z;
  nsresult rv = htmlEditor->GetElementZIndex(positionedElement, &z);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  *outCmdEnabled = (z > 0);
  return NS_OK;
}

NS_IMETHODIMP
nsDecreaseZIndexCommand::DoCommand(const char *aCommandName,
                                   nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return htmlEditor->RelativeChangeZIndex(-1);
}

NS_IMETHODIMP
nsDecreaseZIndexCommand::DoCommandParams(const char *aCommandName,
                                         nsICommandParams *aParams,
                                         nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsDecreaseZIndexCommand::GetCommandStateParams(const char *aCommandName,
                                               nsICommandParams *aParams,
                                               nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled = false;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->SetBooleanValue(STATE_ENABLED, enabled);
}

NS_IMETHODIMP
nsIncreaseZIndexCommand::IsCommandEnabled(const char * aCommandName,
                                          nsISupports *refCon,
                                          bool *outCmdEnabled)
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
nsIncreaseZIndexCommand::DoCommand(const char *aCommandName,
                                   nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }
  mozilla::HTMLEditor* htmlEditor = editor->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_FAILURE;
  }
  return htmlEditor->RelativeChangeZIndex(1);
}

NS_IMETHODIMP
nsIncreaseZIndexCommand::DoCommandParams(const char *aCommandName,
                                         nsICommandParams *aParams,
                                         nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsIncreaseZIndexCommand::GetCommandStateParams(const char *aCommandName,
                                               nsICommandParams *aParams,
                                               nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled = false;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->SetBooleanValue(STATE_ENABLED, enabled);
}

NS_IMETHODIMP
nsRemoveStylesCommand::IsCommandEnabled(const char * aCommandName,
                                        nsISupports *refCon,
                                        bool *outCmdEnabled)
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
nsRemoveStylesCommand::DoCommand(const char *aCommandName,
                                 nsISupports *refCon)
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
nsRemoveStylesCommand::DoCommandParams(const char *aCommandName,
                                       nsICommandParams *aParams,
                                       nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsRemoveStylesCommand::GetCommandStateParams(const char *aCommandName,
                                             nsICommandParams *aParams,
                                             nsISupports *refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

NS_IMETHODIMP
nsIncreaseFontSizeCommand::IsCommandEnabled(const char * aCommandName,
                                            nsISupports *refCon,
                                            bool *outCmdEnabled)
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
nsIncreaseFontSizeCommand::DoCommand(const char *aCommandName,
                                     nsISupports *refCon)
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
nsIncreaseFontSizeCommand::DoCommandParams(const char *aCommandName,
                                           nsICommandParams *aParams,
                                           nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsIncreaseFontSizeCommand::GetCommandStateParams(const char *aCommandName,
                                                 nsICommandParams *aParams,
                                                 nsISupports *refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

NS_IMETHODIMP
nsDecreaseFontSizeCommand::IsCommandEnabled(const char * aCommandName,
                                            nsISupports *refCon,
                                            bool *outCmdEnabled)
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
nsDecreaseFontSizeCommand::DoCommand(const char *aCommandName,
                                     nsISupports *refCon)
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
nsDecreaseFontSizeCommand::DoCommandParams(const char *aCommandName,
                                           nsICommandParams *aParams,
                                           nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}

NS_IMETHODIMP
nsDecreaseFontSizeCommand::GetCommandStateParams(const char *aCommandName,
                                                 nsICommandParams *aParams,
                                                 nsISupports *refCon)
{
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

NS_IMETHODIMP
nsInsertHTMLCommand::IsCommandEnabled(const char * aCommandName,
                                      nsISupports *refCon,
                                      bool *outCmdEnabled)
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
nsInsertHTMLCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
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
nsInsertHTMLCommand::DoCommandParams(const char *aCommandName,
                                     nsICommandParams *aParams,
                                     nsISupports *refCon)
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
nsInsertHTMLCommand::GetCommandStateParams(const char *aCommandName,
                                           nsICommandParams *aParams,
                                           nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED, outCmdEnabled);
}

NS_IMPL_ISUPPORTS_INHERITED0(nsInsertTagCommand, nsBaseComposerCommand)

nsInsertTagCommand::nsInsertTagCommand(nsAtom* aTagName)
: nsBaseComposerCommand()
, mTagName(aTagName)
{
  MOZ_ASSERT(mTagName);
}

nsInsertTagCommand::~nsInsertTagCommand()
{
}

NS_IMETHODIMP
nsInsertTagCommand::IsCommandEnabled(const char * aCommandName,
                                     nsISupports *refCon,
                                     bool *outCmdEnabled)
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
nsInsertTagCommand::DoCommand(const char *aCmdName, nsISupports *refCon)
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

  nsCOMPtr<nsIDOMElement> domElem;
  nsresult rv = htmlEditor->CreateElementWithDefaults(
    nsDependentAtomString(mTagName), getter_AddRefs(domElem));
  NS_ENSURE_SUCCESS(rv, rv);

  return htmlEditor->InsertElementAtSelection(domElem, true);
}

NS_IMETHODIMP
nsInsertTagCommand::DoCommandParams(const char *aCommandName,
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
  nsCString s;
  nsresult rv = aParams->GetCStringValue(STATE_ATTRIBUTE, getter_Copies(s));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString attrib;
  CopyASCIItoUTF16(s, attrib);

  if (attrib.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  // filter out tags we don't know how to insert
  nsAutoString attributeType;
  if (mTagName == nsGkAtoms::a) {
    attributeType.AssignLiteral("href");
  } else if (mTagName == nsGkAtoms::img) {
    attributeType.AssignLiteral("src");
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsIDOMElement> domElem;
  rv = htmlEditor->CreateElementWithDefaults(nsDependentAtomString(mTagName),
                                             getter_AddRefs(domElem));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = domElem->SetAttribute(attributeType, attrib);
  NS_ENSURE_SUCCESS(rv, rv);

  // do actual insertion
  if (mTagName == nsGkAtoms::a) {
    return htmlEditor->InsertLinkAroundSelection(domElem);
  }
  return htmlEditor->InsertElementAtSelection(domElem, true);
}

NS_IMETHODIMP
nsInsertTagCommand::GetCommandStateParams(const char *aCommandName,
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

nsresult
GetListState(mozilla::HTMLEditor* aHTMLEditor,
             bool* aMixed,
             nsAString& aLocalName)
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

nsresult
RemoveOneProperty(mozilla::HTMLEditor* aHTMLEditor,
                  const nsAString& aProp)
{
  MOZ_ASSERT(aHTMLEditor);

  /// XXX Hack alert! Look in nsIEditProperty.h for this
  RefPtr<nsAtom> styleAtom = NS_Atomize(aProp);
  NS_ENSURE_TRUE(styleAtom, NS_ERROR_OUT_OF_MEMORY);

  return aHTMLEditor->RemoveInlineProperty(styleAtom, nullptr);
}


// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
nsresult
RemoveTextProperty(mozilla::HTMLEditor* aHTMLEditor,
                   const nsAString& aProp)
{
  MOZ_ASSERT(aHTMLEditor);

  if (aProp.LowerCaseEqualsLiteral("all")) {
    return aHTMLEditor->RemoveAllInlineProperties();
  }

  return RemoveOneProperty(aHTMLEditor, aProp);
}

// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
nsresult
SetTextProperty(mozilla::HTMLEditor* aHTMLEditor,
                const nsAString& aProp)
{
  MOZ_ASSERT(aHTMLEditor);

  /// XXX Hack alert! Look in nsIEditProperty.h for this
  RefPtr<nsAtom> styleAtom = NS_Atomize(aProp);
  NS_ENSURE_TRUE(styleAtom, NS_ERROR_OUT_OF_MEMORY);

  return aHTMLEditor->SetInlineProperty(styleAtom, nullptr, EmptyString());
}
