/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Class FilterRulesParser
 *
 * Created: David Williams <djw@netscape.com>,  1 Oct 1997.
 */
package grendel.filters;

import java.io.Reader;
import java.io.IOException;

import java.util.Vector;

import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.search.SearchTerm;

import grendel.filters.IFilter;
import grendel.filters.IFilterAction;
import grendel.filters.FilterBase;
import grendel.filters.FilterMaster;

public class FilterRulesParser extends Object {

  // Internal classes.
  public static class Token extends Object {
        public boolean isSubclassOf(Class ref_class) {
          Class my_class = this.getClass();
          while (my_class != Object.class) {
                if (my_class == ref_class)
                  return true;
                my_class = my_class.getSuperclass();
          }
          return false;
        }
        // public String toString(); - object has this
  }
  private static class EofToken extends Token {
        public String toString() {
          return "EOF";
        }
  }
  private static class DelimitToken extends Token {
        public String toString() {
          return ":";
        }
  }
  private static class EndToken extends Token {
        public String toString() {
          return ";";
        }
  }
  private static class OpenToken extends Token {
        public String toString() {
          return "(";
        }
  }
  private static class CloseToken extends Token {
        public String toString() {
          return ")";
        }
  }
  private static class CommaToken extends Token {
        public String toString() {
          return ",";
        }
  }
  private static class MatchesToken extends Token {
        public String toString() {
          return "contains";
        }
  }

  // Class slots
  public static final Token kEOF_TOKEN = new EofToken();
  public static final Token kDELIMIT_TOKEN = new DelimitToken();
  public static final Token kEND_TOKEN = new EndToken();
  public static final Token kOPEN_TOKEN = new OpenToken();
  public static final Token kCLOSE_TOKEN = new CloseToken();
  public static final Token kCOMMA_TOKEN = new CommaToken();
  public static final Token kCONTAINS_TOKEN = new MatchesToken();

  // Instance classes
  private class StringToken extends Token {
        String fString;
        StringToken(String s) {
          fString = s;
        }
        public String toString() {
          return fString;
        }
  }
  private class BadToken extends StringToken {
        BadToken(String s) {
          super(s);
        }
        BadToken(char c) {
          super(null);
          char buf[] = new char[2];
          buf[0] = c;
          buf[1] = '\0';
          fString = new String(buf);
        }
  }

  // Instance slots
  private Reader fReader;
  private int fLastc;

  // Constructors
  public FilterRulesParser(Reader reader) {

        fReader = reader;
        fLastc = -1;
  }

  // Instance methods
  private int getc() throws IOException {
        int rv;
        if (fLastc != -1) {
          rv = fLastc;
          fLastc = -1;
        } else {
          rv = fReader.read();
        }
        return rv;
  }
  private void ungetc(int c) {
        fLastc = c;
  }

  public Token getToken() throws IOException {

        int c;

        for (;;) {
          if ((c = getc()) == -1) {
                return kEOF_TOKEN;
          }

          if (Character.isWhitespace((char)c)) {
                continue; // keep going
          }

          else if (c == ':') {
                return kDELIMIT_TOKEN;
          }
          else if (c == ',') {
                return kCOMMA_TOKEN;
          }
          else if (c == ';') {
                return kEND_TOKEN;
          }
          else if (c == '(') {
                return kOPEN_TOKEN;
          }
          else if (c == ')') {
                return kCLOSE_TOKEN;
          }

          else if (c == '/') { // comment?
                c = getc();

                if (c != '/') {
                  ungetc(c);
                  return new BadToken((char)'/');
                }

                // gobble till eol
                while (c != '\n') {
                  c = getc();
                  if (c == -1)
                        return kEOF_TOKEN;
                }
                continue;
          }

          else if (c == '"') {
                StringBuffer s = new StringBuffer(64);

                // gobble till '"'
                c = getc();
                while (c != '"') {
                  if (c == -1)
                        return kEOF_TOKEN;
                  s.append((char)c);
                  c = getc();
                }

                return new StringToken(s.toString());
          }

          else { // string
                StringBuffer s = new StringBuffer(16);

                // gobble till whitespace
                while (Character.isLetterOrDigit((char)c)) {
                  s.append((char)c);
                  c = getc();
                  if (c == -1)
                        break;
                }
                ungetc(c);

                // look for string amongst known tokens
                // this needs to go into a symbol table but there is only one
                // now.
                if (s.toString().compareTo(kCONTAINS_TOKEN.toString()) == 0)
                  return kCONTAINS_TOKEN;

                return new BadToken(s.toString());
          }

        }
  }

  // A utility routine
  public String[] getArgs() throws IOException, FilterSyntaxException {
        Token token;

        if ((token = getToken()) != kOPEN_TOKEN) {
          throw new
                FilterSyntaxException("Expected '(': " + token);
        }

        if ((token = getToken()) == kCLOSE_TOKEN)
          return null;

        Vector vector = new Vector(4);

        for(;;) {

          if (!token.isSubclassOf(StringToken.class)) {
                throw new
                  FilterSyntaxException("Expected argument found: " + token);
          }

          vector.addElement(token.toString());

          if ((token = getToken()) != kCOMMA_TOKEN)
                break;
        }

        if (token != kCLOSE_TOKEN) {
          throw new
                FilterSyntaxException("Expected ')' found: " + token);
        }

        String[] rv = new String[vector.size()];
        vector.copyInto(rv);
        return rv;
  }
  public IFilter getNext() throws IOException, FilterSyntaxException {
        Token token;
        String name;
        SearchTerm   term;
        IFilterAction action;
        IFilterTermFactory term_factory;
        IFilterActionFactory action_factory;

        FilterMaster filter_master = FilterMaster.Get();

        if ((token = getToken()) == kEOF_TOKEN)
          return null;

        // expecting a string which is name of filter
        if (!token.isSubclassOf(StringToken.class)) {
          throw new
                FilterSyntaxException("Expected name of filter rule found: " + token);
        }
        name = token.toString();

        // expecting delimiter
        token = getToken();
        if (token != kDELIMIT_TOKEN) {
          throw new
                FilterSyntaxException("Expected delimiter found: " + token);
        }

        // expecting search match expression (for now just name())
        // could do this by registering names as tokens....
        token = getToken();
        if (!token.isSubclassOf(StringToken.class)) {
          throw new
                FilterSyntaxException("Expected search term found: " + token);
        }

        term_factory = filter_master.getFilterTermFactory(token.toString());

        if (term_factory == null) {
          throw new
                FilterSyntaxException("Unknown search term found: " + token);
        }

        // Call the term factory to parse the term, and create it.
        term = term_factory.Make(this);

        // expecting delimiter
        token = getToken();
        if (token != kDELIMIT_TOKEN) {
          throw new
                FilterSyntaxException("Expected delimiter found: " + token);
        }

        // expecting search match expression (for now just name())
        // could do this by registering names as tokens....
        token = getToken();
        if (!token.isSubclassOf(StringToken.class)) {
          throw new
                FilterSyntaxException("Expected filter action found: " + token);
        }

        action_factory = filter_master.getFilterActionFactory(token.toString());

        if (action_factory == null) {
          throw new
                FilterSyntaxException("Unknown filter action found: " + token);
        }

        /*
        // collect action arguments.
        String[] args;
        args = getArgs();

        action = action_factory.Make(args);
        */
        action = action_factory.Make(this);

        // expecting delimiter
        token = getToken();
        if (token != kEND_TOKEN) {
          throw new
                FilterSyntaxException("Expected ';' found: " + token);
        }

        return FilterFactory.Make(name, term, action);
  }
}
