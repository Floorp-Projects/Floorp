class JSValue extends ExpressionNode {
    
    String print(String indent)
    {
        return indent + "JSValue\n";
    }

    void evalLHS(Environment theEnv)
    {
        throw new RuntimeException("EvalLHS on non-lvalue");
    }
    
    void eval(Environment theEnv)
    {
        throw new RuntimeException("Eval on JSValue");
    }    
    
    void unimplemented(String op)
    {
        throw new RuntimeException("unimplemented " + op + " called");
    }
    
    void add(Environment theEnv)                        { unimplemented("add"); }
    void subtract(Environment theEnv)                   { unimplemented("subtract"); }
    void multiply(Environment theEnv)                   { unimplemented("multiply"); }
    void divide(Environment theEnv)                     { unimplemented("divide"); }
    void remainder(Environment theEnv)                  { unimplemented("remainder"); }

    void and(Environment theEnv)                        { unimplemented("and"); }
    void or(Environment theEnv)                         { unimplemented("or"); }
    void xor(Environment theEnv)                        { unimplemented("xor"); }
    void shl(Environment theEnv)                        { unimplemented("shl"); }
    void shr(Environment theEnv)                        { unimplemented("shr"); }
    void ushl(Environment theEnv)                       { unimplemented("ushl"); }

    void gt(Environment theEnv)                         { unimplemented("gt"); }
    void ge(Environment theEnv)                         { unimplemented("ge"); }
    void lt(Environment theEnv)                         { unimplemented("lt"); }
    void le(Environment theEnv)                         { unimplemented("le"); }
    void eq(Environment theEnv)                         { unimplemented("eq"); }
    void ne(Environment theEnv)                         { unimplemented("ne"); }
    
    
    JSDouble toJSDouble()                               { unimplemented("toJSDouble"); return null; }
    JSInteger toJSInteger()                             { unimplemented("toJSInteger"); return null; }
    JSString toJSString()                               { unimplemented("toJSString"); return null; }
    JSBoolean toJSBoolean()                             { unimplemented("toJSBoolean"); return null; }
        

}