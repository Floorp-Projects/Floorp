/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsString.h"
#include "nsMaiInterfaceEditableText.h"

G_BEGIN_DECLS

static void interfaceInitCB(AtkEditableTextIface *aIface);

/* editabletext interface callbacks */
static gboolean setRunAttributesCB(AtkEditableText *aText,
                                   AtkAttributeSet *aAttribSet,
                                   gint aStartOffset,
                                   gint aEndOffset);
static void setTextContentsCB(AtkEditableText *aText, const gchar *aString);
static void insertTextCB(AtkEditableText *aText,
                         const gchar *aString, gint aLength, gint *aPosition);
static void copyTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos);
static void cutTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos);
static void deleteTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos);
static void pasteTextCB(AtkEditableText *aText, gint aPosition);

G_END_DECLS

MaiInterfaceEditableText::MaiInterfaceEditableText(nsAccessibleWrap *aAccWrap):
    MaiInterface(aAccWrap)
{
}

MaiInterfaceEditableText::~MaiInterfaceEditableText()
{
}

MaiInterfaceType
MaiInterfaceEditableText::GetType()
{
    return MAI_INTERFACE_EDITABLE_TEXT;
}

const GInterfaceInfo *
MaiInterfaceEditableText::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_editabletext_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_editabletext_info;
}

/* statics */

void
interfaceInitCB(AtkEditableTextIface *aIface)
{
    NS_ASSERTION(aIface, "Invalid aIface");
    if (!aIface)
        return;

    aIface->set_run_attributes = setRunAttributesCB;
    aIface->set_text_contents = setTextContentsCB;
    aIface->insert_text = insertTextCB;
    aIface->copy_text = copyTextCB;
    aIface->cut_text = cutTextCB;
    aIface->delete_text = deleteTextCB;
    aIface->paste_text = pasteTextCB;
}

/* static, callbacks for atkeditabletext virutal functions */

gboolean
setRunAttributesCB(AtkEditableText *aText, AtkAttributeSet *aAttribSet,
                   gint aStartOffset, gint aEndOffset)

{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, FALSE);

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

void
setTextContentsCB(AtkEditableText *aText, const gchar *aString)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    MAI_LOG_DEBUG(("EditableText: setTextContentsCB, aString=%s", aString));

    NS_ConvertUTF8toUCS2 strContent(aString);
    nsresult rv = accText->SetTextContents(strContent);

    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::SetTextContents, failed\n");
}

void
insertTextCB(AtkEditableText *aText,
             const gchar *aString, gint aLength, gint *aPosition)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    NS_ConvertUTF8toUCS2 strContent(aString);

    // interface changed in nsIAccessibleEditabelText.idl ???
    //
    // PRInt32 pos = *aPosition;
    // nsresult rv = accText->InsertText(strContent, aLength, &pos);
    // *aPosition = pos;

    nsresult rv = accText->InsertText(strContent, *aPosition);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::InsertText, failed\n");

    MAI_LOG_DEBUG(("EditableText: insert aString=%s, aLength=%d, aPosition=%d",
                   aString, aLength, *aPosition));
}

void
copyTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    MAI_LOG_DEBUG(("EditableText: copyTextCB, aStartPos=%d, aEndPos=%d",
                   aStartPos, aEndPos));
    nsresult rv = accText->CopyText(aStartPos, aEndPos);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::CopyText, failed\n");
}

void
cutTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;
    MAI_LOG_DEBUG(("EditableText: cutTextCB, aStartPos=%d, aEndPos=%d",
                   aStartPos, aEndPos));
    nsresult rv = accText->CutText(aStartPos, aEndPos);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::CutText, failed\n");
}

void
deleteTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    MAI_LOG_DEBUG(("EditableText: deleteTextCB, aStartPos=%d, aEndPos=%d",
                   aStartPos, aEndPos));
    nsresult rv = accText->DeleteText(aStartPos, aEndPos);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::DeleteText, failed\n");
}

void
pasteTextCB(AtkEditableText *aText, gint aPosition)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return;

    nsCOMPtr<nsIAccessibleEditableText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    MAI_LOG_DEBUG(("EditableText: pasteTextCB, aPosition=%d", aPosition));
    nsresult rv = accText->PasteText(aPosition);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::PasteText, failed\n");
}
