
import java.util.Stack;

class JSStack {
    
    Stack stack = new Stack();
    int frameTop;
    
    void newFrame(StackValue returnAddress)
    {
        stack.push(returnAddress);
        stack.push(new StackValue(frameTop));
        frameTop = stack.size();
    }
    
    StackValue popFrame()
    {
        stack.setSize(frameTop);
        StackValue oldFrame = (StackValue)(stack.pop());
        frameTop = oldFrame.i;
        return (StackValue)stack.pop();
    }
    
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

}