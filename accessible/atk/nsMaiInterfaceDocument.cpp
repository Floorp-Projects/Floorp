/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "LocalAccessible-inl.h"
#include "AccessibleWrap.h"
#include "DocAccessible.h"
#include "nsAccUtils.h"
#include "nsMai.h"
#include "RemoteAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/Likely.h"

using namespace mozilla::a11y;

static const char* const kDocUrlName = "DocURL";
static const char* const kMimeTypeName = "MimeType";

// below functions are vfuncs on an ATK  interface so they need to be C call
extern "C" {

static const gchar* getDocumentLocaleCB(AtkDocument* aDocument);
static AtkAttributeSet* getDocumentAttributesCB(AtkDocument* aDocument);
static const gchar* getDocumentAttributeValueCB(AtkDocument* aDocument,
                                                const gchar* aAttrName);

void documentInterfaceInitCB(AtkDocumentIface* aIface) {
  NS_ASSERTION(aIface, "Invalid Interface");
  if (MOZ_UNLIKELY(!aIface)) return;

  /*
   * We don't support get_document or set_attribute right now.
   */
  aIface->get_document_attributes = getDocumentAttributesCB;
  aIface->get_document_attribute_value = getDocumentAttributeValueCB;
  aIface->get_document_locale = getDocumentLocaleCB;
}

const gchar* getDocumentLocaleCB(AtkDocument* aDocument) {
  nsAutoString locale;
  Accessible* acc = GetInternalObj(ATK_OBJECT(aDocument));
  if (acc) {
    acc->Language(locale);
  }

  return locale.IsEmpty() ? nullptr : AccessibleWrap::ReturnString(locale);
}

static inline GSList* prependToList(GSList* aList, const char* const aName,
                                    const nsAutoString& aValue) {
  if (aValue.IsEmpty()) {
    return aList;
  }

  // libspi will free these
  AtkAttribute* atkAttr = (AtkAttribute*)g_malloc(sizeof(AtkAttribute));
  atkAttr->name = g_strdup(aName);
  atkAttr->value = g_strdup(NS_ConvertUTF16toUTF8(aValue).get());
  return g_slist_prepend(aList, atkAttr);
}

AtkAttributeSet* getDocumentAttributesCB(AtkDocument* aDocument) {
  nsAutoString url;
  nsAutoString mimeType;
  Accessible* acc = GetInternalObj(ATK_OBJECT(aDocument));

  if (!acc || !acc->IsDoc()) {
    return nullptr;
  }

  nsAccUtils::DocumentURL(acc, url);
  nsAccUtils::DocumentMimeType(acc, mimeType);

  // according to atkobject.h, AtkAttributeSet is a GSList
  GSList* attributes = nullptr;
  attributes = prependToList(attributes, kDocUrlName, url);
  attributes = prependToList(attributes, kMimeTypeName, mimeType);

  return attributes;
}

const gchar* getDocumentAttributeValueCB(AtkDocument* aDocument,
                                         const gchar* aAttrName) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aDocument));

  if (!acc || !acc->IsDoc()) {
    return nullptr;
  }

  nsAutoString attrValue;
  if (!strcasecmp(aAttrName, kDocUrlName)) {
    nsAccUtils::DocumentURL(acc, attrValue);
  } else if (!strcasecmp(aAttrName, kMimeTypeName)) {
    nsAccUtils::DocumentMimeType(acc, attrValue);
  } else {
    return nullptr;
  }

  return attrValue.IsEmpty() ? nullptr
                             : AccessibleWrap::ReturnString(attrValue);
}
}
