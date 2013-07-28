/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIReadConfig.h"
#include "nsIAutoConfig.h"
#include "nsIObserver.h"


class nsReadConfig : public nsIReadConfig,
                     public nsIObserver
{

    public:

        NS_DECL_THREADSAFE_ISUPPORTS
        NS_DECL_NSIREADCONFIG
        NS_DECL_NSIOBSERVER

        nsReadConfig();
        virtual ~nsReadConfig();

        nsresult Init();

    protected:
  
        nsresult readConfigFile();
        nsresult openAndEvaluateJSFile(const char *aFileName, int32_t obscureValue, 
                                        bool isEncoded, bool isBinDir);
        bool mRead;
private:
        nsCOMPtr<nsIAutoConfig> mAutoConfig;
};
