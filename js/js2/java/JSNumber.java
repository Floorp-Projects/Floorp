abstract class JSNumber extends JSValue {

    JSValue typeof(Environment theEnv) {
        return new JSString("number");
    }

    
}