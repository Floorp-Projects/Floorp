
import java.util.Hashtable;

class JSObject extends JSValue {
    
    static JSObject JSUndefined = new JSObject("undefined");
    
    JSObject(String aValue)
    {
        value = aValue;
    }
    
    String print(String indent)
    {
        return indent + "JSObject : " + value + "\n";
    }

    void eval(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }

    void typeof(Environment theEnv) {
        if (this == JSUndefined)
            theEnv.theStack.push(new JSString("undefined"));
        else
            theEnv.theStack.push(new JSString("object"));
    }
    
    JSValue defaultValue(String hint) {
        return null;                    // XXX
    }
    
    JSValue toPrimitive(String hint) {
        JSValue result = defaultValue(hint);
        if (result instanceof JSObject)
            throw new JSException(new JSString("default value returned object"));
        else
            return result;
    }
    
    JSBoolean toJSBoolean() {
        return JSBoolean.JSTrue;
    }
        
    JSDouble toJSDouble() {
        return toPrimitive("Number").toJSDouble();
    }
        
    public String toString() {
        return value + contents.toString();
    }
        
    void getProp(Environment theEnv) {
        JSString id = theEnv.theStack.pop().toJSString();
        JSValue v = (JSValue)(contents.get(id.s));
        theEnv.theStack.push(v);
    }
        
    void putProp(Environment theEnv) {
        JSValue v = theEnv.theStack.pop();
        JSString id = theEnv.theStack.pop().toJSString();
        contents.put(id.s, v);
    }
        
        
    Hashtable contents = new Hashtable();
        
    String value;
    
}