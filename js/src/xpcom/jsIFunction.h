/*
 * jsIFunction.h -- the XPCOM interface to JavaScript function objects.
 */

#ifndef JS_IFUNCTION_H
#define JS_IFUNCTION_H

#include "nsISupports.h"
#include <jsapi.h>

class jsIFunction: public nsISupports {
 public:
    virtual jsIFunction() = 0;
    virtual ~jsIFunction() = 0;
};

#endif /* JS_IFUNCTION_H */
