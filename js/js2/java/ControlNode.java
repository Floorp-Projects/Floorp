
import java.util.Vector;

class ControlNode {

    private static Vector gList = new Vector();
    
    static String printAll()
    {
        StringBuffer result = new StringBuffer();
        for (int i = 0; i < gList.size(); i++) {
            result.append(((ControlNode)(gList.elementAt(i))).print());
        }
        return result.toString();
    }

    ControlNode(ExpressionNode anExpr)
    {
        expr = anExpr;
        index = gList.size();
        gList.addElement(this);
    }
    
    void setNext(ControlNode aNext)
    {
        next = aNext;
    }
    
    ControlNode eval(Environment theEnv)
    {
        expr.eval(theEnv);
        return next;
    }
    
    String print()
    {
        StringBuffer result = new StringBuffer("ControlNode ");
        result.append(index);
        result.append("\nexpr:\n");
        if (expr == null)
            result.append("expr = null\n");
        else
            result.append(expr.print(""));
        result.append("next:\n");
        if (next == null)
            result.append("next = null\n");
        else
            result.append("next->" + next.index + "\n");
        return result.toString();
    }

    protected ExpressionNode expr;
    protected ControlNode next;   
    protected int index;

}





