/*
 * jsIScriptable.h -- the XPCOM interface to native JavaScript objects.
 */

#ifndef JS_ISCRIPTABLE_H
#define JS_ISCRIPTABLE_H

#include "nsISupports.h"
#include <jsapi.h>

class jsIScriptable: public nsISupports {
 public:
    virtual jsIScriptable() = 0;
    virtual ~jsIScriptable() = 0;
};

#endif /* JS_ISCRIPTABLE_H */
