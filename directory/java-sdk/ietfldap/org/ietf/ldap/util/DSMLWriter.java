/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap.util;

import java.util.*;
import org.ietf.ldap.*;
import java.io.*;

/**
 * Class for outputting LDAP entries to a stream as DSML.
 *
 * @version 1.0
 */
public class DSMLWriter extends LDAPWriter {

//    static final long serialVersionUID = -2710382547996750924L;

    /**
     * Constructs a <CODE>DSMLWriter</CODE> object to output entries
     * to a stream as DSML.
     *
     * @param pw output stream
     */
    public DSMLWriter( PrintWriter pw ) {
        super( pw );
    }

    /**
     * Prints the schema from an entry containing subschema
     *
     * entry entry containing schema definitions
     */
    public void printSchema( LDAPEntry entry ) {
        LDAPSchema schema = new LDAPSchema( entry );
        printString( "  <dsml:directory-schema>" );
        printObjectClassSchema( schema );
        printAttributeSchema( schema );
        printString( "  </dsml:directory-schema>" );
    }


    /**
     * Prints the object class schema from a schema object
     *
     * schema schema elements
     */
    protected void printObjectClassSchema( LDAPSchema schema ) {
        Enumeration en = schema.getObjectClassSchemas();
        while( en.hasMoreElements() ) {
            LDAPObjectClassSchema s = (LDAPObjectClassSchema)en.nextElement();
            printString( "    <dsml:class" );
            printString( "      id=\"" + s.getNames()[0] + "\"" );
            printString( "      oid=\"" + s.getID() + "\"" );
            String[] superiors = s.getSuperiors();
            if ( superiors != null ) {
                for( int i = 0; i < superiors.length; i++ ) {
                    printString( "      superior=\"#" + superiors[i] + "\"" );
                }
            }
            String classType = "structural";
            switch( s.getType() ) {
            case LDAPObjectClassSchema.ABSTRACT: classType = "abstract";
                break;
            case LDAPObjectClassSchema.AUXILIARY: classType = "auxiliary";
                break;
            }
            printString( "      type=\"" + classType + "\">" );
            if ( s.isObsolete() ) {
                printString( "      obsolete=true" );
            }
            printString( "      <dsml:name>" + s.getNames()[0] + "</dsml:name>" );
            printString( "      <dsml:description>" + s.getDescription() +
                         "</dsml:description>" );
            Enumeration attrs = s.getRequiredAttributes();
            while( attrs.hasMoreElements() ) {
                printString( "      <dsml:attribute ref=\"#" +
                             (String)attrs.nextElement() +
                             "\" required=\"true\"/>" );
            }
            attrs = s.getOptionalAttributes();
            while( attrs.hasMoreElements() ) {
                printString( "      <dsml:attribute ref=\"#" +
                             (String)attrs.nextElement() +
                             "\" required=\"false\"/>" );
            }
            printString( "    </dsml:class>" );
        }
    }


    /**
     * Prints the attribute schema from a schema object
     *
     * schema schema elements
     */
    protected void printAttributeSchema( LDAPSchema schema ) {
        Enumeration en = schema.getAttributeSchemas();
        while( en.hasMoreElements() ) {
            LDAPAttributeSchema s = (LDAPAttributeSchema)en.nextElement();
            printString( "    <dsml:attribute-type" );
            printString( "      id=\"" + s.getNames()[0] + "\"" );
            printString( "      oid=\"" + s.getID() + "\"" );
            String superior = s.getSuperior();
            if ( superior != null ) {
                printString( "      superior=\"#" + superior + "\"" );
            }
            if ( s.isSingleValued() ) {
                printString( "      single-value=true" );
            }
            if ( s.isObsolete() ) {
                printString( "      obsolete=true" );
            }
            if ( s.getQualifier( s.NO_USER_MODIFICATION ) != null ) {
                printString( "      user-modification=false" );
            }
            String[] vals = s.getQualifier( s.EQUALITY );
            if ( (vals != null) && (vals.length > 0) ) {
                printString( "      equality=" + vals[0] );
            }
            vals = s.getQualifier( s.ORDERING );
            if ( (vals != null) && (vals.length > 0) ) {
                printString( "      ordering=" + vals[0] );
            }
            vals = s.getQualifier( s.SUBSTR );
            if ( (vals != null) && (vals.length > 0) ) {
                printString( "      substring=" + vals[0] );
            }
            printString( "      <dsml:name>" + s.getNames()[0] + "</dsml:name>" );
            printString( "      <dsml:description>" + s.getDescription() +
                         "</dsml:description>" );
            printString( "      <dsml:syntax>" + s.getSyntaxString() +
                         "</dsml:syntax>" );
            printString( "    </dsml:attribute-type>" );
        }
    }


    /**
     * Print an attribute of an entry
     *
     * @param attr the attribute to format to the output stream
     */
    protected void printAttribute( LDAPAttribute attr ) {
		String attrName = attr.getName();

		// Object classes are treated differently in DSML. Also, they
		// are always String-valued
		if ( attrName.equalsIgnoreCase( "objectclass" ) ) {
			Enumeration enumVals = attr.getStringValues();
			if ( enumVals != null ) {
				while ( enumVals.hasMoreElements() ) {
					String s = (String)enumVals.nextElement();
					printString( "    <dsml:objectclass>" + s +
								 "</dsml:objectclass>" );
				}
			}
			return;
		}

		printString( "    <dsml:attr name=\"" + attrName + "\">" );

		/* Loop on values for this attribute */
		Enumeration enumVals = attr.getByteValues();

		if ( enumVals != null ) {
			while ( enumVals.hasMoreElements() ) {
				byte[] b = (byte[])enumVals.nextElement();
				String s;
				if ( LDIF.isPrintable(b) ) {
					try {
						s = new String( b, "UTF8" );
					} catch ( UnsupportedEncodingException e ) {
						s = "";
					}
					printEscapedValue( "      <dsml:value>", s,
                                       "</dsml:value>" );
				} else {
					s = getPrintableValue( b );
					if ( s.length() > 0 ) {
						printString( "      " +
									 "<dsml:value encoding=\"base64\">" );
						printString( "       " + s );
						printString( "      </dsml:value>" );
					}
				}
			}
		}
		printString( "    </dsml:attr>" );
	}

    /**
     * Print prologue to entry
     *
     * @param dn the DN of the entry
     */
    protected void printEntryStart( String dn ) {
		if ( dn == null ) {
			dn = "";
		}
		printString( "  <dsml:entry dn=\"" + dn + "\">" );
	}

    /**
     * Print epilogue to entry
     *
     * @param dn the DN of the entry
     */
    protected void printEntryEnd( String dn ) {
		printString( "  </dsml:entry>" );
    }

    /**
     * Print the element start, the value with escaping of special
     * characters, and the element end
     *
     * @param prolog element start
     * @param value value to be escaped
     * @param epilog element end
     */
    protected void printEscapedValue( String prolog, String value,
                                      String epilog ) {
		m_pw.print( prolog );
	    int l = value.length();
        char[] text = new char[l];
        value.getChars( 0, l, text, 0 );
	    for ( int i = 0; i < l; i++ ) {
	        char c = text[i];
	        switch (c) {
	        case '<' : 
	            m_pw.print( "&lt;" );
	            break;
	        case '&' : 
	            m_pw.print( "&amp;" );
	            break;
	        default : 
	            m_pw.print( c );
	        }
	    }
		m_pw.print( epilog);
		m_pw.print( '\n' );
    }

    protected void printString( String value ) {
		m_pw.print( value );
		m_pw.print( '\n' );
    }
}
