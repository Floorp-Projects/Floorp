
import java.util.Vector;

class ControlNodeGroup {
    
    ControlNodeGroup()
    {
    }
    
    ControlNodeGroup(ControlNode aHead)
    {
        head = aHead;
    }
    
    void add(ControlNodeGroup aGroup)
    {
        if (head == null) {
            head = aGroup.head;
        }
        else {
            fixTails(aGroup.getHead());
        }
        addTails(aGroup);
    }
    
    void add(ControlNode aNode)
    {
        fixTails(aNode);
        addTail(aNode);
        if (head == null) head = aNode;
    }
    
    void addBreak(ControlNode aNode)
    {
        fixTails(aNode);
        addBreakTail(aNode);
        if (head == null) head = aNode;
    }
    
    void addContinue(ControlNode aNode)
    {
        fixTails(aNode);
        addContinueTail(aNode);
        if (head == null) head = aNode;
    }
    
    void fixTails(ControlNode butt)
    {
        int count = tails.size();
        for (int i = 0; i < count; i++)
        {
            ControlNode aNode = (ControlNode)(tails.elementAt(i));
            aNode.setNext(butt);
        }
        tails.removeAllElements();
    }
    
    void fixContinues(ControlNode butt, String label)
    {
        int count = continueTails.size();
        int i = 0;
        while (i < count) {
            ControlNode c = (ControlNode)(continueTails.elementAt(i));            
            ExpressionNode e = c.getExpression();
            String tgt = (e == null) ? null : ((JSIdentifier)e).s;
            if ((tgt == null) || tgt.equals(label)) {
                c.setNext(butt);
                continueTails.removeElementAt(i);
                count--;
            }
            else
                i++;
        }
    }
    
    void setHead(ControlNode aHead)
    {
        head = aHead;
    }
    
    ControlNode getHead()
    {
        return head;
    }
    
    void addTail(ControlNode aTail)
    {
        tails.addElement(aTail);
    }
    
    void addBreakTail(ControlNode aTail)
    {
        breakTails.addElement(aTail);
    }
    
    void addContinueTail(ControlNode aTail)
    {
        continueTails.addElement(aTail);
    }
    
    void removeTail(ControlNode aTail)
    {
        tails.removeElement(aTail);
    }
    
    void addTails(ControlNodeGroup aGroup)
    {
        int count = aGroup.tails.size();
        for (int i = 0; i < count; i++)
        {
            tails.addElement(aGroup.tails.elementAt(i));
        }
        aGroup.tails.removeAllElements();
        
        count = aGroup.breakTails.size();
        for (int i = 0; i < count; i++)
        {
            breakTails.addElement(aGroup.breakTails.elementAt(i));
        }
        aGroup.breakTails.removeAllElements();

        count = aGroup.continueTails.size();
        for (int i = 0; i < count; i++)
        {
            continueTails.addElement(aGroup.continueTails.elementAt(i));
        }
        aGroup.continueTails.removeAllElements();
    }
    
    void shiftBreakTails(String label)
    {
        int count = breakTails.size();
        int i = 0;
        while (i < count) {
            ControlNode c = (ControlNode)(breakTails.elementAt(i));
            ExpressionNode e = c.getExpression();
            String tgt = (e == null) ? null : ((JSIdentifier)e).s;
            if ((tgt == null) || tgt.equals(label)) {
                tails.addElement(c);
                breakTails.removeElementAt(i);
                count--;
            }
            else
                i++;
        }        
    }
  
    ControlNode head;
    Vector tails = new Vector();
    Vector breakTails = new Vector();
    Vector continueTails = new Vector();

}