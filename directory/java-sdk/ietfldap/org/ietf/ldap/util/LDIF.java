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
import org.ietf.ldap.client.*;
import java.io.*;
import java.net.*;

/**
 * LDAP Data Interchange Format (LDIF) is a file format used to
 * import and export directory data from an LDAP server and to
 * describe a set of changes to be applied to data in a directory.
 * This format is described in the Internet draft
 * <A HREF="ftp://ftp.ietf.org/internet-drafts/draft-good-ldap-ldif-00.txt"
 * TARGET="_blank">The LDAP Data Interchange Format (LDIF) -
 * Technical Specification</A>.
 * <P>
 *
 * This class implements an LDIF file parser.  You can construct
 * an object of this class to parse data in LDIF format and
 * manipulate the data as individual <CODE>LDIFRecord</CODE> objects.
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.util.LDIFRecord
 */
public class LDIF implements Serializable {

    /**
     * Internal constants
     */
    private final static char COMMENT = '#';
    static final long serialVersionUID = -2710382547996750924L;

    /**
     * Constructs an <CODE>LDIF</CODE> object to parse the
     * LDAP data read from stdin.
     * @exception IOException An I/O error has occurred.
     */
    public LDIF() throws IOException {
        DataInputStream ds = new DataInputStream(System.in);
        BufferedReader d = new BufferedReader(new InputStreamReader(ds, "UTF8"));
        m_reader = new LineReader(d);
        m_source = "System.in";
        m_decoder = new MimeBase64Decoder();
    }

    /**
     * Constructs an <CODE>LDIF</CODE> object to parse the
     * LDIF data read from a specified file.
     * @param file the name of the LDIF file to parse
     * @exception IOException An I/O error has occurred.
     */
    public LDIF(String file) throws IOException {
        FileInputStream fs = new FileInputStream(file);
        DataInputStream ds = new DataInputStream(fs);
        BufferedReader d = new BufferedReader(new InputStreamReader(ds, "UTF8"));
        m_reader = new LineReader(d);
        m_source = file;
        m_decoder = new MimeBase64Decoder();
    }

    /**
     * Constructs an <CODE>LDIF</CODE> object to parse the
     * LDIF data read from an input stream.
     * @param dstThe input stream providing the LDIF data
     * @exception IOException An I/O error has occurred.
     */
    public LDIF(DataInputStream ds) throws IOException {
        BufferedReader d = new BufferedReader(new InputStreamReader(ds, "UTF8"));
        m_reader = new LineReader(d);
        m_source = ds.toString();
        m_decoder = new MimeBase64Decoder();
    }

    /**
     * Returns the next record in the LDIF data. You can call this
     * method repeatedly to iterate through all records in the LDIF data.
     * <P>
     *
     * @return the next record as an <CODE>LDIFRecord</CODE>
     * object or null if there are no more records.
     * @exception IOException An I/O error has occurred.
     * @see org.ietf.ldap.util.LDIFRecord
     */
    public LDIFRecord nextRecord() throws IOException {
        if ( m_done )
            return null;
        else
            return parse_ldif_record( m_reader );
    }

    /**
     * Parses ldif content. The list of attributes is
     * terminated by \r\n or '-'. This function is
     * also used to parse the attributes in modifications.
     * @param ds data input stream
     */
    private LDIFRecord parse_ldif_record(LineReader d)
          throws IOException {
        String line = null;
        String dn = null;
        Vector attrs = new Vector();
        LDIFRecord rec = null;

        // Skip past any blank lines
        while( ((line = d.readLine()) != null) &&
               (line.length() < 1) ) {
        }
        if (line == null) {
            return null;
        }

        if (line.startsWith("version:")) {
            m_version = Integer.parseInt(
                line.substring("version:".length()).trim() );
            if ( m_version != 1 ) {
                throwLDIFException( "Unexpected " + line );
            }
            // Do the next record
            line = d.readLine();
            if ( (line != null) && (line.length() == 0) ) {
                // Skip the newline
                line = d.readLine();
            }
            if (line == null) {
                return null;
            }
        }

        if (!line.startsWith("dn:"))
            throwLDIFException("expecting dn:");
        dn = line.substring(3).trim();
        if (dn.startsWith(":") && (dn.length() > 1)) {
            String substr = dn.substring(1).trim();
            dn = new String(getDecodedBytes(substr), "UTF8");
        }

        LDIFContent content = parse_ldif_content(d);
        rec = new LDIFRecord(dn, content);
        return rec;
    }

    /**
     * Parses ldif content. The list of attributes is
     * terminated by \r\n or '-'. This function is
     * also used to parse the attributes in modifications.
     * @param ds data input stream
     */
    private LDIFContent parse_ldif_content(LineReader d)
          throws IOException {
        String line = d.readLine();
        if ((line == null) || (line.length() < 1) || (line.equals("-"))) {
            // if this is empty line, then we're finished reading all
            // the info for the current entry
            if ((line != null) && (line.length() < 1)) {
                m_currEntryDone = true;
            }
            return null;
        }

        if (line.startsWith("changetype:")) {
            /* handles (changerecord) */
            LDIFContent lc = null;
            String changetype = line.substring(11).trim();
            if (changetype.equals("modify")) {
                lc = parse_mod_spec(d);
            } else if (changetype.equals("add")) {
                lc = parse_add_spec(d);
            } else if (changetype.equals("delete")) {
                lc = parse_delete_spec(d);
            } else if (changetype.equals("moddn") ||
                       changetype.equals("modrdn")) {
                lc = parse_moddn_spec(d);
            } else {
                throwLDIFException("change type not supported");
            }
            return lc;
        }

        /* handles 1*(attrval-spec) */
        Hashtable ht = new Hashtable();
        String newtype = null;
        Object val = null;
        LDAPAttribute newAttr = null;
        Vector controlVector = null;

        /* Read lines until we're past the record */
        while( true ) {
            if (line.startsWith("control:")) {
                if ( controlVector == null ) {
                    controlVector = new Vector();
                }
                controlVector.addElement( parse_control_spec( line ) );
            } else {
                /* An attribute */
                int len = line.length();
                if ( len < 1 ) {
                    break;
                }
                int idx = line.indexOf(':');
                /* Must have a colon */
                if (idx == -1)
                    throwLDIFException("no ':' found");
                /* attribute type */
                newtype = line.substring(0,idx).toLowerCase();
                val = "";
                /* Could be :: for binary */
                idx++;
                if ( len > idx ) {
                    if ( line.charAt(idx) == ':' ) {
                        idx++;
                        String substr = line.substring(idx).trim();
                        val = getDecodedBytes(substr);
                    } else if (line.charAt(idx) == '<') {
                        try {
                            URL url =
                                new URL(line.substring(idx+1).trim());
                            String filename = url.getFile();
                            val = getFileContent(filename);
                        } catch (MalformedURLException ex) {
                            throwLDIFException(
                                ex +
                                ": cannot construct url "+
                                line.substring(idx+1).trim());
                        }
                    } else {
                        val = line.substring(idx).trim();
                    }
                }
                /* Is there a previous value for this attribute? */
                newAttr = (LDAPAttribute)ht.get( newtype );
                if ( newAttr == null ) {
                    newAttr = new LDAPAttribute( newtype );
                }
                if ( val instanceof String ) {
                    newAttr.addValue( (String)val );
                } else {
                    newAttr.addValue( (byte[])val );
                }
                ht.put( newtype, newAttr );
            }
            line = d.readLine();
            if (line == null || (line.length() < 1) ||
                (line.equals("-"))) {
                if ((line != null) && (line.length() < 1)) {
                    m_currEntryDone = true;
                }
                break;
            }
        }
        LDIFAttributeContent ac = new LDIFAttributeContent();
        // Copy over the attributes to the record
        Enumeration en = ht.elements();
        while( en.hasMoreElements() ) {
            ac.addElement( (LDAPAttribute)en.nextElement() );
        }
        ht.clear();
        if( controlVector != null ) {
            LDAPControl[] controls =
                new LDAPControl[controlVector.size()];
            controlVector.copyInto( controls );
            ac.setControls( controls );
            controlVector.removeAllElements();
        }
        return ac;
    }

    private byte[] getDecodedBytes(String line) {
        ByteBuf inBuf = new ByteBuf(line);
        ByteBuf decodedBuf = new ByteBuf();
        /* Translate from base 64 */
        m_decoder.translate( inBuf, decodedBuf );
        return decodedBuf.toBytes();
    }

    private byte[] getFileContent(String url) throws IOException {
        StringTokenizer tokenizer = new StringTokenizer(url, "|");
        String filename = url;
        int num = tokenizer.countTokens();
        if (num == 2) {
            String token = (String)tokenizer.nextElement();
            int index = token.lastIndexOf("/");
            String drive = token.substring(index+1);
            token = (String)tokenizer.nextElement();
            token = token.replace('/', '\\');
            filename = drive+":"+token;
        }

        File file = new File(filename);
        byte[] b = new byte[(int)file.length()];
        FileInputStream fi = new FileInputStream(filename);
        fi.read(b);
        return b;
    }

    /**
     * Parses add content
     * @param ds data input stream
     */
    private LDIFAddContent parse_add_spec(LineReader d)
          throws IOException {
        LDIFAttributeContent ac = (LDIFAttributeContent)parse_ldif_content(d);
        if (m_currEntryDone)
          m_currEntryDone = false;
        LDAPAttribute attrs[] = ac.getAttributes();
        LDIFAddContent rc = new LDIFAddContent(attrs);
        LDAPControl[] controls = ac.getControls();
        if ( controls != null ) {
            rc.setControls( controls );
        }
        return rc;
    }

    /**
     * Parses delete content
     * @param ds data input stream
     */
    private LDIFDeleteContent parse_delete_spec(LineReader d)
          throws IOException {
        Vector controlVector = null;
        LDIFDeleteContent dc = new LDIFDeleteContent();
        String line = d.readLine();
        while( line != null && !line.equals("") ) {
            if (line.startsWith("control:")) {
                if ( controlVector == null ) {
                    controlVector = new Vector();
                }
                controlVector.addElement( parse_control_spec( line ) );
            } else {
                throwLDIFException("invalid SEP" );
            }
            line = d.readLine();
        } 
        if( controlVector != null ) {
            LDAPControl[] controls = new LDAPControl[controlVector.size()];
            controlVector.copyInto( controls );
            dc.setControls( controls );
            controlVector.removeAllElements();
        }

        return dc;
    }

    /**
     * Parses change modification.
     * @param ds data input stream
     */
    private LDIFModifyContent parse_mod_spec(LineReader d)
          throws IOException {

        Vector controlVector = null;
        String line = null;
        line = d.readLine();
        LDIFModifyContent mc = new LDIFModifyContent();
        do {
            int oper = -1;
            if (line.startsWith("add:")) {
                oper = LDAPModification.ADD;
            } else if (line.startsWith("delete:")) {
                oper = LDAPModification.DELETE;
            } else if (line.startsWith("replace:")) {
                oper = LDAPModification.REPLACE;
            } else
                throwLDIFException("unknown modify type");

            LDIFAttributeContent ac =
                (LDIFAttributeContent)parse_ldif_content(d);
            if (ac != null) {
                LDAPAttribute attrs[] = ac.getAttributes();
                for (int i = 0; i < attrs.length; i++) {
                    LDAPModification mod = new LDAPModification(oper, attrs[i]);
                    mc.addElement( mod );
                }
                LDAPControl[] controls = ac.getControls();
                if ( controls != null ) {
                    if ( controlVector == null ) {
                        controlVector = new Vector();
                    }
                    for( int i = 0; i < controls.length; i++ ) {
                        controlVector.addElement( controls[i] );
                    }
                }
            // if there is no attrval-spec, go into the else statement
            } else {
                int index = line.indexOf(":");
                if (index == -1)
                    throwLDIFException("colon missing in "+line);

                String attrName = line.substring(index+1).trim();

                if (oper == LDAPModification.ADD)
                    throwLDIFException("add operation needs the value for attribute "+attrName);
                LDAPAttribute attr = new LDAPAttribute(attrName);
                LDAPModification mod = new LDAPModification(oper, attr);
                mc.addElement(mod);
            }
            if (m_currEntryDone) {
                m_currEntryDone = false;
                break;
            }
            line = d.readLine();
        } while (line != null && !line.equals(""));

        if( controlVector != null ) {
            LDAPControl[] controls = new LDAPControl[controlVector.size()];
            controlVector.copyInto( controls );
            mc.setControls( controls );
            controlVector.removeAllElements();
        }
        return mc;
    }

    /**
     * Parses moddn/modrdn modification.
     * @param d data input stream
     */
    private LDIFModDNContent parse_moddn_spec(LineReader d)
                  throws IOException {
        Vector controlVector = null;
        String line = null;
        line = d.readLine();
        LDIFModDNContent mc = new LDIFModDNContent();
        String val = null;
        do {
            if (line.startsWith("newrdn:") &&
              (line.length() > ("newrdn:".length()+1))) {
                mc.setRDN(line.substring("newrdn:".length()).trim());
            } else if (line.startsWith("deleteoldrdn:") &&
              (line.length() > ("deleteoldrdn:".length()+1))) {
                String str = line.substring("deleteoldrdn:".length()).trim();
                if (str.equals("0"))
                    mc.setDeleteOldRDN(false);
                else if (str.equals("1"))
                    mc.setDeleteOldRDN(true);
                else
                    throwLDIFException("Incorrect input for deleteOldRdn ");
            } else if (line.startsWith("newsuperior:") &&
                       (line.length() > ("newsuperior:".length()+1))) {
                mc.setNewParent(line.substring(
                    "newsuperior:".length()).trim());
            } else if (line.startsWith("newparent:") &&
                       (line.length() > ("newparent:".length()+1))) {
                mc.setNewParent(line.substring(
                    "newparent:".length()).trim());
            } else if (line.startsWith("control:")) {
                if ( controlVector == null ) {
                    controlVector = new Vector();
                }
                controlVector.addElement( parse_control_spec( line ) );
            }
            line = d.readLine();
        } while (line != null && !line.equals(""));

        if( controlVector != null ) {
            LDAPControl[] controls = new LDAPControl[controlVector.size()];
            controlVector.copyInto( controls );
            mc.setControls( controls );
            controlVector.removeAllElements();
        }

        return mc;
    }

    /**
     * Parses the specification of a control<BR>
     *
     * A control looks line one of the following:
     *<BR>
     * control: 1.2.3.4.10.210
     *<BR>
     * control: 1.2.3.4.10.210 true
     *<BR>
     * control: 1.2.3.4.10.210 true: someASCIIvalue
     *<BR>
     * control: 1.2.3.4.10.210: someASCIIvalue
     *<BR>
     * control: 1.2.3.4.10.210 true:: 44GK44GM44GV44KP44KJ
     *<BR>
     * control: 1.2.3.4.10.210:: 44GK44GM44GV44KP44KJ
     *<BR>
     * control: 1.2.3.4.10.210 true:< file:///usr/local/directory/cont.dta
     *<BR>
     * control: 1.2.3.4.10.210:< file:///usr/local/directory/cont.dta
     *
     * @param line a line containing a control spec
     * @return a parsed control.
     * @exception IOException if the line could not be parsed
     */
    protected LDAPControl parse_control_spec( String line )
        throws IOException {
        boolean criticality = true;
        String OID;
        byte[] val = null;
        int len = line.length();
        int idx = line.indexOf(':') + 2;
        /* OID, must be present */
        if ( idx >= len ) {
            throwLDIFException("OID required for control");
        }
        line = line.substring(idx).trim();
        idx = line.indexOf(' ');
        if ( idx < 0 ) {
            OID = line;
        } else {
            /* Optional criticality */
            OID = line.substring(0, idx);
            line = line.substring(idx+1);
            idx = line.indexOf(':');
            String criticalVal;
            if (idx > 0) {
                criticalVal = line.substring(0, idx);
            } else {
                criticalVal = line;
            }
            if ( criticalVal.compareTo("true") == 0 ) {
                criticality = true;
            } else if ( criticalVal.compareTo("false") == 0 ) {
                criticality = false;
            } else {
                throwLDIFException(
                    "Criticality for control must be true" +
                    " or false, not " + criticalVal);
            }
            /* Optional value */
            if ( idx > 0 ) {
                /* Could be :: for binary */
                idx++;
                if ( line.length() > idx ) {
                    if ( line.charAt(idx) == ':' ) {
                        idx++;
                        line = line.substring(idx).trim();
                        val = getDecodedBytes(line);
                    } else if (line.charAt(idx) == '<') {
                        String urlString = line.substring(idx+1).trim();
                        try {
                            URL url = new URL(urlString);
                            String filename = url.getFile();
                            val = getFileContent(filename);
                        } catch (MalformedURLException ex) {
                            throwLDIFException(
                                ex + ": cannot construct url "+
                                urlString);
                        }
                    } else {
                        try {
                            val = line.substring(idx).trim().getBytes("UTF8");
                        } catch(Exception x) {
                        }
                    }
                }
            }
        }
        return new LDAPControl( OID, criticality, val );
    }

    /**
     * Returns true if all the bytes in the given array are valid for output as a
     * String according to the LDIF specification. If not, the array should
     * output base64-encoded.
     * @return <code>true</code> if all the bytes in the given array are valid for 
     * output as a String according to the LDIF specification; otherwise, 
     * <code>false</code>.
     */
    public static boolean isPrintable(byte[] b) {
        for( int i = b.length - 1; i >= 0; i-- ) {
            if ( (b[i] < ' ') || (b[i] > 127) ) {
                if ( b[i] != '\t' )
                    return false;
            }
        }
        return true;
    }

    /**
     * Outputs the String in LDIF line-continuation format. No line will be longer
     * than the given max. A continuation line starts with a single blank space.
     * @param pw the printer writer
     * @param value the given string being printed out
     * @param max the maximum characters allowed in the line
     */
    public static void breakString( PrintWriter pw, String value, int max) {
        int leftToGo = value.length();
        int written = 0;
        int maxChars = max;
        /* Limit to 77 characters per line */
        while( leftToGo > 0 ) {
            int toWrite = Math.min( maxChars, leftToGo );
            String s = value.substring( written, written+toWrite);
            if ( written != 0 ) {
                pw.print( " " + s );
            } else {
                pw.print( s );
                maxChars -= 1;
            }
            written += toWrite;
            leftToGo -= toWrite;
            /* Don't use pw.println, because it outputs an extra CR
               in Win32 */
            pw.print( '\n' );
        }
    }

    /**
     * Gets the version of LDIF used in the data.
     * @return version of LDIF used in the data.
     */
    public int getVersion() {
        return m_version;
    }

    /**
     * Gets the string representation of the
     * entire LDIF file.
     * @return the string representation of the entire LDIF data file.
     */
    public String toString() {
        return "LDIF {" + m_source + "}";
    }

    /**
     * Throws a LDIF file exception including the current line number.
     * @param msg Error message
     */
    protected void throwLDIFException(String msg)throws IOException {
        throw new IOException ("line " +
            (m_currLineNum-m_continuationLength) + ": " + msg);
    }      
    
    /**
     * Internal variables
     */
    private int m_version = 1;
    private boolean m_done = false;
    private LineReader m_reader = null;
    private String m_source = null;
    private MimeBase64Decoder m_decoder = null;
    private boolean m_currEntryDone = false;
    private int m_currLineNum;
    private int m_continuationLength;

    /* Concatenate continuation lines, if present */
    class LineReader {
        LineReader( BufferedReader d ) {
            _d = d;
        }
        /**
         * Reads a non-comment line.
         * @return a string or null.
         */
        String readLine() throws IOException {
            String line = null;
            String result = null;
            int readCnt = 0, continuationLength = 0;
            do {
                /* Leftover line from last time? */
                if ( _next != null ) {
                    line = _next;
                    _next = null;
                } else {
                    line = _d.readLine();
                }
                if (line != null) {
                    readCnt++;
                    /* Empty line means end of record */
                    if( line.length() < 1 ) {
                        if ( result == null )
                            result = line;
                        else {
                            _next = line;
                            break;
                        }
                    } else if( line.charAt(0) == COMMENT ) {
                        /* Ignore comment lines */
                    } else if( line.charAt(0) != ' ' ) {
                        /* Not a continuation line */
                        if( result == null ) {
                            result = line;
                        } else {
                            _next = line;
                            break;
                        }
                    } else {
                        /* Continuation line */
                        if ( result == null ) {
                            m_currLineNum += readCnt;
                            throwLDIFException("continuation out of nowhere");
                        }
                        result += line.substring(1);
                        continuationLength++;
                    }
                } else {
                    /* End of file */
                    break;
                }
            } while ( true );

            m_done = ( line == null );
            
            m_currLineNum += readCnt;
            if (_next != null) {
                // read one line ahead
                m_currLineNum--;
            }            
            m_continuationLength = continuationLength;
            
            return result;
        }
        private BufferedReader _d;
        String _next = null;
    }

    /**
     * Converts a byte array to a printable string following
     * the LDIF rules (encode in base64 if necessary)
     *
     * @param b the byte array to convert
     * @return a converted string which is printable.
     */
    public static String toPrintableString( byte[] b ) {
        String s = "";
        if (isPrintable(b)) {
            try {
                s = new String(b, "UTF8");
            } catch ( java.io.UnsupportedEncodingException e ) {
            }
        } else {
            ByteBuf inBuf = new ByteBuf( b, 0, b.length );
            ByteBuf encodedBuf = new ByteBuf();
            // Translate to base 64 
            MimeBase64Encoder encoder = new MimeBase64Encoder();
            encoder.translate( inBuf, encodedBuf );
            int nBytes = encodedBuf.length();
            if ( nBytes > 0 ) {
                s = new String(encodedBuf.toBytes(), 0, nBytes);
            }
        }
        return s;
    }

    /**
     * Test driver - just reads and parses an LDIF file, printing
     * each record as interpreted
     *
     * @param args name of the LDIF file to parse
     */
    public static void main( String[] args ) {
        if ( args.length != 1 ) {
            System.out.println( "Usage: java LDIF <FILENAME>" );
            System.exit( 1 );
        }
        LDIF ldif = null;
        try {
            ldif = new LDIF( args[0] );
        } catch (Exception e) {
            System.err.println("Failed to read LDIF file " + args[0] +
                               ", " + e.toString());
            System.exit(1);
        }
        try {
            for( LDIFRecord rec = ldif.nextRecord();
                 rec != null; rec = ldif.nextRecord() ) {
                System.out.println( rec.toString() + '\n' );
            }
        } catch ( IOException ex ) {
            System.out.println( ex );
            System.exit( 1 );
        }
        System.exit( 0 );
    }
}
