/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

package com.compilercompany.ecmascript;

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
