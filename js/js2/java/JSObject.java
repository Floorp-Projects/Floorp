
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

    public String toString() {
        return value + contents.toString();
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
    
    JSBoolean toJSBoolean(Environment theEnv) {
        return JSBoolean.JSTrue;
    }
        
    JSDouble toJSDouble(Environment theEnv) {
        return toPrimitive(theEnv, "Number").toJSDouble(theEnv);
    }
        
    void getProp(Environment theEnv) {
        JSString id = theEnv.theStack.pop().toJSString(theEnv);
        JSValue v = (JSValue)(contents.get(id.s));
        theEnv.theStack.push(v);
    }
        
    void putProp(Environment theEnv) {
        JSValue v = theEnv.theStack.pop();
        JSString id = theEnv.theStack.pop().toJSString(theEnv);
        contents.put(id.s, v);
    }
        
        
    Hashtable contents = new Hashtable();
        
    String value;
    
}