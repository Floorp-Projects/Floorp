/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Evan Yan (evan.yan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAccessibleWrap.h"
#ifdef USE_ATK_TYPE_DOCUMENT
#include "nsMaiInterfaceDocument.h"

const char *const kDocTypeName = "W3C-doctype";
const char *const kDocUrlName = "DocURL";

void
documentInterfaceInitCB(AtkDocumentIface *aIface)
{
    NS_ASSERTION(aIface, "Invalid Interface");
    if(!aIface)
        return;

    /*
     * We don't support get_document, get_locale and set_attribute right now.
     */
    aIface->get_document_type = getDocumentTypeCB;
    aIface->get_document_attributes = getDocumentAttributesCB;
    aIface->get_document_attribute_value = getDocumentAttributeValueCB;
}

const gchar *
getDocumentTypeCB(AtkDocument *aDocument)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aDocument));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleDocument> accDocument;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleDocument),
                            getter_AddRefs(accDocument));
    NS_ENSURE_TRUE(accDocument, nsnull);

    nsAutoString aMimeType;
    nsresult rv = accDocument->GetMimeType(aMimeType);
    NS_ENSURE_SUCCESS(rv, nsnull);
    return nsAccessibleWrap::ReturnString(aMimeType);
}

AtkAttributeSet *
getDocumentAttributesCB(AtkDocument *aDocument)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aDocument));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleDocument> accDocument;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleDocument),
                            getter_AddRefs(accDocument));
    NS_ENSURE_TRUE(accDocument, nsnull);

    // according to atkobject.h, AtkAttributeSet is a GSList
    GSList *attributes = nsnull;

    nsAutoString aURL, aW3CDocType;
    nsresult rv = accDocument->GetURL(aURL);
    if (NS_SUCCEEDED(rv)) {
        // libspi will free these
        AtkAttribute *attrURL = (AtkAttribute *)g_malloc(sizeof(AtkAttribute));
        attrURL->name = g_strdup(kDocUrlName);
        attrURL->value = g_strdup(NS_ConvertUTF16toUTF8(aURL).get());
        attributes = g_slist_prepend(attributes, attrURL);
    }

    rv = accDocument->GetDocType(aW3CDocType);
    if (NS_SUCCEEDED(rv)) {
        // libspi will free these
        AtkAttribute *attrDocType = (AtkAttribute *)g_malloc(sizeof(AtkAttribute));
        attrDocType->name = g_strdup(kDocTypeName);
        attrDocType->value = g_strdup(NS_ConvertUTF16toUTF8(aW3CDocType).get());
        attributes = g_slist_prepend(attributes, attrDocType);
    }
    
    return attributes;
}

const gchar *
getDocumentAttributeValueCB(AtkDocument *aDocument,
                            const gchar *aAttrName)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aDocument));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleDocument> accDocument;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleDocument),
                            getter_AddRefs(accDocument));
    NS_ENSURE_TRUE(accDocument, nsnull);

    nsresult rv;
    nsAutoString attrValue;
    if (!g_ascii_strcasecmp(aAttrName, kDocTypeName)) {
        rv = accDocument->GetDocType(attrValue);
        NS_ENSURE_SUCCESS(rv, nsnull);
    }
    else if (!g_ascii_strcasecmp(aAttrName, kDocUrlName)) {
        rv = accDocument->GetURL(attrValue);
        NS_ENSURE_SUCCESS(rv, nsnull);
    }
    else {
        return nsnull;
    }
    return nsAccessibleWrap::ReturnString(attrValue);
}
#endif /* USE_ATK_TYPE_DOCUMENT */
