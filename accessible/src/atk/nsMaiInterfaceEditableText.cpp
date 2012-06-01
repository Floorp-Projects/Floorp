/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "HyperTextAccessible.h"
#include "nsMai.h"

#include "nsString.h"

extern "C" {

static gboolean
setRunAttributesCB(AtkEditableText *aText, AtkAttributeSet *aAttribSet,
                   gint aStartOffset, gint aEndOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return FALSE;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, FALSE);

    nsCOMPtr<nsISupports> attrSet;
    /* how to insert attributes into nsISupports ??? */

    nsresult rv = accText->SetAttributes(aStartOffset, aEndOffset,
                                         attrSet);
    return NS_FAILED(rv) ? FALSE : TRUE;
}

static void
setTextContentsCB(AtkEditableText *aText, const gchar *aString)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    MAI_LOG_DEBUG(("EditableText: setTextContentsCB, aString=%s", aString));

    NS_ConvertUTF8toUTF16 strContent(aString);
    accText->SetTextContents(strContent);
}

static void
insertTextCB(AtkEditableText *aText,
             const gchar *aString, gint aLength, gint *aPosition)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    NS_ConvertUTF8toUTF16 strContent(aString, aLength);

    // interface changed in nsIAccessibleEditableText.idl ???
    //
    // PRInt32 pos = *aPosition;
    // nsresult rv = accText->InsertText(strContent, aLength, &pos);
    // *aPosition = pos;

    accText->InsertText(strContent, *aPosition);

    MAI_LOG_DEBUG(("EditableText: insert aString=%s, aLength=%d, aPosition=%d",
                   aString, aLength, *aPosition));
}

static void
copyTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    MAI_LOG_DEBUG(("EditableText: copyTextCB, aStartPos=%d, aEndPos=%d",
                   aStartPos, aEndPos));
    accText->CopyText(aStartPos, aEndPos);
}

static void
cutTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;
    MAI_LOG_DEBUG(("EditableText: cutTextCB, aStartPos=%d, aEndPos=%d",
                   aStartPos, aEndPos));
    accText->CutText(aStartPos, aEndPos);
}

static void
deleteTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    MAI_LOG_DEBUG(("EditableText: deleteTextCB, aStartPos=%d, aEndPos=%d",
                   aStartPos, aEndPos));
    accText->DeleteText(aStartPos, aEndPos);
}

static void
pasteTextCB(AtkEditableText *aText, gint aPosition)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    MAI_LOG_DEBUG(("EditableText: pasteTextCB, aPosition=%d", aPosition));
    accText->PasteText(aPosition);
}
}

void
editableTextInterfaceInitCB(AtkEditableTextIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid aIface");
  if (NS_UNLIKELY(!aIface))
    return;

  aIface->set_run_attributes = setRunAttributesCB;
  aIface->set_text_contents = setTextContentsCB;
  aIface->insert_text = insertTextCB;
  aIface->copy_text = copyTextCB;
  aIface->cut_text = cutTextCB;
  aIface->delete_text = deleteTextCB;
  aIface->paste_text = pasteTextCB;
}
