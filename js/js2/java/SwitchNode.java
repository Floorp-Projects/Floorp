
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
        ControlNode n = super.eval(theEnv);
        JSValue v = theEnv.theStack.pop();
        int count = caseExpr.size();
        for (int i = 0; i < count; i++) {
            ExpressionNode e = (ExpressionNode)(caseExpr.elementAt(i));
            e.eval(theEnv);
            v.eq(theEnv);                       
            if (theEnv.theStack.pop().toJSBoolean().isTrue())
                return (ControlNode)(caseCode.elementAt(i));
        }
        if (defaultCode != null)
            return defaultCode;
        else
            return n;
    }

    Vector caseExpr = new Vector();
    Vector caseCode = new Vector();
    
    ControlNode defaultCode;
    
}