abstract class JSNumber extends JSValue {

    void typeof(Environment theEnv) {
        theEnv.theStack.push(new JSString("number"));
    }

    
}