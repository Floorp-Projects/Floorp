public class Interpreter {


    void executeScript(Node node)
    {
        Node child = node.getFirstChild();
        while (child != null) {
            if (child.getType() != TokenStream.FUNCTION)
                executeCode(child);
            child = child.getNextSibling();
        }
    }

    void executeCode(Node top)
    {
        PostorderNodeIterator ni = new PostorderNodeIterator(top);

        JSStack theStack = new JSStack();

        Node n = ni.nextNode();
        while (n != null) {
            ni = n.execute(theStack, ni);
            n = ni.nextNode();
        }


    }



}