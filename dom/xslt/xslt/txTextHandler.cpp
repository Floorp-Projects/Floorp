/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txTextHandler.h"
#include "nsAString.h"

txTextHandler::txTextHandler(bool aOnlyText)
    : mLevel(0), mOnlyText(aOnlyText) {}

nsresult txTextHandler::attribute(nsAtom* aPrefix, nsAtom* aLocalName,
                                  nsAtom* aLowercaseLocalName, int32_t aNsID,
                                  const nsString& aValue) {
  return NS_OK;
}

nsresult txTextHandler::attribute(nsAtom* aPrefix, const nsAString& aLocalName,
                                  const int32_t aNsID, const nsString& aValue) {
  return NS_OK;
}

nsresult txTextHandler::characters(const nsAString& aData, bool aDOE) {
  if (mLevel == 0) mValue.Append(aData);

  return NS_OK;
}

nsresult txTextHandler::comment(const nsString& aData) { return NS_OK; }

nsresult txTextHandler::endDocument(nsresult aResult) { return NS_OK; }

nsresult txTextHandler::endElement() {
  if (mOnlyText) --mLevel;

  return NS_OK;
}

nsresult txTextHandler::processingInstruction(const nsString& aTarget,
                                              const nsString& aData) {
  return NS_OK;
}

nsresult txTextHandler::startDocument() { return NS_OK; }

nsresult txTextHandler::startElement(nsAtom* aPrefix, nsAtom* aLocalName,
                                     nsAtom* aLowercaseLocalName,
                                     const int32_t aNsID) {
  if (mOnlyText) ++mLevel;

  return NS_OK;
}

nsresult txTextHandler::startElement(nsAtom* aPrefix,
                                     const nsAString& aLocalName,
                                     const int32_t aNsID) {
  if (mOnlyText) ++mLevel;

  return NS_OK;
}
