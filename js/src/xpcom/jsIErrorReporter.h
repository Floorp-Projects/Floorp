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
