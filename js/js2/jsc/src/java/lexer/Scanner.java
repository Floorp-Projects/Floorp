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

/**
 * Scanner.java
 */


package com.compilercompany.ecmascript;
import java.util.Vector;
import java.io.*;


public class Scanner implements Tokens, States, CharacterClasses {

    private static final boolean debug = false;

    private   Vector tokens;   // vector of token instances.
    private   int    slash_context;  // slashdiv_context or slashregexp_context
    private   String codebase;
    private   boolean foundNewlineInComment;
    private   boolean isFirstTokenOnLine;
    
    InputBuffer input;

    public static PrintStream err;
    public static PrintStream out;
    public static void setOut(String filename) throws Exception {
        out = new PrintStream( new FileOutputStream(filename) );
    }

    /**
     *
     * self tests.
     * 
     */

    static int failureCount = 0;

    public static void main( String[] args ) {

        PrintStream outfile = null;

        try {

            outfile = new PrintStream( new FileOutputStream( "Scanner.test" ) );
            System.setOut( outfile );

            System.out.println( "Scanner test begin" );

            test_nexttoken();

            if( failureCount != 0 )
                System.out.println( "Scanner test completed: " + failureCount + " tests failed" );
            else
                System.out.println( "Scanner test completed successfully" );

        } catch( Exception e ) {
            e.printStackTrace();
        } finally {
            outfile.close();
            System.setOut( null );
        }
    }

    /**
     * Scanner constructors.
     */

    private Scanner() {
        this.tokens = new Vector();
        this.slash_context |= slashregexp_context_flag;
    }

    public Scanner( Reader reader, PrintStream errout, String codebase ) {
        this();
        this.input = new InputBuffer(reader, errout, codebase);
        this.codebase = codebase;
        this.err    = errout;
    }

    /**
     * nextchar() -- 
     * Get the next character that has lexical significance. WhiteSpace,
     * LineTerminators and Comments are normalized to various combinations
     * of ' ' and '\n'. Unicode format control character are not significant
     * in the lexical grammar and so are removed from the character stream.
     * If the end of the input stream is reached, then 0 is returned.
     */

    private final char nextchar() throws Exception {

        return (char)input.nextchar();

    }

    public final String lexeme() throws Exception {
        return input.copy();
    }

    /**
     * retract() -- 
     * Causes one character of input to be 'put back' onto the
     * que. [Test whether this works for comments and white space.]
     */

    public final void retract() throws Exception {
        input.retract();
        return;
    }

    /**
     * Various helper methods for managing and testing the 
     * scanning context for slashes.
     */

    private static final int slashdiv_context_flag    = 0x00000001;
    private static final int slashregexp_context_flag = 0x00000002;

    public final void enterSlashDivContext() {
        slash_context |= slashdiv_context_flag;
    }

    public final void exitSlashDivContext() {
        slash_context ^= slashdiv_context_flag;
    }

    public final void enterSlashRexexpContext() {
        slash_context |= slashregexp_context_flag;
    }

    public final void exitSlashRegexpContext() {
        slash_context ^= slashregexp_context_flag;
    }

    private final int isSlashDivContext() {
        return slash_context & slashdiv_context_flag;
    }

    private final int isSlashRegexpContext() {
        return slash_context & slashregexp_context_flag;
    }

    /**
     * makeTokenInstance() --
     * Make an instance of the specified token class using the lexeme string.
     * Return the index of the token which is its identifier.
     */

    protected final int makeTokenInstance( int token_class, String lexeme ) {
        tokens.addElement( new Token( token_class, lexeme ) );
        return tokens.size()-1; /* return the tokenid */
    }

    /**
     * getTokenClass() --
     * Get the class of a token instance.
     */

    public final int getTokenClass( int token_id ) {

        // if the token id is negative, it is a token_class.
        if( token_id < 0 ) {
            return token_id;
        }

        // otherwise, get instance data from the instance vector.
        return ((Token)tokens.elementAt( token_id )).getTokenClass();
    }

    /**
     * getTokenText() --
     * Get the text of a token instance.
     *
     */

    public final String getTokenText( int token_id ) {

        // if the token id is negative, it is a token_class.
        if( token_id < 0 ) {
            return Token.getTokenClassName(token_id);
        }

        // otherwise, get instance data from the instance vector.
        return ((Token)tokens.elementAt( token_id )).getTokenText();
    }

    /**
     * getPersistentTokenText() --
     * Get the persistent text of a token instance.
     *
     */

    public final String getPersistentTokenText( int token_id ) {

        // if the token id is negative, it is a token_class.
        if( token_id < 0 ) {
            return Token.getTokenClassName(token_id);
        }

        // otherwise, get instance data from the instance vector.
        return Token.getTokenClassName(getTokenClass(token_id)) 
                       + "(" + ((Token)tokens.elementAt( token_id )).getTokenText() + ")";
    }

    /**
     * getTokenSource() --
     * get the source of a token instance.
     */

    public final String getTokenSource( int token_id ) {

        // if the token id is negative, it is a token_class.
        if( token_id < 0 ) {
            return Token.getTokenClassName(token_id);
        }

        // otherwise, get instance data from the instance vector.
        return ((Token)tokens.elementAt( token_id )).getTokenSource();
    }

    /**
     * getLineText() -- 
     * Get the text of the current line.
     */

    public final String getLineText() {
        return input.curr_line.toString();
    }

    /**
     * getLinePointer() --
     * Generate a string that contains a carat character at 
     * a specified position.
     */

    public final String getLinePointer() {
        return getLinePointer( "" );
    }

    public final String getLinePointer(String header) {
        StringBuffer padding = new StringBuffer();
        for ( int i = header.length() ; i > 0 ; i-- ) {
            padding.append( " " );
        }
        for( int i=1 ; i < input.colPos ; i++ ) {
            padding.append( " " );
        }
        return new String(padding + "^");
    }

    public final String getLinePointer(int pos) {
        StringBuffer padding = new StringBuffer();
        for( int i=0 ; i < pos ; i++ ) {
            padding.append( " " );
        }
        return new String(padding + "^");
    }

    public final String getErrorText() {
        return input.curr_line.toString();
    }

    /**
     * Record an error.
     */

    public static final int lexical_error = 0;
    public static final int lexical_lineterminatorinsinglequotedstringliteral_error = lexical_error+1;
    public static final int lexical_lineterminatorindoublequotedstringliteral_error = lexical_lineterminatorinsinglequotedstringliteral_error+1;
    public static final int lexical_backslashinsinglequotedstring_error = lexical_lineterminatorindoublequotedstringliteral_error +1;
    public static final int lexical_backslashindoublequotedstring_error = lexical_backslashinsinglequotedstring_error +1;
    public static final int lexical_endofstreaminstringliteral_error = lexical_backslashindoublequotedstring_error +1;
    public static final int lexical_nondecimalidentifiercharindecmial_error = lexical_endofstreaminstringliteral_error +1;
    public static final int syntax_error = lexical_nondecimalidentifiercharindecmial_error+1;

    private static final String[] error_messages = {
        "Lexical error.",
        "String literal must be terminated before line break.",
        "String literal must be terminated before line break.",
        "Backslash characters are not allowed in string literals.",
        "Backslash characters are not allowed in string literals.",
        "End of input reached before closing quote for string literal.",
        "Decimal numbers may not contain non-decimal identifier characters.",
        "Syntax error."
    };

    public final void error(int kind, String msg, int tokenid) {
        String loc = null;
        int pos;
        switch(kind) {
            case lexical_error:
            case lexical_lineterminatorinsinglequotedstringliteral_error:
            case lexical_lineterminatorindoublequotedstringliteral_error:
            case lexical_backslashindoublequotedstring_error:
            case lexical_backslashinsinglequotedstring_error:
            case lexical_endofstreaminstringliteral_error:
            case syntax_error:
            default:
                loc = ((codebase==null||codebase.length()==0)?"":codebase+": ") + "Ln " + (input.markLn+1) + ", Col " + (input.markCol+1) + ": ";
                msg = msg==null?error_messages[kind]:msg;
                break;
        }
        pos = loc.length()-1;
        System.err.println(loc+msg);
        System.err.println(input.getLineText(input.positionOfMark()));
        System.err.println(getLinePointer(input.markCol));
        skiperror(kind);
    }

    public final void error(int kind, String msg) {
        error(kind,msg,error_token);    
    }

    public final void error(int kind) {
        error(kind,null);    
    }

    /**
     * skip ahead after an error is detected. this simply goes until the next
     * whitespace or end of input.
     */

    private final void skiperror() {
        skiperror(lexical_error);
    }

    private final void skiperror(int kind) {
        Debugger.trace("start error recovery");
        try {
        switch(kind) {
            case lexical_error:
                while ( true ) {
                    char nc = nextchar();
                    Debugger.trace("nc " + nc);
                    if( nc == ' ' ||
                        nc == '\n' ||
                        nc == 0 ) {
                        return;
                    }
                } 
            case lexical_lineterminatorinsinglequotedstringliteral_error:
            case lexical_backslashinsinglequotedstring_error:
            case lexical_lineterminatorindoublequotedstringliteral_error:
            case lexical_backslashindoublequotedstring_error:
                while ( true ) {
                    char nc = nextchar();
                    if( nc == '\'' || nc == 0 ) {
                        return;
                    }
                }
            case lexical_nondecimalidentifiercharindecmial_error:
                while ( true ) {
                    char nc = nextchar();
                    if( nc == ' ' || nc == '\n' || nc == 0 ) {
                        return;
                    }
                }
            case syntax_error:
                err.println("Syntax error");
                break;
            default:
                break;
        } 
        } catch (Exception x) {
            x.printStackTrace();
        } finally {
            Debugger.trace("stop error recovery");
        }
    }

    /**
     *
     */

    boolean test_skiperror() {
        return true;
    }

  /**
   *
   *
  **/

  public final boolean followsLineTerminator() {
      if( debug ) {
          Debugger.trace("isFirstTokenOnLine = " + isFirstTokenOnLine);
      }
      return isFirstTokenOnLine;
  }

  /**
   *
   *
  **/

  final public int nexttoken() throws Exception {

    int state = start_state;
    isFirstTokenOnLine = false;

    while ( true ) {
        if( debug ) { 
            Debugger.trace( "\nstate = " + state + ", next = " + input.positionOfNext() );
        }

      switch( state ) {

        case start_state:
          int c = nextchar();
          input.mark();
          switch( c ) {
            case '@': return ampersand_token;
            case '\'': state = singlequote_state; continue;
            case '\"': state = doublequote_state; continue;
            case '-': state = minus_state; continue;
            case '!': state = not_state; continue;
            case '%': state = remainder_state; continue;
            case '&': state = and_state; continue;
            case '(': return leftparen_token;
            case ')': return rightparen_token;
            case '*': state = star_state; continue;
            case ',': return comma_token;
            case '.': state = dot_state; continue;
            case '/': state = slash_state; continue;
            case ':': state = colon_state; continue;
            case ';': return semicolon_token;
            case '?': return questionmark_token;
            case '[': return leftbracket_token;
            case ']': return rightbracket_token;
            case '^': state = bitwisexor_state; continue;
            case '{': return leftbrace_token;
            case '|': state = or_state; continue;
            case '}': return rightbrace_token;
            case '~': return bitwisenot_token;
            case '+': state = plus_state; continue;
            case '<': state = lessthan_state;   continue;
            case '=': state = equal_state;      continue;
            case '>': state = greaterthan_state;continue;
            case 'a': state = a_state;          continue;
            case 'b': state = b_state;          continue;
            case 'c': state = c_state;          continue;
            case 'd': state = d_state;          continue;
            case 'e': state = e_state;          continue;
            case 'f': state = f_state;          continue;
            case 'g': state = g_state;          continue;
            case 'i': state = i_state;          continue;
            case 'l': state = l_state;          continue;
            case 'n': state = n_state;          continue;
            case 'p': state = p_state;          continue;
            case 'r': state = r_state;          continue;
            case 's': state = s_state;          continue;
            case 't': state = t_state;          continue;
            case 'u': state = u_state;          continue;
            case 'v': state = v_state;          continue;
            case 'w': state = w_state;          continue;
            case 'A': case 'B': case 'C': case 'D': case 'E': 
            case 'F': case 'G': case 'H': case 'h': case 'I':  
            case 'J': case 'j': case 'K': case 'k': case 'L':  
            case 'M': case 'm': case 'N': case 'O': case 'o': case 'P': 
                      case 'Q': case 'q': case 'R': case 'S': case 'T':  
            case 'U': case 'V': case 'W': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
                      state = A_state; continue;
            case '0': state = zero_state; continue;
            case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = decimalinteger_state; continue;
            case ' ': state = start_state; continue;
			case '\n': state = start_state;
                      isFirstTokenOnLine = true; continue;
            case 0:   return eos_token;
            default:  switch(input.classOfNext()) {
                          case Lu: case Ll: case Lt:
                          case Lm: case Lo: Nl:
                                   state = A_state; continue;
                          default:
                                   state = error_state; continue;
                      }
          }
        case A_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            case 0:   case ' ': case '\n': 
            default:  switch(input.classOfNext()) {
                      case Lu: case Ll: case Lt:
                      case Lm: case Lo: case Nl:
                      case Mn: case Mc: case Nd:
                      case Pc: state = A_state; continue;
                      default: retract(); 
                               return makeTokenInstance( identifier_token, input.copy() );
                      }
          }

        /**
         * prefix: <eol>
        **/

        case eol_state:
          isFirstTokenOnLine = true;
          switch ( nextchar() ) {
            case '\n': Debugger.trace("eating eol"); state = eol_state; continue; /* eat extra eols */
            default:   Debugger.trace("returning eol"); 
                       state = start_state; /*first = input.positionOfNext();*/ 
                       retract(); state = start_state; continue;
          }

        /**
         * prefix: '
        **/

        case singlequote_state:
          switch ( nextchar() ) {
            case '\'': return makeTokenInstance( stringliteral_token, input.copy() );
            // case '\\': error(lexical_backslashinsinglequotedstring_error); return error_token;
            case '\n': error(lexical_lineterminatorinsinglequotedstringliteral_error); return error_token;
            case 0:    error(lexical_endofstreaminstringliteral_error); return error_token;
            default:   state = singlequote_state; continue;
          }

        /**
         * prefix: "
        **/

        case doublequote_state:
          switch ( nextchar() ) {
            case '\"': return makeTokenInstance( stringliteral_token, input.copy() );
            // case '\\': error(lexical_backslashindoublequotedstring_error); return error_token;
            case '\n': error(lexical_lineterminatorindoublequotedstringliteral_error); return error_token;
            case 0:    error(lexical_endofstreaminstringliteral_error); return error_token;
            default:   state = doublequote_state; continue;
          }

        /**
         * prefix: 0
         * accepts: 0x... | 0X... | 01... | 0... | 0
        **/

        case zero_state:
          switch ( nextchar() ) {
            case 'x': case 'X': 
                      state = hexinteger_state; continue;
            case '0': case '1': case '2': case '3': 
            case '4': case '5': case '6': case '7': 
                      state = octalinteger_state; continue;
            case '.': state = decimalinteger_state; continue;
            case 'E': case 'e':
                      state = exponent_state; continue;
            default:  retract(); return makeTokenInstance( numberliteral_token, input.copy() );
          }

        /**
         * prefix: 0x<hex digits>
         * accepts: 0x123f
        **/

        case hexinteger_state:
          switch ( nextchar() ) {
            case '0': case '1': case '2': case '3': 
            case '4': case '5': case '6': case '7': 
            case '8': case '9': case 'a': case 'b': 
			case 'c': case 'd': case 'e': case 'f':
            case 'A': case 'B': case 'C': case 'D': 
			case 'E': case 'F': 
                      state = hexinteger_state; continue;
            default:  retract(); return makeTokenInstance( numberliteral_token, input.copy() );
										// skip the 0x prefix
          }

        /**
         * prefix: 0<octal digit>
         * accepts: 0123
        **/

        case octalinteger_state:
          switch ( nextchar() ) {
            case '0': case '1': case '2': case '3': 
            case '4': case '5': case '6': case '7': 
                      state = octalinteger_state; continue;
            default:  retract(); return makeTokenInstance( numberliteral_token, input.copy() );
          }

        /**
         * prefix: .
         * accepts: .123 | .
        **/

        case dot_state:
          switch ( nextchar() ) {
            case '0': case '1': case '2': case '3': 
            case '4': case '5': case '6': case '7': 
            case '8': case '9': 
                      state = decimal_state; continue;
            case '.': state = doubledot_state; continue;
            default:  retract(); return dot_token;
          }

        /**
         * accepts: ..
        **/

        case doubledot_state:
          switch ( nextchar() ) {
            case '.': return tripledot_token;
            default:  retract(); return doubledot_token;
          }

        /**
         * prefix: N
         * accepts: 0.123 | 1.23 | 123 | 1e23 | 1e-23
        **/

        case decimalinteger_state:
          switch ( nextchar() ) {
            case '0': case '1': case '2': case '3': 
            case '4': case '5': case '6': case '7':
            case '8': case '9': case '.':
                      state = decimalinteger_state; continue;
            case 'E': case 'e':
                      state = exponent_state; continue;
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd':                     case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
                      error(lexical_nondecimalidentifiercharindecmial_error); return error_token;
            default:  retract(); return makeTokenInstance( numberliteral_token, input.copy() );
          }

        /**
         * prefix: N.
         * accepts: 0.1 | 1e23 | 1e-23
        **/

        case decimal_state:
          switch ( nextchar() ) {
            case '0': case '1': case '2': case '3': 
            case '4': case '5': case '6': case '7':
            case '8': case '9': 
                      state = decimal_state; continue;
            case 'E': case 'e': 
                      state = exponent_state; continue;
            default:  retract(); return makeTokenInstance( numberliteral_token, input.copy() );
          }

        /**
         * prefix: ..e
         * accepts: ..eN | ..e+N | ..e-N
        **/

        case exponent_state:
          switch ( nextchar() ) {
            case '0': case '1': case '2': case '3': 
            case '4': case '5': case '6': case '7':
            case '8': case '9': case '+': case '-': 
                      state = exponent_state; continue;
            default:  retract(); return makeTokenInstance( numberliteral_token, input.copy() );
          }

        /**
         * tokens: --  -=  -
        **/

        case minus_state:
          switch ( nextchar() ) {
            case '-': return minusminus_token;
            case '=': return minusassign_token;
            default:  retract(); return minus_token;
          }

        /**
         * prefix: !
         * tokens: !=  !
        **/

        case not_state:
          switch ( nextchar() ) {
            case '=': return notequals_token;
            default:  retract(); return not_token;
          }

        /**
         * prefix: %
         * tokens: %= %
        **/

        case remainder_state:
          switch ( nextchar() ) {
            case '=': return modulusassign_token;
            default:  retract(); return modulus_token;
          }

        /**
         * tokens: &&  &=  &
        **/

        case and_state:
          switch ( nextchar() ) {
            case '&': return logicaland_token;
            case '=': return bitwiseandassign_token;
            default:  retract(); return bitwiseand_token;
          }

        /**
         * tokens: *=  *
        **/

        case star_state:
          switch ( nextchar() ) {
            case '=': return multassign_token;
            default:  retract(); return mult_token;
          }

        /**
         * tokens: // /* <slashdiv> <slashregexp>
         */

        case slash_state:
          switch ( nextchar() ) {
            case '/': state = linecomment_state; continue;
            case '*': foundNewlineInComment=false; state = blockcomment_state; continue;
            default:
                retract(); /* since we didn't use the current character 
                              for this decision. */
                if(isSlashDivContext()!=0) {
                    state = slashdiv_state; continue;
                } else {
                    state = slashregexp_state; continue;
                }
          }

        /**
         * tokens: / /=
         */

        case slashdiv_state:
            switch ( nextchar() ) {
                case '=': return divassign_token;
                default:  retract(); return div_token;
            }

        /**
         * tokens: : ::
         */

        case colon_state:
            switch ( nextchar() ) {
                case ':': return doublecolon_token;
                default:  retract(); return colon_token;
            }

        /**
         * tokens: /<regexpbody>/<regexpflags>
         */

        case slashregexp_state:
            switch ( nextchar() ) {
                case '/': state = regexp_state; continue;
                case 0: case '\n': error(lexical_error); return error_token;
                default:  state = slashregexp_state; continue;
            }

        /**
         * tokens: g | i | m
         */

        case regexp_state:
          switch ( nextchar() ) {
            case 'g': state = regexpg_state; continue;
            case 'i': state = regexpi_state; continue;
            case 'm': state = regexpm_state; continue;
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'H': case 'h': case 'I':
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      error(lexical_error); return error_token;
            case 0: case ' ': case '\n': 
            default: retract(); return makeTokenInstance( regexpliteral_token, input.copy() );
          }

        /**
         * tokens: i | m
         */

        case regexpg_state:
          switch ( nextchar() ) {
            case 'i': state = regexpgi_state; continue;
            case 'm': state = regexpgm_state; continue;
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I':
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      error(lexical_error); return error_token;
            case 0: case ' ': case '\n': 
            default: retract(); return makeTokenInstance( regexpliteral_token, input.copy() );
          }

        /**
         * tokens: g | m
         */

        case regexpi_state:
          switch ( nextchar() ) {
            case 'g': state = regexpgi_state; continue;
            case 'm': state = regexpim_state; continue;
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'H': case 'h': case 'I': case 'i':
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      error(lexical_error); return error_token;
            case 0: case ' ': case '\n': 
            default: retract(); return makeTokenInstance( regexpliteral_token, input.copy() );
          }

        /**
         * tokens: g | i
         */

        case regexpm_state:
          switch ( nextchar() ) {
            case 'g': state = regexpgm_state; continue; 
            case 'i': state = regexpim_state; continue;
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'H': case 'h': case 'I': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      error(lexical_error); return error_token;
            case 0: case ' ': case '\n': 
            default: retract(); return makeTokenInstance( regexpliteral_token, input.copy() );
          }

        /**
         * tokens: g
         */

        case regexpim_state:
          switch ( nextchar() ) {
            case 'g': state = regexpgim_state; continue;
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'H': case 'h': case 'I': case 'i':
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      error(lexical_error); return error_token;
            case 0: case ' ': case '\n': 
            default: retract(); return makeTokenInstance( regexpliteral_token, input.copy() );
          }

        /**
         * tokens: i
         */

        case regexpgm_state:
          switch ( nextchar() ) {
            case 'i': state = regexpgim_state; continue;
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      error(lexical_error); return error_token;
            case 0: case ' ': case '\n': 
            default: retract(); return makeTokenInstance( regexpliteral_token, input.copy() );
          }

        /**
         * tokens: m
         */

        case regexpgi_state:
          switch ( nextchar() ) {
            case 'm': state = regexpgim_state; continue;
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i':
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      error(lexical_error); return error_token;
            case 0: case ' ': case '\n': 
            default: retract(); return makeTokenInstance( regexpliteral_token, input.copy() );
          }

        /**
         * tokens: 
         */

        case regexpgim_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i':
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      error(lexical_error); return error_token;
            case 0: case ' ': case '\n': 
            default: retract(); return makeTokenInstance( regexpliteral_token, input.copy() );
          }

        /**
         * tokens: ^^ ^^= ^=  ^
        **/

        case bitwisexor_state:
          switch ( nextchar() ) {
            case '=': return bitwisexorassign_token;
            case '^': state = logicalxor_token; break;
            default:  retract(); return bitwisexor_token;
          }

        /**
         * tokens: ^^ ^=  ^
        **/

        case logicalxor_state:
          switch ( nextchar() ) {
            case '=': return logicalxorassign_token;
            default:  retract(); return logicalxor_token;
          }

        /**
         * tokens: ||  |=  |
        **/

        case or_state:
          switch ( nextchar() ) {
            case '|': return logicalor_token;
            case '=': return bitwiseorassign_token;
            default:  retract(); return bitwiseor_token;
          }

        /**
         * tokens: ++  += +
        **/

        case plus_state:
          switch ( nextchar() ) {
            case '+': return plusplus_token;
            case '=': return plusassign_token;
            default:  retract(); return plus_token;
          }


        /**
         * tokens: <=  <
        **/

        case lessthan_state:
          switch ( nextchar() ) {
            case '<': state = leftshift_state; break;
            case '=': return lessthanorequals_token;
            default:  retract(); return lessthan_token;
          }

        /**
         * tokens: <<=  <<
        **/

        case leftshift_state:
          switch ( nextchar() ) {
            case '=': return leftshiftassign_token;
            default:  retract(); return leftshift_token;
          }

        /**
         * tokens: ==  =
        **/

        case equal_state:
          switch ( nextchar() ) {
            case '=': state = equalequal_state; break;
            default:  retract(); return assign_token;
          }

        /**
         * tokens: ===  ==
        **/

        case equalequal_state:
          switch ( nextchar() ) {
            case '=': return strictequals_token;
            default:  retract(); return equals_token;
          }

        /**
         * prefix: >
        **/

        case greaterthan_state:
          switch ( nextchar() ) {
            case '>': state = rightshift_state; break;
            case '=': return greaterthanorequals_token;
            default:  retract(); return greaterthan_token;
          }

        /**
         * prefix: >>
        **/

        case rightshift_state:
          switch ( nextchar() ) {
            case '>': state = unsignedrightshift_state; break;
            case '=': return rightshiftassign_token;
            default:  retract(); return rightshift_token;
          }

        /**
         * prefix: >>>
        **/

        case unsignedrightshift_state:
          switch ( nextchar() ) {
            case '=': return unsignedrightshiftassign_token;
            default:  retract(); return unsignedrightshift_token;
          }

        /**
         * prefix: a
        **/

        case a_state:
          switch ( nextchar() ) {
            case 'b': state = ab_state; continue;
            case 't': state = at_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: ab
        **/

        case ab_state:
          switch ( nextchar() ) {
            case 's': state = abs_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: abs
        **/

        case abs_state:
          switch ( nextchar() ) {
            case 't': state = abst_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: abst
        **/

        case abst_state:
          switch ( nextchar() ) {
            case 'r': state = abstr_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: abstr
        **/

        case abstr_state:
          switch ( nextchar() ) {
            case 'a': state = abstra_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: abstra
        **/

        case abstra_state:
          switch ( nextchar() ) {
            case 'c': state = abstrac_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: abstrac
        **/

        case abstrac_state:
          switch ( nextchar() ) {
            case 't': state = abstract_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: abstract
        **/

        case abstract_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return abstract_token;
          }

        /**
         * prefix: at
        **/

        case at_state:
          switch ( nextchar() ) {
            case 't': state = att_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: att
        **/

        case att_state:
          switch ( nextchar() ) {
            case 'r': state = attr_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: attr
        **/

        case attr_state:
          switch ( nextchar() ) {
            case 'i': state = attri_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: attri
        **/

        case attri_state:
          switch ( nextchar() ) {
            case 'b': state = attrib_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: attrib
        **/

        case attrib_state:
          switch ( nextchar() ) {
            case 'u': state = attribu_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: attribu
        **/

        case attribu_state:
          switch ( nextchar() ) {
            case 't': state = attribut_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: attribut
        **/

        case attribut_state:
          switch ( nextchar() ) {
            case 'e': state = attribute_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: attribute
        **/

        case attribute_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return attribute_token;
          }

        /**
         * prefix: b
        **/

        case b_state:
          switch ( nextchar() ) {
            case 'r': state = br_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: bo
        **/

        case bo_state:
          switch ( nextchar() ) {
            case 'o': state = boo_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: boo
        **/

        case boo_state:
          switch ( nextchar() ) {
            case 'l': state = bool_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: bool
        **/

        case bool_state:
          switch ( nextchar() ) {
            case 'e': state = boole_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: boole
        **/

        case boole_state:
          switch ( nextchar() ) {
            case 'a': state = boolea_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: boolea
        **/

        case boolea_state:
          switch ( nextchar() ) {
            case 'n': state = boolean_state; continue;
            default:  retract(); state = A_state; continue;
          }

         /**
         * prefix: boolean
        **/

        case boolean_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return boolean_token;
          }

        /**
         * prefix: br
        **/

        case br_state:
          switch ( nextchar() ) {
            case 'e': state = bre_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: bre
        **/

        case bre_state:
          switch ( nextchar() ) {
            case 'a': state = brea_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: brea
        **/

        case brea_state:
          switch ( nextchar() ) {
            case 'k': state = break_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: break
        **/

        case break_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return break_token;
          }

        /**
         * prefix: by
        **/

        case by_state:
          switch ( nextchar() ) {
            case 't': state = byt_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: byt
        **/

        case byt_state:
          switch ( nextchar() ) {
            case 'e': state = byte_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: byte
        **/

        case byte_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return byte_token;
          }

        /**
         * prefix: c
        **/

        case c_state:
          switch ( nextchar() ) {
            case 'a': state = ca_state; continue;
            case 'l': state = cl_state; continue;
            case 'o': state = co_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: ca
        **/

        case ca_state:
          switch ( nextchar() ) {
            case 's': state = cas_state; continue;
            case 't': state = cat_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: cas
        **/

        case cas_state:
          switch ( nextchar() ) {
            case 'e': state = case_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: case
        **/

        case case_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return case_token;
          }

        /**
         * prefix: cat
        **/

        case cat_state:
          switch ( nextchar() ) {
            case 'c': state = catc_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: catc
        **/

        case catc_state:
          switch ( nextchar() ) {
            case 'h': state = catch_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: catch
        **/

        case catch_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return catch_token;
          }

        /**
         * prefix: ch
        **/

        case ch_state:
          switch ( nextchar() ) {
            case 'a': state = cha_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: cha
        **/

        case cha_state:
          switch ( nextchar() ) {
            case 'r': state = char_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: char
        **/

        case char_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return char_token;
          }

        /**
         * prefix: cl
        **/

        case cl_state:
          switch ( nextchar() ) {
            case 'a': state = cla_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: cla
        **/

        case cla_state:
          switch ( nextchar() ) {
            case 's': state = clas_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: clas
        **/

        case clas_state:
          switch ( nextchar() ) {
            case 's': state = class_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: class
        **/

        case class_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return class_token;
          }

        /**
         * prefix: co
        **/

        case co_state:
          switch ( nextchar() ) {
            case 'n': state = con_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: con
        **/

        case con_state:
          switch ( nextchar() ) {
            case 's': state = cons_state; continue;
            case 't': state = cont_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: cons
        **/

        case cons_state:
          switch ( nextchar() ) {
            case 't': state = const_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: const
        **/

        case const_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r':
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return const_token;
          }

        /**
         * prefix: cont
        **/

        case cont_state:
          switch ( nextchar() ) {
            case 'i': state = conti_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: conti
        **/

        case conti_state:
          switch ( nextchar() ) {
            case 'n': state = contin_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: contin
        **/

        case contin_state:
          switch ( nextchar() ) {
            case 'u': state = continu_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: continu
        **/

        case continu_state:
          switch ( nextchar() ) {
            case 'e': state = continue_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: continue
        **/

        case continue_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return continue_token;
          }

        /**
         * prefix: d
        **/

        case d_state:
          switch ( nextchar() ) {
            case 'e': state = de_state; continue;
            case 'o': state = do_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: de
        **/

        case de_state:
          switch ( nextchar() ) {
            case 'b': state = deb_state; continue;
            case 'f': state = def_state; continue;
            case 'l': state = del_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: deb
        **/

        case deb_state:
          switch ( nextchar() ) {
            case 'u': state = debu_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: debu
        **/

        case debu_state:
          switch ( nextchar() ) {
            case 'g': state = debug_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: debug
        **/

        case debug_state:
          switch ( nextchar() ) {
            case 'g': state = debugg_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: debugg
        **/

        case debugg_state:
          switch ( nextchar() ) {
            case 'e': state = debugge_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: debugge
        **/

        case debugge_state:
          switch ( nextchar() ) {
            case 'r': state = debugger_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: debugger
        **/

        case debugger_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return debugger_token;
          }

        /**
         * prefix: def
        **/

        case def_state:
          switch ( nextchar() ) {
            case 'a': state = defa_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: defa
        **/

        case defa_state:
          switch ( nextchar() ) {
            case 'u': state = defau_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: defau
        **/

        case defau_state:
          switch ( nextchar() ) {
            case 'l': state = defaul_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: defaul
        **/

        case defaul_state:
          switch ( nextchar() ) {
            case 't': state = default_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: default
        **/

        case default_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return default_token;
          }

        /**
         * prefix: del
        **/

        case del_state:
          switch ( nextchar() ) {
            case 'e': state = dele_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: dele
        **/

        case dele_state:
          switch ( nextchar() ) {
            case 't': state = delet_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: delet
        **/

        case delet_state:
          switch ( nextchar() ) {
            case 'e': state = delete_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: delete
        **/

        case delete_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return delete_token;
          }

        /**
         * prefix: do
        **/

        case do_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u':
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return do_token;
          }

        /**
         * prefix: dou
        **/

        case dou_state:
          switch ( nextchar() ) {
            case 'b': state = doub_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: doub
        **/

        case doub_state:
          switch ( nextchar() ) {
            case 'l': state = doubl_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: doubl
        **/

        case doubl_state:
          switch ( nextchar() ) {
            case 'e': state = double_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: double
        **/

        case double_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return double_token;
          }

        /**
         * prefix: e
        **/

        case e_state:
          switch ( nextchar() ) {
            case 'l': state = el_state; continue;
            case 'n': state = en_state; continue;
            case 'v': state = ev_state; continue;
            case 'x': state = ex_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: el
        **/

        case el_state:
          switch ( nextchar() ) {
            case 's': state = els_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: els
        **/

        case els_state:
          switch ( nextchar() ) {
            case 'e': state = else_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: else
        **/

        case else_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return else_token;
          }

        /**
         * prefix: en
        **/

        case en_state:
          switch ( nextchar() ) {
            case 'u': state = enu_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: enu
        **/

        case enu_state:
          switch ( nextchar() ) {
            case 'm': state = enum_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: enum
        **/

        case enum_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return enum_token;
          }

        /**
         * prefix: ev
        **/

        case ev_state:
          switch ( nextchar() ) {
            case 'a': state = eva_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: eva
        **/

        case eva_state:
          switch ( nextchar() ) {
            case 'l': state = eval_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: eval
        **/

        case eval_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return eval_token;
          }

        /**
         * prefix: ex
        **/

        case ex_state:
          switch ( nextchar() ) {
            case 'p': state = exp_state; continue;
            case 't': state = ext_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: exp
        **/

        case exp_state:
          switch ( nextchar() ) {
            case 'o': state = expo_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: expo
        **/

        case expo_state:
          switch ( nextchar() ) {
            case 'r': state = expor_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: expor
        **/

        case expor_state:
          switch ( nextchar() ) {
            case 't': state = export_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: export
        **/

        case export_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return export_token;
          }

        /**
         * prefix: ext
        **/

        case ext_state:
          switch ( nextchar() ) {
            case 'e': state = exte_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: exte
        **/

        case exte_state:
          switch ( nextchar() ) {
            case 'n': state = exten_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: exten
        **/

        case exten_state:
          switch ( nextchar() ) {
            case 'd': state = extend_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: extend
        **/

        case extend_state:
          switch ( nextchar() ) {
            case 's': state = extends_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: extends
        **/

        case extends_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return extends_token;
          }

        /**
         * prefix: f
        **/

        case f_state:
          switch ( nextchar() ) {
            case 'a': state = fa_state; continue;
            case 'i': state = fi_state; continue;
            case 'o': state = fo_state; continue;
            case 'u': state = fu_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: fa
        **/

        case fa_state:
          switch ( nextchar() ) {
            case 'l': state = fal_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: fal
        **/

        case fal_state:
          switch ( nextchar() ) {
            case 's': state = fals_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: fals
        **/

        case fals_state:
          switch ( nextchar() ) {
            case 'e': state = false_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: false
        **/

        case false_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return false_token;
          }

        /**
         * prefix: fi
        **/

        case fi_state:
          switch ( nextchar() ) {
            case 'n': state = fin_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: fin
        **/

        case fin_state:
          switch ( nextchar() ) {
            case 'a': state = fina_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: fina
        **/

        case fina_state:
          switch ( nextchar() ) {
            case 'l': state = final_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: final
        **/

        case final_state:
          switch ( nextchar() ) {
            case 'l': state = finall_state; continue;
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default:  retract(); return final_token;
          }

        /**
         * prefix: finall
        **/

        case finall_state:
          switch ( nextchar() ) {
            case 'y': state = finally_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: finally
        **/

        case finally_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return finally_token;
          }

        /**
         * prefix: fl
        **/

        case fl_state:
          switch ( nextchar() ) {
            case 'o': state = flo_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: flo
        **/

        case flo_state:
          switch ( nextchar() ) {
            case 'a': state = floa_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: floa
        **/

        case floa_state:
          switch ( nextchar() ) {
            case 't': state = float_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: float
        **/

        case float_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return float_token;
          }

        /**
         * prefix: fo
        **/

        case fo_state:
          switch ( nextchar() ) {
            case 'r': state = for_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: for
        **/

        case for_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return for_token;
          }

        /**
         * prefix: fu
        **/

        case fu_state:
          switch ( nextchar() ) {
            case 'n': state = fun_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: fun
        **/

        case fun_state:
          switch ( nextchar() ) {
            case 'c': state = func_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: func
        **/

        case func_state:
          switch ( nextchar() ) {
            case 't': state = funct_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: funct
        **/

        case funct_state:
          switch ( nextchar() ) {
            case 'i': state = functi_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: functi
        **/

        case functi_state:
          switch ( nextchar() ) {
            case 'o': state = functio_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: functio
        **/

        case functio_state:
          switch ( nextchar() ) {
            case 'n': state = function_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: function
        **/

        case function_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return function_token;
          }

        /**
         * prefix: g
        **/

        case g_state:
          switch ( nextchar() ) {
            case 'o': state = go_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: go
        **/

        case go_state:
          switch ( nextchar() ) {
            case 't': state = got_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: got
        **/

        case got_state:
          switch ( nextchar() ) {
            case 'o': state = goto_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: goto
        **/

        case goto_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return goto_token;
          }

        /**
         * prefix: i
        **/

        case i_state:
          switch ( nextchar() ) {
            case 'f': state = if_state; continue;
            case 'm': state = im_state; continue;
            case 'n': state = in_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: if
        **/

        case if_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return if_token;
          }

        /**
         * prefix: im
        **/

        case im_state:
          switch ( nextchar() ) {
            case 'p': state = imp_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: imp
        **/

        case imp_state:
          switch ( nextchar() ) {
            case 'l': state = impl_state; continue;
            case 'o': state = impo_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: impl
        **/

        case impl_state:
          switch ( nextchar() ) {
            case 'e': state = imple_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: imple
        **/

        case imple_state:
          switch ( nextchar() ) {
            case 'm': state = implem_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: implem
        **/

        case implem_state:
          switch ( nextchar() ) {
            case 'e': state = impleme_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: impleme
        **/

        case impleme_state:
          switch ( nextchar() ) {
            case 'n': state = implemen_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: implemen
        **/

        case implemen_state:
          switch ( nextchar() ) {
            case 't': state = implement_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: implement
        **/

        case implement_state:
          switch ( nextchar() ) {
            case 's': state = implements_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: implements
        **/

        case implements_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return implements_token;
          }

        /**
         * prefix: impo
        **/

        case impo_state:
          switch ( nextchar() ) {
            case 'r': state = impor_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: impor
        **/

        case impor_state:
          switch ( nextchar() ) {
            case 't': state = import_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: import
        **/

        case import_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return import_token;
          }

        /**
         * prefix: in
        **/

        case in_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S':           case 'T':           case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            case 'c': state = inc_state; continue;
            case 's': state = ins_state; continue;
            case 't': state = int_state; continue;
            default: retract(); return in_token;
          }

        /**
         * prefix: inc
        **/

        case inc_state:
          switch ( nextchar() ) {
            case 'l': state = incl_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: incl
        **/

        case incl_state:
          switch ( nextchar() ) {
            case 'u': state = inclu_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: inclu
        **/

        case inclu_state:
          switch ( nextchar() ) {
            case 'd': state = includ_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: includ
        **/

        case includ_state:
          switch ( nextchar() ) {
            case 'e': state = include_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: include
        **/

        case include_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return include_token;
          }

        /**
         * prefix: ins
        **/

        case ins_state:
          switch ( nextchar() ) {
            case 't': state = inst_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: inst
        **/

        case inst_state:
          switch ( nextchar() ) {
            case 'a': state = insta_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: insta
        **/

        case insta_state:
          switch ( nextchar() ) {
            case 'n': state = instan_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: instan
        **/

        case instan_state:
          switch ( nextchar() ) {
            case 'c': state = instanc_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: instanc
        **/

        case instanc_state:
          switch ( nextchar() ) {
            case 'e': state = instance_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: instance
        **/

        case instance_state:
          switch ( nextchar() ) {
            case 'o': state = instanceo_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: instanceo
        **/

        case instanceo_state:
          switch ( nextchar() ) {
            case 'f': state = instanceof_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: instanceof
        **/

        case instanceof_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return instanceof_token;
          }

        /**
         * prefix: int
        **/

        case int_state:
          switch ( nextchar() ) {
            case 'e': state = inte_state; continue;
            default: retract(); state = A_state; continue;
          }

        /**
         * prefix: inte
        **/

        case inte_state:
          switch ( nextchar() ) {
            case 'r': state = inter_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: inter
        **/

        case inter_state:
          switch ( nextchar() ) {
            case 'f': state = interf_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: interf
        **/

        case interf_state:
          switch ( nextchar() ) {
            case 'a': state = interfa_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: interfa
        **/

        case interfa_state:
          switch ( nextchar() ) {
            case 'c': state = interfac_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: interfac
        **/

        case interfac_state:
          switch ( nextchar() ) {
            case 'e': state = interface_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: interface
        **/

        case interface_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return interface_token;
          }

        /**
         * prefix: l
        **/

        case l_state:
          switch ( nextchar() ) {
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: lo
        **/

        case lo_state:
          switch ( nextchar() ) {
            case 'n': state = lon_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: lon
        **/

        case lon_state:
          switch ( nextchar() ) {
            case 'g': state = long_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: long
        **/

        case long_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return long_token;
          }

        /**
         * prefix: n
        **/

        case n_state:
          switch ( nextchar() ) {
            case 'a': state = na_state; continue;
            case 'e': state = ne_state; continue;
            case 'u': state = nu_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: na
        **/

        case na_state:
          switch ( nextchar() ) {
            case 't': state = nat_state; continue;
            case 'm': state = nam_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: nam
        **/

        case nam_state:
          switch ( nextchar() ) {
            case 'e': state = name_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: name
        **/

        case name_state:
          switch ( nextchar() ) {
            case 's': state = names_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: names
        **/

        case names_state:
          switch ( nextchar() ) {
            case 'p': state = namesp_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: namesp
        **/

        case namesp_state:
          switch ( nextchar() ) {
            case 'a': state = namespa_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: namespa
        **/

        case namespa_state:
          switch ( nextchar() ) {
            case 'c': state = namespac_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: namespac
        **/

        case namespac_state:
          switch ( nextchar() ) {
            case 'e': state = namespace_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: namespace
        **/

        case namespace_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return namespace_token;
          }

        /**
         * prefix: nat
        **/

        case nat_state:
          switch ( nextchar() ) {
            case 'i': state = nati_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: nati
        **/

        case nati_state:
          switch ( nextchar() ) {
            case 'v': state = nativ_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: nativ
        **/

        case nativ_state:
          switch ( nextchar() ) {
            case 'e': state = native_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: native
        **/

        case native_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return native_token;
          }

        /**
         * prefix: ne
        **/

        case ne_state:
          switch ( nextchar() ) {
            case 'w': state = new_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: new
        **/

        case new_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return new_token;
          }

        /**
         * prefix: nu
        **/

        case nu_state:
          switch ( nextchar() ) {
            case 'l': state = nul_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: nul
        **/

        case nul_state:
          switch ( nextchar() ) {
            case 'l': state = null_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: null
        **/

        case null_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return null_token;
          }

        /**
         * prefix: p
        **/

        case p_state:
          switch ( nextchar() ) {
            case 'a': state = pa_state; continue;
            case 'r': state = pr_state; continue;
            case 'u': state = pu_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: pa
        **/

        case pa_state:
          switch ( nextchar() ) {
            case 'c': state = pac_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: pac
        **/

        case pac_state:
          switch ( nextchar() ) {
            case 'k': state = pack_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: pack
        **/

        case pack_state:
          switch ( nextchar() ) {
            case 'a': state = packa_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: packa
        **/

        case packa_state:
          switch ( nextchar() ) {
            case 'g': state = packag_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: packag
        **/

        case packag_state:
          switch ( nextchar() ) {
            case 'e': state = package_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: package
        **/

        case package_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return package_token;
          }

        /**
         * prefix: pr
        **/

        case pr_state:
          switch ( nextchar() ) {
            case 'i': state = pri_state; continue;
            case 'o': state = pro_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: pri
        **/

        case pri_state:
          switch ( nextchar() ) {
            case 'v': state = priv_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: priv
        **/

        case priv_state:
          switch ( nextchar() ) {
            case 'a': state = priva_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: priva
        **/

        case priva_state:
          switch ( nextchar() ) {
            case 't': state = privat_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: privat
        **/

        case privat_state:
          switch ( nextchar() ) {
            case 'e': state = private_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: private
        **/

        case private_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return private_token;
          }

        /**
         * prefix: pro
        **/

        case pro_state:
          switch ( nextchar() ) {
            case 't': state = prot_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: prot
        **/

        case prot_state:
          switch ( nextchar() ) {
            case 'e': state = prote_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: prote
        **/

        case prote_state:
          switch ( nextchar() ) {
            case 'c': state = protec_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: protec
        **/

        case protec_state:
          switch ( nextchar() ) {
            case 't': state = protect_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: protect
        **/

        case protect_state:
          switch ( nextchar() ) {
            case 'e': state = protecte_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: protecte
        **/

        case protecte_state:
          switch ( nextchar() ) {
            case 'd': state = protected_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: protected
        **/

        case protected_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return protected_token;
          }

        /**
         * prefix: public
        **/

        case pu_state:
          switch ( nextchar() ) {
            case 'b': state = pub_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: pub
        **/

        case pub_state:
          switch ( nextchar() ) {
            case 'l': state = publ_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: publ
        **/

        case publ_state:
          switch ( nextchar() ) {
            case 'i': state = publi_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: publi
        **/

        case publi_state:
          switch ( nextchar() ) {
            case 'c': state = public_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: public
        **/

        case public_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return public_token;
          }

        /**
         * prefix: r
        **/

        case r_state:
          switch ( nextchar() ) {
            case 'e': state = re_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: re
        **/

        case re_state:
          switch ( nextchar() ) {
            case 't': state = ret_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: ret
        **/

        case ret_state:
          switch ( nextchar() ) {
            case 'u': state = retu_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: retu
        **/

        case retu_state:
          switch ( nextchar() ) {
            case 'r': state = retur_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: retur
        **/

        case retur_state:
          switch ( nextchar() ) {
            case 'n': state = return_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: return
        **/

        case return_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return return_token;
          }

        /**
         * prefix: s
        **/

        case s_state:
          switch ( nextchar() ) {
            case 't': state = st_state; continue;
            case 'u': state = su_state; continue;
            case 'w': state = sw_state; continue;
            case 'y': state = sy_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: sh
        **/

        case sh_state:
          switch ( nextchar() ) {
            case 'o': state = sho_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: sho
        **/

        case sho_state:
          switch ( nextchar() ) {
            case 'r': state = shor_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: shor
        **/

        case shor_state:
          switch ( nextchar() ) {
            case 't': state = short_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: short
        **/

        case short_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return short_token;
          }

        /**
         * prefix: st
        **/

        case st_state:
          switch ( nextchar() ) {
            case 'a': state = sta_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: sta
        **/

        case sta_state:
          switch ( nextchar() ) {
            case 't': state = stat_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: stat
        **/

        case stat_state:
          switch ( nextchar() ) {
            case 'i': state = stati_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: stati
        **/

        case stati_state:
          switch ( nextchar() ) {
            case 'c': state = static_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: static
        **/

        case static_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return static_token;
          }

        /**
         * prefix: su
        **/

        case su_state:
          switch ( nextchar() ) {
            case 'p': state = sup_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: sup
        **/

        case sup_state:
          switch ( nextchar() ) {
            case 'e': state = supe_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: supe
        **/

        case supe_state:
          switch ( nextchar() ) {
            case 'r': state = super_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: super
        **/

        case super_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return super_token;
          }

        /**
         * prefix: sw
        **/

        case sw_state:
          switch ( nextchar() ) {
            case 'i': state = swi_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: swi
        **/

        case swi_state:
          switch ( nextchar() ) {
            case 't': state = swit_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: swit
        **/

        case swit_state:
          switch ( nextchar() ) {
            case 'c': state = switc_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: switc
        **/

        case switc_state:
          switch ( nextchar() ) {
            case 'h': state = switch_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: switch
        **/

        case switch_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return switch_token;
          }

        /**
         * prefix: sy
        **/

        case sy_state:
          switch ( nextchar() ) {
            case 'n': state = syn_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: syn
        **/

        case syn_state:
          switch ( nextchar() ) {
            case 'c': state = sync_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: sync
        **/

        case sync_state:
          switch ( nextchar() ) {
            case 'h': state = synch_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: synch
        **/

        case synch_state:
          switch ( nextchar() ) {
            case 'r': state = synchr_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: synchr
        **/

        case synchr_state:
          switch ( nextchar() ) {
            case 'o': state = synchro_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: synchro
        **/

        case synchro_state:
          switch ( nextchar() ) {
            case 'n': state = synchron_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: synchron
        **/

        case synchron_state:
          switch ( nextchar() ) {
            case 'i': state = synchroni_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: synchroni
        **/

        case synchroni_state:
          switch ( nextchar() ) {
            case 'z': state = synchroniz_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: synchroniz
        **/

        case synchroniz_state:
          switch ( nextchar() ) {
            case 'e': state = synchronize_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: synchronize
        **/

        case synchronize_state:
          switch ( nextchar() ) {
            case 'd': state = synchronized_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: synchronized
        **/

        case synchronized_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return synchronized_token;
          }

        /**
         * prefix: t
        **/

        case t_state:
          switch ( nextchar() ) {
            case 'h': state = th_state; continue;
            case 'r': state = tr_state; continue;
            case 'y': state = ty_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: th
        **/

        case th_state:
          switch ( nextchar() ) {
            case 'i': state = thi_state; continue;
            case 'r': state = thr_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: thi
        **/

        case thi_state:
          switch ( nextchar() ) {
            case 's': state = this_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: this
        **/

        case this_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return this_token;
          }

        /**
         * prefix: thr
        **/

        case thr_state:
          switch ( nextchar() ) {
            case 'o': state = thro_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: thro
        **/

        case thro_state:
          switch ( nextchar() ) {
            case 'w': state = throw_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: throw
        **/

        case throw_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S':           case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            case 's': state = throws_state; continue;
            default: retract(); return throw_token;
          }

        /**
         * prefix: throws
        **/

        case throws_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return throws_token;
          }

        /**
         * prefix: tr
        **/

        case tr_state:
          switch ( nextchar() ) {
            case 'a': state = tra_state; continue;
            case 'u': state = tru_state; continue;
            case 'y': state = try_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: tra
        **/

        case tra_state:
          switch ( nextchar() ) {
            case 'n': state = tran_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: tran
        **/

        case tran_state:
          switch ( nextchar() ) {
            case 's': state = trans_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: trans
        **/

        case trans_state:
          switch ( nextchar() ) {
            case 'i': state = transi_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: transi
        **/

        case transi_state:
          switch ( nextchar() ) {
            case 'e': state = transie_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: transie
        **/

        case transie_state:
          switch ( nextchar() ) {
            case 'n': state = transien_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: transien
        **/

        case transien_state:
          switch ( nextchar() ) {
            case 't': state = transient_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: transient
        **/

        case transient_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return transient_token;
          }

        /**
         * prefix: tru
        **/

        case tru_state:
          switch ( nextchar() ) {
            case 'e': state = true_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: true
        **/

        case true_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return true_token;
          }

        /**
         * prefix: try
        **/

        case try_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return try_token;
          }

        /**
         * prefix: ty
        **/

        case ty_state:
          switch ( nextchar() ) {
            case 'p': state = typ_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: typ
        **/

        case typ_state:
          switch ( nextchar() ) {
            case 'e': state = type_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: type
        **/

        case type_state:
          switch ( nextchar() ) {
            case 'o': state = typeo_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: typeo
        **/

        case typeo_state:
          switch ( nextchar() ) {
            case 'f': state = typeof_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: typeof
        **/

        case typeof_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return typeof_token;
          }

        /**
         * prefix: u
        **/

        case u_state:
          switch ( nextchar() ) {
            case 's': state = us_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: us
        **/

        case us_state:
          switch ( nextchar() ) {
            case 'e': state = use_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: use
        **/

        case use_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return use_token;
          }

        /**
         * prefix: v
        **/

        case v_state:
          switch ( nextchar() ) {
            case 'a': state = va_state; continue;
            case 'o': state = vo_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: va
        **/

        case va_state:
          switch ( nextchar() ) {
            case 'r': state = var_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: var
        **/

        case var_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return var_token;
          }

        /**
         * prefix: vo
        **/

        case vo_state:
          switch ( nextchar() ) {
            case 'i': state = voi_state; continue;
            case 'l': state = vol_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: voi
        **/

        case voi_state:
          switch ( nextchar() ) {
            case 'd': state = void_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: void
        **/

        case void_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return void_token;
          }

        /**
         * prefix: vol
        **/

        case vol_state:
          switch ( nextchar() ) {
            case 'a': state = vola_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: vola
        **/

        case vola_state:
          switch ( nextchar() ) {
            case 't': state = volat_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: volat
        **/

        case volat_state:
          switch ( nextchar() ) {
            case 'i': state = volati_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: volati
        **/

        case volati_state:
          switch ( nextchar() ) {
            case 'l': state = volatil_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: volatil
        **/

        case volatil_state:
          switch ( nextchar() ) {
            case 'e': state = volatile_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: volatile
        **/

        case volatile_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return volatile_token;
          }

        /**
         * prefix: w
        **/

        case w_state:
          switch ( nextchar() ) {
            case 'h': state = wh_state; continue;
            case 'i': state = wi_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: wh
        **/

        case wh_state:
          switch ( nextchar() ) {
            case 'i': state = whi_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: whi
        **/

        case whi_state:
          switch ( nextchar() ) {
            case 'l': state = whil_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: whil
        **/

        case whil_state:
          switch ( nextchar() ) {
            case 'e': state = while_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: while
        **/

        case while_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return while_token;
          }

        /**
         * prefix: wi
        **/

        case wi_state:
          switch ( nextchar() ) {
            case 't': state = wit_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: wit
        **/

        case wit_state:
          switch ( nextchar() ) {
            case 'h': state = with_state; continue;
            default:  retract(); state = A_state; continue;
          }

        /**
         * prefix: with
        **/

        case with_state:
          switch ( nextchar() ) {
            case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': 
            case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': 
            case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': 
            case 'M': case 'm': case 'N': case 'n': case 'O': case 'o':
            case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': 
            case 'S': case 's': case 'T': case 't': case 'U': case 'u': 
            case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': 
            case 'Y': case 'y': case 'Z': case 'z': case '$': case '_':
            case '0': case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9':
                      state = A_state; continue;
            default: retract(); return with_token;
          }

        /**
         * prefix: /*
        **/

        case blockcomment_state:
          switch ( nextchar() ) {
            case '*':  state = blockcommentstar_state; continue;
            case '\n': isFirstTokenOnLine = true; 
                       state = blockcommentstar_state; continue;
            case 0:    error(lexical_error); return error_token;
            default:   state = blockcomment_state; continue;
          }

        case blockcommentstar_state:
          switch ( nextchar() ) {
            case '/':  state = start_state; /*first = input.positionOfNext();*/ continue;
            case 0:    error(lexical_error); return error_token;
            default:   state = blockcomment_state; continue;
			  // if not a slash, then keep looking for an end comment.
          }

        /**
         * prefix: // <comment chars>
        **/

        case linecomment_state:
          switch ( nextchar() ) {
            case '\n': retract(); state = start_state; /*first = input.positionOfNext();*/ continue;
			  // don't include newline in line comment. (Sec 7.3)
            case 0:    error(lexical_error); return error_token;
            default:   state = linecomment_state; continue;
          }

        /**
         * skip error
        **/

        case error_state:
          error(lexical_error); skiperror();
          return error_token;

        default: state = error_state; continue;

      }
    }
  }

  /**
   *
   */

  static final void test_nexttoken() throws Exception {

    System.out.println( "nexttoken test begin" );

    String[] input = { 
        "true", 
        "false", 
        "null", 
        "abc",
        "\"a1b2\"" , 
        "\'a1b2\'", 
        "\'a1b2\"",
        "<<=", 
        "<=", 
        "0",
        "123", 
        "1.23",
        "/abc/gim",
        "/ab" };

    int[] expected = {
         
        true_token, false_token, null_token, identifier_token,
        stringliteral_token, stringliteral_token, error_token,
        leftshiftassign_token, lessthanorequals_token,
        numberliteral_token, numberliteral_token, numberliteral_token, 
        regexpliteral_token,
        error_token };

    Scanner scanner;
    int result;
    for( int i = 0; i < input.length; i++ ) {
      scanner = new Scanner( new StringReader(input[i]),System.err,"" );
      result  = scanner.nexttoken();
      if( expected[i] == scanner.getTokenClass(result) )
        System.out.println( "  " + i + " passed: " + input[i] + " --> " + 
                    Token.getTokenClassName(scanner.getTokenClass(result)) + 
                    ": " + scanner.getTokenText(result) );
      else {
        failureCount++;
        System.out.println( "  " + i + " failed: " + input[i] + " --> " + 
                    Token.getTokenClassName(scanner.getTokenClass(result)) + 
                    ": " + scanner.getTokenText(result) );
      }
    }

    System.out.println( "nexttoken test complete" );
  }
}

/*
 * The end.
 */





