/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Created: Eric Bina <ebina@netscape.com>, 30 Oct 1997.
 */

package grendel.storage.addressparser;

import java.io.*;
import java.util.*;


// Class to tokenize a RFC822 header-body.
// The class is initialized by passing the header-body as
// a string, and it immediatly attempts to tokenize
// the string into the following tokens as defined by rfc822
//  atom
//  special character
//  quoted string
//  domain literal
//  comment
class RFC822Tokenizer
{
  // Various constants defining parsing states.
  static final int AT_END = 0;
  static final int IN_NOTHING = 1;
  static final int IN_ATOM = 2;
  static final int IN_COMMENT = 3;
  static final int IN_DOMAIN_LITERAL = 4;
  static final int IN_QUOTED_TEXT = 5;
  static final int IN_SPECIAL = 6;

  // Important characters to switch states on.
  static final char BEGIN_COMMENT = '(';
  static final char END_COMMENT = ')';
  static final char BEGIN_DOMAIN_LITERAL = '[';
  static final char END_DOMAIN_LITERAL = ']';
  static final char BEGIN_QUOTE = '\"';
  static final char END_QUOTE = '\"';
  static final char BEGIN_QUOTE_PAIR = '\\';
  static final char CARRIAGE_RETURN = '\r';

  private StringStream sstr;
  private Vector tokens;

  public RFC822Tokenizer(String str)
  {
    int parse_state = IN_NOTHING;
    int len = str.length();
    char t_char;

    // Create to vector to store the tokenized output.
    // Wrapped the passed string in a class to feed
    // it through the parser.
    this.tokens = new Vector();
    this.sstr = new StringStream(str);
    t_char = this.sstr.currentChar();

    // This while loop is the main body of the parser.
    while ((parse_state != AT_END)&&(this.sstr.currentIndex() < len)) {
      int indx1, indx2;

      switch(parse_state) {
      case IN_NOTHING:
        parse_state = skipNothing();
        break;
      case IN_ATOM:
        indx1 = this.sstr.currentIndex();
        parse_state = skipAtom();
        indx2 = this.sstr.currentIndex();
        addAtom(str, indx1, indx2);
        break;
      case IN_COMMENT:
        indx1 = this.sstr.currentIndex();
        t_char = this.sstr.nextChar();
        parse_state = skipComment();
        t_char = this.sstr.nextChar();
        indx2 = this.sstr.currentIndex();
        addComment(str, indx1, indx2);
        break;
      case IN_DOMAIN_LITERAL:
        indx1 = this.sstr.currentIndex();
        t_char = this.sstr.nextChar();
        parse_state = skipDomainLiteral();
        t_char = this.sstr.nextChar();
        indx2 = this.sstr.currentIndex();
        addDomainLiteral(str, indx1, indx2);
        break;
      case IN_QUOTED_TEXT:
        indx1 = this.sstr.currentIndex();
        t_char = this.sstr.nextChar();
        parse_state = skipQuotedText();
        t_char = this.sstr.nextChar();
        indx2 = this.sstr.currentIndex();
        addQuotedText(str, indx1, indx2);
        break;
      case IN_SPECIAL:
        indx1 = this.sstr.currentIndex();
        t_char = this.sstr.nextChar();
        indx2 = this.sstr.currentIndex();
        addSpecial(str, indx1, indx2);
        parse_state = IN_NOTHING;
        break;
      default:
        break;
      }
    }
  }


  public Vector getTokens()
  {
    return(this.tokens);
  }


  public void showVector()
  {
    Vector vec = this.tokens;
    int num = vec.size();

    for (int indx=0; indx < num; indx++)
    {
      RFC822Token token;
      String str;

      token = (RFC822Token)vec.elementAt(indx);
      str = (String)token.getObject();
      System.out.print("{" + str + "}->");
      token.printTokenType();
      System.out.println(" ");
    }
    System.out.println("\n");
  }



  /*********************
   *********************
   ** Private methods **
   *********************
   *********************/

  /*
   * Methods to add tokens to the output vector.
   */

  // Wrap the atom token substring in a RFC822Token object
  // and add it to the output vector.
  private void addAtom(String str, int indx1, int indx2)
  {
    RFC822Token token;
    String substr = str.substring(indx1, indx2);

    token = new RFC822Token(substr, RFC822Token.ATOM);
    this.tokens.addElement(token);
  }

  // Wrap the comment token substring in a RFC822Token object
  // and add it to the output vector.
  private void addComment(String str, int indx1, int indx2)
  {
    RFC822Token token;
    String substr = str.substring(indx1, indx2);

    token = new RFC822Token(substr, RFC822Token.COMMENT);
    this.tokens.addElement(token);
  }

  // Wrap the domain-literal token substring in a RFC822Token object
  // and add it to the output vector.
  private void addDomainLiteral(String str, int indx1, int indx2)
  {
    RFC822Token token;
    String substr = str.substring(indx1, indx2);

    token = new RFC822Token(substr, RFC822Token.DOMAIN_LITERAL);
    this.tokens.addElement(token);
  }

  // Wrap the quoted-string token substring in a RFC822Token object
  // and add it to the output vector.
  private void addQuotedText(String str, int indx1, int indx2)
  {
    RFC822Token token;
    String substr = str.substring(indx1, indx2);

    token = new RFC822Token(substr, RFC822Token.QUOTED_STRING);
    this.tokens.addElement(token);
  }

  // Wrap the special character token substring in a RFC822Token object
  // and add it to the output vector.
  // Should use a Character object here instead.
  private void addSpecial(String str, int indx1, int indx2)
  {
    RFC822Token token;
    String substr = str.substring(indx1, indx2);

    token = new RFC822Token(substr, RFC822Token.SPECIAL_CHAR);
    this.tokens.addElement(token);
  }


  /*
   * Methods to test conditions.
   */

  // Test if the passed character is one of the rfc822
  // special characters.
  private boolean isSpecial(char t_char)
  {
    if ((t_char == '(')||
        (t_char == ')')||
        (t_char == '<')||
        (t_char == '>')||
        (t_char == '@')||
        (t_char == ',')||
        (t_char == ';')||
        (t_char == ':')||
        (t_char == '"')||
        (t_char == '.')||
        (t_char == '[')||
        (t_char == ']')||
        (t_char == '\\'))
      return true;

    return false;
  }


  /*
   * Methods to test if we remain within character set types.
   */

  // Test if we are still in the outer parse state.
  private boolean inNothing(char t_char)
  {
    if (Character.isSpaceChar(t_char))
      return true;

    if (Character.isISOControl(t_char))
      return true;

    return false;
  }


  // Test if we are still in the atom parse state.
  private boolean inAtom(char t_char)
  {
    // Should test for just ASCII 32
    if (Character.isSpaceChar(t_char))
      return false;

    // Should test for ASCII 0 - 31 inclusive, and
    // DEL (ASCII 127).
    if (Character.isISOControl(t_char))
      return false;

    if (isSpecial(t_char))
      return false;

    return true;
  }


  // Test if we are still in the comment parse state.
  private boolean inComment(char t_char)
  {
    if (t_char == CARRIAGE_RETURN)
      return false;

    if (t_char == '\\')
      return false;

    if (t_char == '(')
      return false;

    if (t_char == ')')
      return false;

    return true;
  }


  // Test if we are still in the domain-literal parse state.
  private boolean inDomainLiteral(char t_char)
  {
    if (t_char == CARRIAGE_RETURN)
      return false;

    if (t_char == '\\')
      return false;

    if (t_char == '[')
      return false;

    if (t_char == ']')
      return false;

    return true;
  }


  // Test if we are still in the quoted-string parse state.
  private boolean inQuotedText(char t_char)
  {
    if (t_char == CARRIAGE_RETURN)
    {
      return false;
    }

    if (t_char == '\\')
    {
      return false;
    }

    if (t_char == END_QUOTE)
    {
      return false;
    }

    return true;
  }


  /*
   * Methods to skip characters until a state change
   * should occur
   */

  // Skip all characters in the outer nothing state.
  private int skipNothing()
  {
    char t_char;
    int state = AT_END;

    t_char = this.sstr.currentChar();
    while ((this.sstr.atEnd() == false)&&(inNothing(t_char) != false))
    {
      t_char = this.sstr.nextChar();
    }

    if (this.sstr.atEnd())
    {
      state = AT_END;
    }
    else if (t_char == BEGIN_COMMENT)
    {
      state = IN_COMMENT;
    }
    else if (t_char == BEGIN_DOMAIN_LITERAL)
    {
      state = IN_DOMAIN_LITERAL;
    }
    else if (t_char == BEGIN_QUOTE)
    {
      state = IN_QUOTED_TEXT;
    }
    else if (isSpecial(t_char) != false)
    {
      state = IN_SPECIAL;
    }
    else
    {
      state = IN_ATOM;
    }

    return state;
  }


  // Skip all characters in the atom state.
  private int skipAtom()
  {
    char t_char;
    int state = AT_END;

    t_char = this.sstr.currentChar();
    while ((this.sstr.atEnd() == false)&&(inAtom(t_char) != false))
    {
      t_char = this.sstr.nextChar();
    }

    if (this.sstr.atEnd())
    {
      state = AT_END;
    }
    else if (t_char == BEGIN_COMMENT)
    {
      state = IN_COMMENT;
    }
    else if (t_char == BEGIN_DOMAIN_LITERAL)
    {
      state = IN_DOMAIN_LITERAL;
    }
    else if (t_char == BEGIN_QUOTE)
    {
      state = IN_QUOTED_TEXT;
    }
    else if (Character.isSpaceChar(t_char))
    {
      state = IN_NOTHING;
    }
    else if (Character.isISOControl(t_char))
    {
      state = IN_NOTHING;
    }
    else if (isSpecial(t_char) != false)
    {
      state = IN_SPECIAL;
    }
    else
    {
      // ERROR
    }

    return state;
  }


  // Skip all characters in the comment state.
  private int skipComment()
  {
    char t_char;
    int state = AT_END;

    t_char = this.sstr.currentChar();
    while ((this.sstr.atEnd() == false)&&(inComment(t_char) != false))
    {
      t_char = this.sstr.nextChar();
    }

    if (this.sstr.atEnd())
    {
      state = AT_END;
    }
    else if (t_char == BEGIN_COMMENT)
    {
      t_char = this.sstr.nextChar();
      state = skipComment();
      t_char = this.sstr.nextChar();
      state = skipComment();
    }
    else if (t_char == END_COMMENT)
    {
      state = IN_NOTHING;
    }
    else if (t_char == BEGIN_QUOTE_PAIR)
    {
      t_char = this.sstr.nextChar();
      t_char = this.sstr.nextChar();
      state = skipComment();
    }
    else if (t_char == CARRIAGE_RETURN)
    {
      // ERROR
    }
    else
    {
      // ERROR
    }

    return state;
  }


  // Skip all characters in the domain-literal state.
  private int skipDomainLiteral()
  {
    char t_char;
    int state = AT_END;

    t_char = this.sstr.currentChar();
    while ((this.sstr.atEnd() == false)&&(inDomainLiteral(t_char) != false))
    {
      t_char = this.sstr.nextChar();
    }

    if (this.sstr.atEnd())
    {
      state = AT_END;
    }
    else if (t_char == BEGIN_DOMAIN_LITERAL)
    {
      // ERROR
    }
    else if (t_char == END_DOMAIN_LITERAL)
    {
      state = IN_NOTHING;
    }
    else if (t_char == BEGIN_QUOTE_PAIR)
    {
      t_char = this.sstr.nextChar();
      t_char = this.sstr.nextChar();
      state = skipDomainLiteral();
    }
    else if (t_char == CARRIAGE_RETURN)
    {
      // ERROR
    }
    else
    {
      // ERROR
    }

    return state;
  }


  // Skip all characters in the quoted-string state.
  private int skipQuotedText()
  {
    char t_char;
    int state = AT_END;

    t_char = this.sstr.currentChar();
    while ((this.sstr.atEnd() == false)&&(inQuotedText(t_char) != false))
    {
      t_char = this.sstr.nextChar();
    }

    if (this.sstr.atEnd())
    {
      state = AT_END;
    }
    else if (t_char == END_QUOTE)
    {
      state = IN_NOTHING;
    }
    else if (t_char == BEGIN_QUOTE_PAIR)
    {
      t_char = this.sstr.nextChar();
      t_char = this.sstr.nextChar();
      state = skipQuotedText();
    }
    else if (t_char == CARRIAGE_RETURN)
    {
      // ERROR
    }
    else
    {
      // ERROR
    }

    return state;
  }


  /*
   * Member class StringStream
   */
  private class StringStream {
    private String str;
    private int indx;
    private int length;
    private boolean atEnd;

    public StringStream(String str)
    {
      this.str = str;
      this.indx = 0;
      this.length = str.length();
      this.atEnd = false;
    }

    public int currentIndex()
    {
      if (this.atEnd)
      {
        return(this.length);
      }
      else
      {
        return(this.indx);
      }
    }

    public char currentChar()
    {
      return(this.str.charAt(this.indx));
    }

    public char nextChar()
    {
      this.indx++;
      if (this.indx >= this.length)
      {
        this.indx = this.length - 1;
        this.atEnd = true;
      }
      return(this.str.charAt(this.indx));
    }

    public boolean atEnd()
    {
      return(this.atEnd);
    }
  }
}

