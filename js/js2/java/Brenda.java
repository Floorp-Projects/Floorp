import java.io.*;

class Brenda {
    
	public static void main(String[] args) {
		try {
			JSLexer lexer = new JSLexer((args != null) ? new DataInputStream(new FileInputStream(args[0])) : new DataInputStream(System.in));
			JSParser parser = new JSParser(lexer);
			ControlNode tree = parser.statements(0);
			System.out.println(ControlNode.printAll());
		} catch(Exception e) {
			System.err.println("exception: "+e);
		}
	}
    
}