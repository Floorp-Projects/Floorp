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
 * Abstract class for outputting LDAP entries to a stream.
 *
 * @version 1.0
 */
public abstract class LDAPWriter implements Serializable {

//    static final long serialVersionUID = -2710382547996750924L;
    protected PrintWriter m_pw;
    private static MimeBase64Encoder m_encoder = new MimeBase64Encoder();

    /**
     * Constructs an <CODE>LDAPWriter</CODE> object to output entries
     * to a stream.
     *
     * @param pw output stream
     */
    public LDAPWriter( PrintWriter pw ) {
        m_pw = pw;
    }

    /**
     * The main method of LDAPWriter. It calls printEntryIntro,
     * printAttribute, and printEntryEnd of derived classes.
     *
     * @param entry an LDAPEntry to be formatted to the output
     * stream
     */
	public void printEntry( LDAPEntry entry ) throws IOException {
        printEntryStart( entry.getDN() );
		/* Get the attributes of the entry */
		LDAPAttributeSet attrs = entry.getAttributeSet();
		Iterator enumAttrs = attrs.iterator();
		/* Loop on attributes */
		while ( enumAttrs.hasNext() ) {
			LDAPAttribute anAttr =
				(LDAPAttribute)enumAttrs.next();
            printAttribute( anAttr );
		}
        printEntryEnd( entry.getDN() );
    }

    /**
     * Default schema writer - assumes an ordinary entry
     *
     * @param entry an LDAPEntry containing schema to be formatted
     * to the output stream
     */
	public void printSchema( LDAPEntry entry ) throws IOException {
        printEntry( entry );
    }

    /**
     * Print an attribute of an entry
     *
     * @param attr the attribute to format to the output stream
     */
    protected abstract void printAttribute( LDAPAttribute attr );

    /**
     * Print prologue to entry
     *
     * @param dn the DN of the entry
     */
    protected abstract void printEntryStart( String dn );

    /**
     * Print epilogue to entry
     *
     * @param dn the DN of the entry
     */
    protected abstract void printEntryEnd( String dn );

	protected String getPrintableValue( byte[] b ) {
		String s = "";
		ByteBuf inBuf = new ByteBuf( b, 0, b.length );
		ByteBuf encodedBuf = new ByteBuf();
		// Translate to base 64 
		m_encoder.translate( inBuf, encodedBuf );
		int nBytes = encodedBuf.length();
		if ( nBytes > 0 ) {
			s = new String(encodedBuf.toBytes(),
						   0, nBytes);
		}
		return s;
	}
}
