
import java.util.Vector;

class ControlNodeGroup {
    
    ControlNodeGroup(ControlNode aHead)
    {
        head = aHead;
        tails = new Vector();
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
    }
    
  
    ControlNode head;
    Vector tails;

}