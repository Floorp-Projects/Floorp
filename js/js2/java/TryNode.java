
import java.util.Vector;

class TryNode extends ControlNode {
    
    TryNode(ControlNode tryCode)
    {
        super(null);
        tryBody = tryCode;
    }
    
    void addFinally(ControlNode finallyCode)
    {
        finallyBody = finallyCode;
    }
    
    void addCatchClause(ExpressionNode e, ControlNode c)
    {
        catchExpr.addElement(e);
        catchCode.addElement(c);
    }

    ControlNode eval(Environment theEnv)
    {
        try {
            ControlNode c = tryBody;
            while (c != null) c = c.eval(theEnv);
        }
        catch (JSException x) {
            int count = catchExpr.size();
            for (int i = 0; i < count; i++) {
                ExpressionNode e = (ExpressionNode)(catchExpr.elementAt(i));
                String id = ((JSObject)e).value;
                theEnv.scope.contents.put(id, x.getValue());    // XXX YAARGH !!!
                return (ControlNode)(catchCode.elementAt(i));
            }
        }
        return next;
    }
    
    Vector catchExpr = new Vector();
    Vector catchCode = new Vector();
        
    ControlNode tryBody;
    ControlNode finallyBody;    

}