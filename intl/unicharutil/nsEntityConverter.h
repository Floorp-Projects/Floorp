/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEntityConverter_h__
#define nsEntityConverter_h__

#include "nsIEntityConverter.h"
#include "nsIStringBundle.h"
#include "nsCOMPtr.h"

class nsEntityConverter: public nsIEntityConverter
{
public:
    //
    // implementation methods
    //
    nsEntityConverter();

    //
    // nsISupports
    //
    NS_DECL_ISUPPORTS

    NS_IMETHOD ConvertUTF32ToEntity(uint32_t character, uint32_t entityVersion, char **_retval) override;
    NS_IMETHOD ConvertToEntity(char16_t character, uint32_t entityVersion, char **_retval) override;
    NS_IMETHOD ConvertToEntities(const char16_t *inString, uint32_t entityVersion, char16_t **_retval) override;

protected:
    // map version number to a string bundle
    nsIStringBundle* GetVersionBundleInstance(uint32_t versionNumber);

    // load a string bundle file
    already_AddRefed<nsIStringBundle> LoadEntityBundle(const char *fileName);

    const char* kHTML40LATIN1 = "html40Latin1.properties";
    const char* kHTML40SYMBOLS = "html40Symbols.properties";
    const char* kHTML40SPECIAL = "html40Special.properties";
    const char* kMATHML20 = "mathml20.properties";
    nsCOMPtr<nsIStringBundle> mHTML40Latin1Bundle;
    nsCOMPtr<nsIStringBundle> mHTML40SymbolsBundle;
    nsCOMPtr<nsIStringBundle> mHTML40SpecialBundle;
    nsCOMPtr<nsIStringBundle> mMathML20Bundle;

    virtual ~nsEntityConverter();
};

#endif
