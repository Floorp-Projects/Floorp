/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_URIUTILS_H
#define TRANSFRMX_URIUTILS_H

#include "txCore.h"

class nsIDocument;
class nsINode;

/**
 * A utility class for URI handling
 * Not yet finished, only handles file URI at this point
**/

class URIUtils {
public:

    /**
     * Reset the given document with the document of the source node
     */
    static void ResetWithSource(nsIDocument *aNewDoc, nsINode *aSourceNode);

    /**
     * Resolves the given href argument, using the given documentBase
     * if necessary.
     * The new resolved href will be appended to the given dest String
    **/
    static void resolveHref(const nsAString& href, const nsAString& base,
                            nsAString& dest);
}; //-- URIUtils

/* */
#endif
