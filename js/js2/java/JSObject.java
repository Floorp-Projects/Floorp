
import java.util.Hashtable;

class JSObject extends JSValue {
    
    static JSObject objectPrototype = new JSObject("Object");
    static JSObject JSUndefined = new JSObject("undefined");
    
    JSObject(String aClass)
    {
        oClass = aClass;
        prototype = objectPrototype;
    }
    
    void setPrototype(JSObject aPrototype)
    {
        prototype = aPrototype;
    }
    
    String print(String indent)
    {
        return indent + "JSObject : " + oClass + "\n";
    }

    public String toString() {
        return oClass + contents.toString();
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
    
    JSValue getProp(Environment theEnv, JSString id)
    {
        Object v = contents.get(id.s);
        if (v == null)
            if (prototype == null)
                return JSUndefined;
            else
                return prototype.getProp(theEnv, id);
        else
            return (JSValue)v;
    }
        
    boolean hasProp(Environment theEnv, JSString id)
    {
        Object v = contents.get(id.s);
        if (v == null)
            if (prototype == null)
                return false;
            else
                return prototype.hasProp(theEnv, id);
        else
            return true;
    }
        
    JSValue putProp(Environment theEnv, JSString id, JSValue rV) {
        contents.put(id.s, rV);
        return rV;
    }
        
        
    Hashtable contents = new Hashtable();
        
    String oClass;
    
    JSObject prototype;
}