class NativeNumber extends JSObject {
    
    NativeNumber(double p) {
        super("Number");
        d = p;
    }
    
    JSValue defaultValue(Environment theEnv, String hint) {
        if (hint.equals("String"))
            return new JSString(Double.toString(d));
        else
            return new JSDouble(d);
    }    
    
    double d;
    
    
}