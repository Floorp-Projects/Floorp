
import java.util.Hashtable;

class Environment {

    JSStack theStack = new JSStack();        
    JSObject scope = new JSObject("globals");
    
    String print()
    {
        StringBuffer result = new StringBuffer("Globals contents :\n");
        result.append(scope.toString());
        result.append("\nStack Top = " + theStack.size());
        return result.toString();
    }


}