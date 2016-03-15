/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MITRE_XMLPARSER_H
#define MITRE_XMLPARSER_H

#include "txCore.h"

class txXPathNode;

/**
 * API to load XML files into DOM datastructures.
 * Parsing is either done by expat, or by expat via the syncloaderservice
 */

/**
 * Parse a document from the aHref location, with referrer URI on behalf
 * of the document aLoader.
 */
extern "C" nsresult
txParseDocumentFromURI(const nsAString& aHref, const txXPathNode& aLoader,
                       nsAString& aErrMsg, txXPathNode** aResult);

#endif
