
import java.util.Stack;

class JSStack {
    
    Stack stack = new Stack();
    
    void push(StackValue v)
    {
        stack.push(v);
    }
    
    boolean isEmpty()
    {
        return stack.isEmpty();
    }

    StackValue pop()
    {
        return (StackValue)stack.pop();
    }
    
    int size()
    {
        return stack.size();
    }

}