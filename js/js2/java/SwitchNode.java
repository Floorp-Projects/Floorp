
import java.util.Vector;

class SwitchNode extends ControlNode {
    

    SwitchNode(ExpressionNode e)
    {
        super(e);
    }
    
    void addCase(ExpressionNode e, ControlNode c)
    {
        if (e == null)
            defaultCode = c;
        else {
            caseExpr.addElement(e);
            caseCode.addElement(c);
        }
    }
    
    ControlNode eval(Environment theEnv)
    {
        JSValue v = expr.eval(theEnv);
        int count = caseExpr.size();
        for (int i = 0; i < count; i++) {
            ExpressionNode e = (ExpressionNode)(caseExpr.elementAt(i));
            JSBoolean b = v.eq(theEnv, e.eval(theEnv)).toJSBoolean(theEnv);                       
            if (b.isTrue())
                return (ControlNode)(caseCode.elementAt(i));
        }
        if (defaultCode != null)
            return defaultCode;
        else
            return next;
    }

    Vector caseExpr = new Vector();
    Vector caseCode = new Vector();
    
    ControlNode defaultCode;
    
}