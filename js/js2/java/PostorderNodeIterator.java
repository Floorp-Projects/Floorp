
import java.util.Stack;

class PostorderNodeIterator {

    PostorderNodeIterator(Node n)
    {
        stack = new Stack();
    	while (n.first != null) {
    	    stack.push(n);
            n = n.first;
        }
        start = n;
    }

    Node nextNode()
    {
        if (current == null)
            return current = start;
        
        if (stack.isEmpty())
            return null;
        else {
            current = current.next;
            if (current != null) {
            	while (current.first != null) {
            	    stack.push(current);
                    current = current.first;
                }
            }
            else
                current = (Node)stack.pop();
        }
        
        return current;
    }

    Node start;
    Node current;
    Stack stack;
}