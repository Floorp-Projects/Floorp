/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsReadConfig_h
#define nsReadConfig_h

#include "mozilla/RefPtr.h"
#include "nsAutoConfig.h"
#include "nsIObserver.h"

class nsReadConfig final : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsReadConfig();

  nsresult Init();

 protected:
  virtual ~nsReadConfig();

  nsresult readConfigFile();
  nsresult openAndEvaluateJSFile(const char* aFileName, int32_t obscureValue,
                                 bool isEncoded, bool isBinDir);
  bool mRead;

 private:
  RefPtr<nsAutoConfig> mAutoConfig;
};

#endif
