import java.io.*;

class Brenda {
    
	public static void main(String[] args) {
		try {
			JSLexer lexer = new JSLexer((args != null) ? new DataInputStream(new FileInputStream(args[0])) : new DataInputStream(System.in));
			JSParser parser = new JSParser(lexer);
			ExpressionNode tree = parser.expression(true, true);
			System.out.println(tree.print(""));
		} catch(Exception e) {
			System.err.println("exception: "+e);
		}
	}
    
}