/* -*- Mode: cc; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * jsIErrorReporter.h -- the XPCOM interface to JS error and warning reporters.
 */

#ifndef JS_IERRORREPORTER_H
#define JS_IERRORREPORTER_H

#include "nsISupports.h"
#include <jsapi.h>

class jsIErrorReporter: public nsISupports {
 public:
    virtual jsIErrorReporter() = 0;
    virtual ~jsIErrorReporter() = 0;
    /**
     * Report a warning.
     */
    NS_IMETHOD reportWarning(JSString *message,
			     JSString *sourceName,
			     uintN lineno) = 0;

    /**
     * Report an error.
     */
    NS_IMETHOD reportError(JSString *message,
			   JSString *sourceName,
			   uintN lineno) = 0;

};

#endif /* JS_IERRORREPORTER_H */
