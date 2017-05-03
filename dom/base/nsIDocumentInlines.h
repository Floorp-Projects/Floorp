/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocumentInlines_h
#define nsIDocumentInlines_h

#include "nsIDocument.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsStyleSheetService.h"

inline mozilla::dom::HTMLBodyElement*
nsIDocument::GetBodyElement()
{
  return static_cast<mozilla::dom::HTMLBodyElement*>(GetHtmlChildElement(nsGkAtoms::body));
}

template<typename T>
size_t
nsIDocument::FindDocStyleSheetInsertionPoint(
    const nsTArray<T>& aDocSheets,
    mozilla::StyleSheet* aSheet)
{
  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();

  // lowest index first
  int32_t newDocIndex = GetIndexOfStyleSheet(aSheet);

  int32_t count = aDocSheets.Length();
  int32_t index;
  for (index = 0; index < count; index++) {
    mozilla::StyleSheet* sheet = static_cast<mozilla::StyleSheet*>(
      aDocSheets[index]);
    int32_t sheetDocIndex = GetIndexOfStyleSheet(sheet);
    if (sheetDocIndex > newDocIndex)
      break;

    // If the sheet is not owned by the document it can be an author
    // sheet registered at nsStyleSheetService or an additional author
    // sheet on the document, which means the new
    // doc sheet should end up before it.
    if (sheetDocIndex < 0) {
      if (sheetService) {
        auto& authorSheets =
          *sheetService->AuthorStyleSheets(GetStyleBackendType());
        if (authorSheets.IndexOf(sheet) != authorSheets.NoIndex) {
          break;
        }
      }
      if (sheet == GetFirstAdditionalAuthorSheet()) {
        break;
      }
    }
  }

  return size_t(index);
}

#endif // nsIDocumentInlines_h
