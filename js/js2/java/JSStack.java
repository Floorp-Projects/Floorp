
import java.util.Stack;

class JSStack {

    Stack stack = new Stack();

    void push(StackValue value)
    {
        stack.push(value);
    }

    StackValue pop()
    {
        return (StackValue)stack.pop();
    }

}