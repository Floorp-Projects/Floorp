
import java.util.Vector;

class NativeFunction extends JSObject {
    
    NativeFunction(ControlNode aBody)
    {
        super("Function");
        body = aBody;
    }
    
    JSValue call(Environment theEnv, JSValue rV)
    {
        
        JSScope args = new JSScope("Arguments");
        theEnv.enterNewScope(args);
        
        for (int i = 0; i < parameters.size(); i++) {
            if (rV instanceof JSValueList)
                args.putProp(theEnv, (JSString)(parameters.elementAt(i)), (JSValue) ( ((JSValueList)rV).contents.elementAt(i)) );
            else
                args.putProp(theEnv, (JSString)(parameters.elementAt(i)), rV );
        }
        
        ControlNode c = body;
        while (c != null) c = c.eval(theEnv);
        
        theEnv.leaveScope();
        return theEnv.resultValue;
    }
    
    ControlNode body;
    Vector parameters = new Vector();
    
}