/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Header file to be included by module -
 * warning: defines a whole bunch of static functions
 */

#ifndef nsCharDetConstructors_h__
#define nsCharDetConstructors_h__

// chardet
#include "nsISupports.h"
#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include "nsIStringCharsetDetector.h"
#include "nsCyrillicDetector.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsRUProbDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUKProbDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRUStringProbDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUKStringProbDetector)

#endif
