
import java.util.Stack;

class JSStack {
    
    Stack stack = new Stack();
    
    void push(JSValue v)
    {
        stack.push(v);
    }
    
    boolean isEmpty()
    {
        return stack.isEmpty();
    }

    JSValue pop()
    {
        return (JSValue)stack.pop();
    }
    
    JSValue peek()
    {
        return (JSValue)stack.peek();
    }
    
    int size()
    {
        return stack.size();
    }
    
    void setStack(int top)
    {
        stack.setSize(top);
    }

}