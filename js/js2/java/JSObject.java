
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
        
    JSValue eval(Environment theEnv)
    {
        return this;
    }

    JSValue typeof(Environment theEnv) {
        if (this == JSUndefined)
            return new JSString("undefined");
        else
            return new JSString("object");
    }
    
    JSBoolean toJSBoolean(Environment theEnv) {
        return JSBoolean.JSTrue;
    }
        
    JSDouble toJSDouble(Environment theEnv) {
        return toPrimitive(theEnv, "Number").toJSDouble(theEnv);
    }
        
    JSValue getProp(Environment theEnv, JSString id) {
        return (JSValue)(contents.get(id.s));
    }
        
    JSValue putProp(Environment theEnv, JSString id, JSValue rV) {
        contents.put(id.s, rV);
        return rV;
    }
        
        
    Hashtable contents = new Hashtable();
        
    String value;
    
}