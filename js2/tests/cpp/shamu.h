struct JSRuntime {
    double jsNaN;
    double jsNegativeInfinity;
    double jsPositiveInfinity;
    JSString *emptyString;
};

struct JSContext {
    JSRuntime *runtime;
};


#ifdef JS_ARGUMENT_FORMATTER_DEFINED
/*
 * Linked list mapping format strings for JS_{Convert,Push}Arguments{,VA} to
 * formatter functions.  Elements are sorted in non-increasing format string
 * length order.
 */
struct JSArgumentFormatMap {
    const char          *format;
    size_t              length;
    JSArgumentFormatter formatter;
    JSArgumentFormatMap *next;
};

#endif