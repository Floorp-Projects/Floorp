
import java.util.Hashtable;

class Environment {

    JSStack theStack = new JSStack();    
    Hashtable theGlobals = new Hashtable();
    
    String print()
    {
        StringBuffer result = new StringBuffer("Globals contents :\n");
        result.append(theGlobals.toString());
        result.append("\nStack Top = " + theStack.size());
        return result.toString();
    }


}