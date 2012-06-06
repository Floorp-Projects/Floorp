/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLAbsPosEditor.h"

#include "nsIDOMElement.h"
#include "nsIAtom.h"
#include "nsGkAtoms.h"

#include "nsIClipboard.h"

#include "nsCOMPtr.h"

#include "nsComposerCommands.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsICommandParams.h"
#include "nsComponentManagerUtils.h"

#include "mozilla/Assertions.h"

//prototype
nsresult GetListState(nsIHTMLEditor* aEditor, bool* aMixed,
                      nsAString& aLocalName);
nsresult RemoveOneProperty(nsIHTMLEditor* aEditor, const nsAString& aProp);
nsresult RemoveTextProperty(nsIHTMLEditor* aEditor, const nsAString& aProp);
nsresult SetTextProperty(nsIHTMLEditor *aEditor, const nsAString& aProp);


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

NS_IMPL_ISUPPORTS1(nsBaseComposerCommand, nsIControllerCommand)


nsBaseStateUpdatingCommand::nsBaseStateUpdatingCommand(nsIAtom* aTagName)
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
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsBaseStateUpdatingCommand::DoCommand(const char *aCommandName,
                                      nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_NOT_INITIALIZED);

  return ToggleState(editor);
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
  if (editor)
    return GetCurrentState(editor, aParams);

  return NS_OK;
}

NS_IMETHODIMP
nsPasteNoFormattingCommand::IsCommandEnabled(const char * aCommandName, 
                                             nsISupports *refCon, 
                                             bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = false;

  // This command is only implemented by nsIHTMLEditor, since
  //  pasting in a plaintext editor automatically only supplies 
  //  "unformatted" text
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_NOT_IMPLEMENTED);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(htmlEditor);
  NS_ENSURE_TRUE(editor, NS_ERROR_INVALID_ARG);

  return editor->CanPaste(nsIClipboard::kGlobalClipboard, outCmdEnabled);
}


NS_IMETHODIMP
nsPasteNoFormattingCommand::DoCommand(const char *aCommandName,
                                      nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_NOT_IMPLEMENTED);

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

nsStyleUpdatingCommand::nsStyleUpdatingCommand(nsIAtom* aTagName)
: nsBaseStateUpdatingCommand(aTagName)
{
}

nsresult
nsStyleUpdatingCommand::GetCurrentState(nsIEditor *aEditor, 
                                        nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_NOT_INITIALIZED);
  
  bool firstOfSelectionHasProp = false;
  bool anyOfSelectionHasProp = false;
  bool allOfSelectionHasProp = false;

  nsresult rv = htmlEditor->GetInlineProperty(mTagName, EmptyString(),
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
nsStyleUpdatingCommand::ToggleState(nsIEditor *aEditor)
{
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_NO_INTERFACE);

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
    rv = GetCurrentState(aEditor, params);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = params->GetBooleanValue(STATE_ALL, &doTagRemoval);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (doTagRemoval) {
    // Also remove equivalent properties (bug 317093)
    if (mTagName == nsGkAtoms::b) {
      rv = RemoveTextProperty(htmlEditor, NS_LITERAL_STRING("strong"));
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (mTagName == nsGkAtoms::i) {
      rv = RemoveTextProperty(htmlEditor, NS_LITERAL_STRING("em"));
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (mTagName == nsGkAtoms::strike) {
      rv = RemoveTextProperty(htmlEditor, NS_LITERAL_STRING("s"));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = RemoveTextProperty(htmlEditor, nsDependentAtomString(mTagName));
  } else {
    // Superscript and Subscript styles are mutually exclusive
    aEditor->BeginTransaction();

    nsDependentAtomString tagName(mTagName);
    if (mTagName == nsGkAtoms::sub || mTagName == nsGkAtoms::sup) {
      rv = RemoveTextProperty(htmlEditor, tagName);
    }
    if (NS_SUCCEEDED(rv))
      rv = SetTextProperty(htmlEditor, tagName);

    aEditor->EndTransaction();
  }

  return rv;
}

nsListCommand::nsListCommand(nsIAtom* aTagName)
: nsBaseStateUpdatingCommand(aTagName)
{
}

nsresult
nsListCommand::GetCurrentState(nsIEditor* aEditor, nsICommandParams* aParams)
{
  NS_ASSERTION(aEditor, "Need editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_NO_INTERFACE);

  bool bMixed;
  nsAutoString localName;
  nsresult rv = GetListState(htmlEditor, &bMixed, localName);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList = mTagName->Equals(localName);
  aParams->SetBooleanValue(STATE_ALL, !bMixed && inList);
  aParams->SetBooleanValue(STATE_MIXED, bMixed);
  aParams->SetBooleanValue(STATE_ENABLED, true);
  return NS_OK;
}

nsresult
nsListCommand::ToggleState(nsIEditor *aEditor)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(editor, NS_NOINTERFACE);

  nsresult rv;
  nsCOMPtr<nsICommandParams> params =
      do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
  if (NS_FAILED(rv) || !params)
    return rv;

  rv = GetCurrentState(aEditor, params);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList;
  rv = params->GetBooleanValue(STATE_ALL,&inList);
  NS_ENSURE_SUCCESS(rv, rv);

  nsDependentAtomString listType(mTagName);
  if (inList) {
    rv = editor->RemoveList(listType);    
  } else {
    rv = editor->MakeOrChangeList(listType, false, EmptyString());
  }
  
  return rv;
}

nsListItemCommand::nsListItemCommand(nsIAtom* aTagName)
: nsBaseStateUpdatingCommand(aTagName)
{
}

nsresult
nsListItemCommand::GetCurrentState(nsIEditor* aEditor,
                                   nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need editor here");
  // 39584
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_NOINTERFACE);

  bool bMixed, bLI, bDT, bDD;
  nsresult rv = htmlEditor->GetListItemState(&bMixed, &bLI, &bDT, &bDD);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inList = false;
  if (!bMixed)
  {
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
nsListItemCommand::ToggleState(nsIEditor *aEditor)
{
  NS_ASSERTION(aEditor, "Need editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_NOT_INITIALIZED);

  bool inList;
  // Need to use mTagName????
  nsresult rv;
  nsCOMPtr<nsICommandParams> params =
      do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
  if (NS_FAILED(rv) || !params)
    return rv;
  rv = GetCurrentState(aEditor, params);
  rv = params->GetBooleanValue(STATE_ALL,&inList);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (inList) {
    // To remove a list, first get what kind of list we're in
    bool bMixed;
    nsAutoString localName;
    rv = GetListState(htmlEditor, &bMixed, localName);
    NS_ENSURE_SUCCESS(rv, rv); 
    if (localName.IsEmpty() || bMixed) {
      return rv;
    }
    return htmlEditor->RemoveList(localName);
  }

  // Set to the requested paragraph type
  //XXX Note: This actually doesn't work for "LI",
  //    but we currently don't use this for non DL lists anyway.
  // Problem: won't this replace any current block paragraph style?
  return htmlEditor->SetParagraphFormat(nsDependentAtomString(mTagName));
}

NS_IMETHODIMP
nsRemoveListCommand::IsCommandEnabled(const char * aCommandName,
                                      nsISupports *refCon,
                                      bool *outCmdEnabled)
{
  *outCmdEnabled = false;
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_OK);

  bool isEditable = false;
  nsresult rv = editor->GetIsSelectionEditable(&isEditable);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isEditable) {
    return NS_OK;
  }

  // It is enabled if we are in any list type
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_NO_INTERFACE);

  bool bMixed;
  nsAutoString localName;
  rv = GetListState(htmlEditor, &bMixed, localName);
  NS_ENSURE_SUCCESS(rv, rv);

  *outCmdEnabled = bMixed || !localName.IsEmpty();
  return NS_OK;
}


NS_IMETHODIMP
nsRemoveListCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    // This removes any list type
    rv = editor->RemoveList(EmptyString());
  }

  return rv;  
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
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsIndentCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    rv = editor->Indent(NS_LITERAL_STRING("indent"));
  }
  
  return rv;  
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
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(refCon);
  if (editor && htmlEditor)
  {
    bool canIndent, isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return htmlEditor->GetIndentState(&canIndent, outCmdEnabled);
  }

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsOutdentCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(refCon);
  if (htmlEditor)
    return htmlEditor->Indent(NS_LITERAL_STRING("outdent"));
  
  return NS_OK;  
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
  // should be disabled sometimes, like if the current selection is an image
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
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

  nsresult rv = NS_OK;
  if (editor)
  {
      nsAutoString tString;

      if (aParams) {
        nsXPIDLCString s;
        rv = aParams->GetCStringValue(STATE_ATTRIBUTE, getter_Copies(s));
        if (NS_SUCCEEDED(rv))
          tString.AssignWithConversion(s);
        else
          rv = aParams->GetStringValue(STATE_ATTRIBUTE, tString);
      }

      rv = SetState(editor, tString);
  }
  
  return rv;  
}

NS_IMETHODIMP
nsMultiStateCommand::GetCommandStateParams(const char *aCommandName,
                                           nsICommandParams *aParams,
                                           nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  nsresult rv = NS_OK;
  if (editor)
  {
      rv = GetCurrentState(editor, aParams);
  }
  return rv;
}

nsParagraphStateCommand::nsParagraphStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsParagraphStateCommand::GetCurrentState(nsIEditor *aEditor,
                                         nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = htmlEditor->GetParagraphState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv))
  {
    nsCAutoString tOutStateString;
    tOutStateString.AssignWithConversion(outStateString);
    aParams->SetBooleanValue(STATE_MIXED,outMixed);
    aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  }
  return rv;
}


nsresult
nsParagraphStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  return htmlEditor->SetParagraphFormat(newState);
}

nsFontFaceStateCommand::nsFontFaceStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsFontFaceStateCommand::GetCurrentState(nsIEditor *aEditor,
                                        nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  nsAutoString outStateString;
  bool outMixed;
  nsresult rv = htmlEditor->GetFontFaceState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv))
  {
    aParams->SetBooleanValue(STATE_MIXED,outMixed);
    aParams->SetCStringValue(STATE_ATTRIBUTE, NS_ConvertUTF16toUTF8(outStateString).get());
  }
  return rv;
}


nsresult
nsFontFaceStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);
  
  if (newState.EqualsLiteral("tt")) {
    // The old "teletype" attribute
    nsresult rv = htmlEditor->SetInlineProperty(nsGkAtoms::tt, EmptyString(),
                                                EmptyString());
    NS_ENSURE_SUCCESS(rv, rv);
    // Clear existing font face
    return htmlEditor->RemoveInlineProperty(nsGkAtoms::font,
                                            NS_LITERAL_STRING("face"));
  }

  // Remove any existing TT nodes
  nsresult rv = htmlEditor->RemoveInlineProperty(nsGkAtoms::tt, EmptyString());
  NS_ENSURE_SUCCESS(rv, rv);

  if (newState.IsEmpty() || newState.EqualsLiteral("normal")) {
    return htmlEditor->RemoveInlineProperty(nsGkAtoms::font,
                                            NS_LITERAL_STRING("face"));
  }

  return htmlEditor->SetInlineProperty(nsGkAtoms::font,
                                       NS_LITERAL_STRING("face"), newState);
}

nsFontSizeStateCommand::nsFontSizeStateCommand()
  : nsMultiStateCommand()
{
}

//  nsCAutoString tOutStateString;
//  tOutStateString.AssignWithConversion(outStateString);
nsresult
nsFontSizeStateCommand::GetCurrentState(nsIEditor *aEditor,
                                        nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_INVALID_ARG);

  nsAutoString outStateString;
  nsCOMPtr<nsIAtom> fontAtom = do_GetAtom("font");
  bool firstHas, anyHas, allHas;
  nsresult rv = htmlEditor->GetInlinePropertyWithAttrValue(fontAtom,
                                         NS_LITERAL_STRING("size"),
                                         EmptyString(),
                                         &firstHas, &anyHas, &allHas,
                                         outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString tOutStateString;
  tOutStateString.AssignWithConversion(outStateString);
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
nsFontSizeStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_INVALID_ARG);

  if (!newState.IsEmpty() &&
      !newState.EqualsLiteral("normal") &&
      !newState.EqualsLiteral("medium")) {
    return htmlEditor->SetInlineProperty(nsGkAtoms::font,
                                         NS_LITERAL_STRING("size"), newState);
  }

  // remove any existing font size, big or small
  nsresult rv = htmlEditor->RemoveInlineProperty(nsGkAtoms::font,
                                                 NS_LITERAL_STRING("size"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = htmlEditor->RemoveInlineProperty(nsGkAtoms::big, EmptyString());
  NS_ENSURE_SUCCESS(rv, rv);

  return htmlEditor->RemoveInlineProperty(nsGkAtoms::small, EmptyString());
}

nsFontColorStateCommand::nsFontColorStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsFontColorStateCommand::GetCurrentState(nsIEditor *aEditor,
                                         nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = htmlEditor->GetFontColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString tOutStateString;
  tOutStateString.AssignWithConversion(outStateString);
  aParams->SetBooleanValue(STATE_MIXED, outMixed);
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  return NS_OK;
}

nsresult
nsFontColorStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);
  
  if (newState.IsEmpty() || newState.EqualsLiteral("normal")) {
    return htmlEditor->RemoveInlineProperty(nsGkAtoms::font,
                                            NS_LITERAL_STRING("color"));
  }
  
  return htmlEditor->SetInlineProperty(nsGkAtoms::font,
                                       NS_LITERAL_STRING("color"), newState);
}

nsHighlightColorStateCommand::nsHighlightColorStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsHighlightColorStateCommand::GetCurrentState(nsIEditor *aEditor,
                                              nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv = htmlEditor->GetHighlightColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString tOutStateString;
  tOutStateString.AssignWithConversion(outStateString);
  aParams->SetBooleanValue(STATE_MIXED, outMixed);
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  return NS_OK;
}

nsresult
nsHighlightColorStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  if (newState.IsEmpty() || newState.EqualsLiteral("normal")) {
    return htmlEditor->RemoveInlineProperty(nsGkAtoms::font,
                                            NS_LITERAL_STRING("bgcolor"));
  }

  return htmlEditor->SetInlineProperty(nsGkAtoms::font,
                                       NS_LITERAL_STRING("bgcolor"),
                                       newState);
}

NS_IMETHODIMP
nsHighlightColorStateCommand::IsCommandEnabled(const char * aCommandName,
                                               nsISupports *refCon,
                                               bool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


nsBackgroundColorStateCommand::nsBackgroundColorStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsBackgroundColorStateCommand::GetCurrentState(nsIEditor *aEditor,
                                               nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  bool outMixed;
  nsAutoString outStateString;
  nsresult rv =  htmlEditor->GetBackgroundColorState(&outMixed, outStateString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString tOutStateString;
  tOutStateString.AssignWithConversion(outStateString);
  aParams->SetBooleanValue(STATE_MIXED, outMixed);
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  return NS_OK;
}

nsresult
nsBackgroundColorStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  return htmlEditor->SetBackgroundColor(newState);
}

nsAlignCommand::nsAlignCommand()
: nsMultiStateCommand()
{
}

nsresult
nsAlignCommand::GetCurrentState(nsIEditor *aEditor, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);
 
  nsIHTMLEditor::EAlignment firstAlign;
  bool outMixed;
  nsresult rv = htmlEditor->GetAlignment(&outMixed, &firstAlign);
  
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsAutoString outStateString;
  switch (firstAlign)
  {
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
  nsCAutoString tOutStateString;
  tOutStateString.AssignWithConversion(outStateString);
  aParams->SetBooleanValue(STATE_MIXED,outMixed);
  aParams->SetCStringValue(STATE_ATTRIBUTE, tOutStateString.get());
  return NS_OK;
}

nsresult
nsAlignCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  return htmlEditor->Align(newState);
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
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  nsCOMPtr<nsIHTMLAbsPosEditor> htmlEditor = do_QueryInterface(aCommandRefCon);
  if (htmlEditor)
  {
    bool isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return htmlEditor->GetAbsolutePositioningEnabled(outCmdEnabled);
  }

  *outCmdEnabled = false;
  return NS_OK;
}

nsresult
nsAbsolutePositioningCommand::GetCurrentState(nsIEditor *aEditor, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  
  nsCOMPtr<nsIHTMLAbsPosEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  bool isEnabled;
  htmlEditor->GetAbsolutePositioningEnabled(&isEnabled);
  if (!isEnabled) {
    aParams->SetBooleanValue(STATE_MIXED,false);
    aParams->SetCStringValue(STATE_ATTRIBUTE, "");
    return NS_OK;
  }

  nsCOMPtr<nsIDOMElement>  elt;
  nsresult rv = htmlEditor->GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(elt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString outStateString;
  if (elt)
    outStateString.AssignLiteral("absolute");

  aParams->SetBooleanValue(STATE_MIXED,false);
  aParams->SetCStringValue(STATE_ATTRIBUTE, NS_ConvertUTF16toUTF8(outStateString).get());
  return NS_OK;
}

nsresult
nsAbsolutePositioningCommand::ToggleState(nsIEditor *aEditor)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  
  nsCOMPtr<nsIHTMLAbsPosEditor> htmlEditor = do_QueryInterface(aEditor);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMElement> elt;
  nsresult rv = htmlEditor->GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(elt));
  NS_ENSURE_SUCCESS(rv, rv);

  return htmlEditor->AbsolutePositionSelection(!elt);
}


NS_IMETHODIMP
nsDecreaseZIndexCommand::IsCommandEnabled(const char * aCommandName,
                                          nsISupports *refCon,
                                          bool *outCmdEnabled)
{
  nsCOMPtr<nsIHTMLAbsPosEditor> htmlEditor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  htmlEditor->GetAbsolutePositioningEnabled(outCmdEnabled);
  if (!(*outCmdEnabled))
    return NS_OK;

  nsCOMPtr<nsIDOMElement> positionedElement;
  htmlEditor->GetPositionedElement(getter_AddRefs(positionedElement));
  *outCmdEnabled = false;
  if (positionedElement) {
    PRInt32 z;
    nsresult res = htmlEditor->GetElementZIndex(positionedElement, &z);
    NS_ENSURE_SUCCESS(res, res);
    *outCmdEnabled = (z > 0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDecreaseZIndexCommand::DoCommand(const char *aCommandName,
                                   nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLAbsPosEditor> htmlEditor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_NOT_IMPLEMENTED);

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
  nsCOMPtr<nsIHTMLAbsPosEditor> htmlEditor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_FAILURE);

  htmlEditor->GetAbsolutePositioningEnabled(outCmdEnabled);
  if (!(*outCmdEnabled))
    return NS_OK;

  nsCOMPtr<nsIDOMElement> positionedElement;
  htmlEditor->GetPositionedElement(getter_AddRefs(positionedElement));
  *outCmdEnabled = (nsnull != positionedElement);
  return NS_OK;
}

NS_IMETHODIMP
nsIncreaseZIndexCommand::DoCommand(const char *aCommandName,
                                   nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLAbsPosEditor> htmlEditor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(htmlEditor, NS_ERROR_NOT_IMPLEMENTED);

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
  // test if we have any styles?
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}



NS_IMETHODIMP
nsRemoveStylesCommand::DoCommand(const char *aCommandName,
                                 nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    rv = editor->RemoveAllInlineProperties();
  }
  
  return rv;  
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
  // test if we are at max size?
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsIncreaseFontSizeCommand::DoCommand(const char *aCommandName,
                                     nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    rv = editor->IncreaseFontSize();
  }
  
  return rv;  
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
  // test if we are at min size?
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsDecreaseFontSizeCommand::DoCommand(const char *aCommandName,
                                     nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    rv = editor->DecreaseFontSize();
  }
  
  return rv;  
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
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsInsertHTMLCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInsertHTMLCommand::DoCommandParams(const char *aCommandName,
                                     nsICommandParams *aParams,
                                     nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_NOT_IMPLEMENTED);

  // Get HTML source string to insert from command params
  nsAutoString html;
  nsresult rv = aParams->GetStringValue(STATE_DATA, html);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!html.IsEmpty())
    return editor->InsertHTML(html);

  return NS_OK;
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

nsInsertTagCommand::nsInsertTagCommand(nsIAtom* aTagName)
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
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


// corresponding STATE_ATTRIBUTE is: src (img) and href (a) 
NS_IMETHODIMP
nsInsertTagCommand::DoCommand(const char *aCmdName, nsISupports *refCon)
{
  NS_ENSURE_TRUE(mTagName == nsGkAtoms::hr, NS_ERROR_NOT_IMPLEMENTED);

  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_NOT_IMPLEMENTED);

  nsCOMPtr<nsIDOMElement> domElem;
  nsresult rv = editor->CreateElementWithDefaults(
    nsDependentAtomString(mTagName), getter_AddRefs(domElem));
  NS_ENSURE_SUCCESS(rv, rv);

  return editor->InsertElementAtSelection(domElem, true);
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

  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_NOT_IMPLEMENTED);

  // do we have an href to use for creating link?
  nsXPIDLCString s;
  nsresult rv = aParams->GetCStringValue(STATE_ATTRIBUTE, getter_Copies(s));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString attrib; attrib.AssignWithConversion(s);

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
  rv = editor->CreateElementWithDefaults(nsDependentAtomString(mTagName),
                                         getter_AddRefs(domElem));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = domElem->SetAttribute(attributeType, attrib);
  NS_ENSURE_SUCCESS(rv, rv);

  // do actual insertion
  if (mTagName == nsGkAtoms::a)
    return editor->InsertLinkAroundSelection(domElem);

  return editor->InsertElementAtSelection(domElem, true);
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
GetListState(nsIHTMLEditor* aEditor, bool* aMixed, nsAString& aLocalName)
{
  MOZ_ASSERT(aEditor);
  MOZ_ASSERT(aMixed);

  *aMixed = false;
  aLocalName.Truncate();

  bool bOL, bUL, bDL;
  nsresult rv = aEditor->GetListState(aMixed, &bOL, &bUL, &bDL);
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
RemoveOneProperty(nsIHTMLEditor* aEditor, const nsAString& aProp)
{
  MOZ_ASSERT(aEditor);

  /// XXX Hack alert! Look in nsIEditProperty.h for this
  nsCOMPtr<nsIAtom> styleAtom = do_GetAtom(aProp);
  NS_ENSURE_TRUE(styleAtom, NS_ERROR_OUT_OF_MEMORY);

  return aEditor->RemoveInlineProperty(styleAtom, EmptyString());
}


// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
nsresult
RemoveTextProperty(nsIHTMLEditor* aEditor, const nsAString& aProp)
{
  MOZ_ASSERT(aEditor);

  if (aProp.LowerCaseEqualsLiteral("all")) {
    return aEditor->RemoveAllInlineProperties();
  }
  
  return RemoveOneProperty(aEditor, aProp);
}

// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
nsresult
SetTextProperty(nsIHTMLEditor* aEditor, const nsAString& aProp)
{
  MOZ_ASSERT(aEditor);

  /// XXX Hack alert! Look in nsIEditProperty.h for this
  nsCOMPtr<nsIAtom> styleAtom = do_GetAtom(aProp);
  NS_ENSURE_TRUE(styleAtom, NS_ERROR_OUT_OF_MEMORY);

  return aEditor->SetInlineProperty(styleAtom, EmptyString(), EmptyString());
}
