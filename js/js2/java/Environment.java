
import java.util.Hashtable;

class Environment {

    JSScope scope = new JSScope("globals");
    JSScope globalScope = scope;
    

    void enterNewScope(JSScope newScope)
    {
        newScope.parent = scope;
        scope = newScope;
    }
    
    void leaveScope()
    {
        scope = scope.parent;
    }

    String print()
    {
        StringBuffer result = new StringBuffer("Globals contents :\n");
        result.append(scope.toString());
        return result.toString();
    }

    JSValue resultValue;

}