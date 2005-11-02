/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Tom Kneeland
 *    -- original author.
 */

#ifndef MITRE_XMLPARSER_H
#define MITRE_XMLPARSER_H

#include "dom.h"

#ifdef TX_EXE
#include <iostream.h>
#endif

/**
 * API to load XML files into DOM datastructures.
 * Parsing is either done by expat, or by expat via the synchloaderservice
 */

/**
 * Parse a document from the aHref location, with referrer URI on behalf
 * of the document aLoader.
 */
extern "C" nsresult
txParseDocumentFromURI(const nsAString& aHref, const nsAString& aReferrer,
                       Document* aLoader, nsAString& aErrMsg,
                       Document** aResult);

#ifdef TX_EXE
/**
 * Parse a document from the given stream
 */
extern "C" nsresult
txParseFromStream(istream& aInputStream, const nsAString& aUri,
                  nsAString& aErrorString, Document** aResult);
#endif
#endif
