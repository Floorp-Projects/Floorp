package com.compilercompany.es3c.v1;

/**
 * Represents token instances: literals and identifiers.
 *
 * This file implements the class Token that is used to carry
 * information from the Scanner to the Parser.
 */

import java.util.Vector;

public final class Token implements Tokens {

	int    tokenClass;
    String lexeme;

	public Token( int tokenClass, String lexeme ) {
		this.tokenClass = tokenClass;
		this.lexeme     = lexeme;
	}

	public final int getTokenClass() {
		return tokenClass;
	}

	public final String getTokenText() {
		if( tokenClass == stringliteral_token ) {
			// erase quotes.
            return lexeme.substring(1,lexeme.length()-1);
		}
		return lexeme;
	}

	public final String getTokenSource() {
		return lexeme;
	}

	public static String getTokenClassName( int token_class ) {
		return tokenClassNames[-1*token_class];
	}

    /**
	 * main()
	 *
	 * Unit test driver. Execute the command 'java Token' at the
	 * shell to verify this class' basic functionality.
	 */

    public static void main(String[] args) {
	    System.out.println("Token begin");
		for(int i = 0; i>=stringliteral_token;i--) {
		    System.out.println(tokenClassNames[-1*i]);
	    }
		System.out.println("Token end");
	}

}

/*
 * The end.
 */
