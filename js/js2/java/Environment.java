
import java.util.Hashtable;

class Environment {

    JSObject scope = new JSObject("globals", null);

    void enterNewScope(JSObject newScope)
    {
        
    }

    String print()
    {
        StringBuffer result = new StringBuffer("Globals contents :\n");
        result.append(scope.toString());
        return result.toString();
    }

    JSValue resultValue;

}