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
package netscape.ldap.util;

import java.util.*;
import netscape.ldap.*;
import java.io.*;

/**
 * Class for outputting LDAP entries to a stream as LDIF.
 *
 * @version 1.0
 */
public class LDIFWriter extends LDAPWriter {

//    static final long serialVersionUID = -2710382547996750924L;
	private String m_sep;
	private boolean m_foldLines;
	private boolean m_attrsOnly;
	private boolean m_toFiles;
	private static final String DEFAULT_SEPARATOR = ":";
	private static final int MAX_LINE = 77;


    /**
     * Constructs an <CODE>LDIFWriter</CODE> object to output entries
     * to a stream as LDIF.
     *
     * @param pw output stream
     */
    public LDIFWriter( PrintWriter pw ) {
        this( pw, false, DEFAULT_SEPARATOR, true, false );
    }

    /**
     * Constructs an <CODE>LDIFWriter</CODE> object to output entries
     * to a stream as LDIF.
     *
     * @param pw output stream
	 * @param attrsOnly <code>true</code> if only attribute names, not
	 * values, are to be printed
	 * @param separator String to use between attribute names and values;
	 * the default is ":"
	 * @param foldLines <code>true</code> to fold lines at 77 characters,
	 * <code>false</code> to not fold them; the default is <code>true</code>.
	 * @param toFiles <code>true</code> to write each attribute value to a
	 * file in the temp folder, <code>false</code> to write them to the
	 * output stream in printable format; the default is <code>false</code>.
     */
    public LDIFWriter( PrintWriter pw, boolean attrsOnly,
					   String separator, boolean foldLines,
					   boolean toFiles ) {
		super( pw );
		m_attrsOnly = attrsOnly;
		m_sep = separator;
		m_foldLines = foldLines;
		m_toFiles = toFiles;
    }

    /**
     * Print an attribute of an entry
     *
     * @param attr the attribute to format to the output stream
     */
    protected void printAttribute( LDAPAttribute attr ) {
		String attrName = attr.getName();

		if ( m_attrsOnly ) {
			printString( attrName + m_sep );
			return;
		}

		/* Loop on values for this attribute */
		Enumeration enumVals = attr.getByteValues();

		if ( enumVals != null ) {
			while (enumVals.hasMoreElements()) {
				if ( m_toFiles ) {
					try {
						FileOutputStream f = getTempFile( attrName );
						f.write( (byte[])enumVals.nextElement() );
					} catch ( Exception e ) {
						System.err.println( "Error writing values " +
								"of " + attrName + ", " +
								e.toString() );
						System.exit(1);
					}
				} else {
					byte[] b = (byte[])enumVals.nextElement();
					String s;
					if ( LDIF.isPrintable(b) ) {
						try {
							s = new String( b, "UTF8" );
						} catch ( UnsupportedEncodingException e ) {
							s = "";
						}
						printString( attrName + m_sep + " " + s );
					} else {
						s = getPrintableValue( b );
						if ( s.length() > 0 ) {
							printString( attrName + ":: " + s );
						} else {
							printString( attrName + m_sep + ' ' );
						}
					}
				}
			}
		} else {
			printString( attrName + m_sep + ' ' );
		}
	}

    /**
     * Print prologue to entry
     *
     * @param dn the DN of the entry
     */
    protected void printEntryStart( String dn ) {
		if ( dn == null ) {
			printString( "dn" + m_sep + " ");
		} else {
            byte[] b = null;
            try {
                b = dn.getBytes( "UTF8" );
            } catch ( UnsupportedEncodingException ex ) {
            }
            if ( LDIF.isPrintable(b) ) {
                printString( "dn" + m_sep + " " + dn );
            } else {
                dn = getPrintableValue( b );
                printString( "dn" + m_sep + m_sep + " " + dn );
            }
        }
	}

    /**
     * Print epilogue to entry
     *
     * @param dn the DN of the entry
     */
    protected void printEntryEnd( String dn ) {
        m_pw.println();
    }

    protected void printString( String value ) {
        if ( m_foldLines ) {
            LDIF.breakString( m_pw, value, MAX_LINE );
        } else {
            m_pw.print( value );
            m_pw.print( '\n' );
        }
    }

	/**
	 * Create a unique file name in the temp folder and open an
	 * output stream to the file
	 *
	 * @param name base name of file; an extension is appended which
	 * consists of a number that makes the name unique
	 * @return an open output stream to the file
	 * @exception IOException if the file couldn't be opened for output
	 */
    protected FileOutputStream getTempFile( String name )
               throws IOException {
	    int num = 0;
		File f;
		String filename;
		do {
		    filename = name + '.' + num;
		    f = new File( filename );
			num++;
		} while ( f.exists() );
		printString(name + m_sep + " " + filename);
		return new FileOutputStream( f );
	}
}
