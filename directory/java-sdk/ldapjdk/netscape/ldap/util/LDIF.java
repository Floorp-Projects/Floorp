/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap.util;

import java.util.*;
import netscape.ldap.*;
import netscape.ldap.client.*;
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
 * @see netscape.ldap.util.LDIFRecord
 */
public class LDIF {

    /**
     * Internal constants
     */
    private final static char COMMENT = '#';

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
     * @param file The name of the LDIF file that you want to parse.
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
     * @param ds The input stream providing the LDIF data.
     * @exception IOException An I/O error has occurred.
     */
    public LDIF(DataInputStream ds) throws IOException {
        BufferedReader d = new BufferedReader(new InputStreamReader(ds, "UTF8"));
        m_reader = new LineReader(d);
        m_source = ds.toString();
    }

    /**
     * Returns the next record in the LDIF data. You can call this
     * method repeatedly to iterate through all records in the LDIF data.
     * <P>
     *
     * @return The next record as an <CODE>LDIFRecord</CODE>
     * object or null if there are no more records.
     * @exception IOException An I/O error has occurred.
     * @see netscape.ldap.util.LDIFRecord
     */
    public LDIFRecord nextRecord() throws IOException {
        /* TBD: parse the version number */
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

        line = d.readLine();
        if (line == null) {
            return null;
        }

        if (!line.startsWith("dn:"))
            throw new IOException("no dn found in <" + line + ">");
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
            // if this is empty line, then we finish reading all the info for
            // the current entry
            if ((line != null) && (line.length() < 1))
                m_currEntryDone = true;
            return null;
        }

        if (line.startsWith("changetype:")) {
            /* handles (changerecord) */
            String changetype = line.substring(11).trim();
            if (changetype.equals("modify")) {
                LDIFModifyContent mc = parse_mod_spec(d);
                return mc;
            } else if (changetype.equals("add")) {
                LDIFAddContent ac = parse_add_spec(d);
                return ac;
            } else if (changetype.equals("delete")) {
                LDIFDeleteContent dc = parse_delete_spec(d);
                return dc;
            } else if (changetype.equals("moddn") ||
              changetype.equals("modrdn")) {
                LDIFModDNContent mdnc = parse_moddn_spec(d);
                return mdnc;
            } else
                throw new IOException("change type not supported");
        } else {
            /* handles 1*(attrval-spec) */
            Hashtable ht = new Hashtable();
            String newtype;
            Object val;
            LDAPAttribute newAttr = null;
            do {
                int len = line.length();
                if ( len < 1 )
                    break;
                int idx = line.indexOf(':');
                /* Must have a colon */
                if (idx == -1)
                    throw new IOException("no ':' found in <" + line + ">");
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
                          URL url = new URL(line.substring(idx+1).trim());
                          String filename = url.getFile();
                          val = getFileContent(filename);
                      } catch (MalformedURLException ex) {
                          throw new IOException("Exception: cannot construct url "+
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

                line = d.readLine();
                if (line == null || (line.length() < 1) || (line.equals("-"))) {
                    if ((line != null) && (line.length() < 1))
                        m_currEntryDone = true;
                    break;
                }
            } while ( true );
            Enumeration en = ht.elements();
            LDIFAttributeContent ac = new LDIFAttributeContent();
            while( en.hasMoreElements() ) {
                ac.addElement( (LDAPAttribute)en.nextElement() );
            }
            ht.clear();
            ht = null;

            return ac;
        }
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
        return rc;
    }

    /**
     * Parses delete content
     * @param ds data input stream
     */
    private LDIFDeleteContent parse_delete_spec(LineReader d)
          throws IOException {
        String line = d.readLine();
        if (line == null || line.equals("")) {
            LDIFDeleteContent dc = new LDIFDeleteContent();
            return dc;
        } else
            throw new IOException("invalid SEP" );
    }

    /**
     * Parses change modification.
     * @param ds data input stream
     */
    private LDIFModifyContent parse_mod_spec(LineReader d)
          throws IOException {

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
                throw new IOException("unknown modify type");

            LDIFAttributeContent ac =
                (LDIFAttributeContent)parse_ldif_content(d);
            if (ac != null) {
                LDAPAttribute attrs[] = ac.getAttributes();
                for (int i = 0; i < attrs.length; i++) {
                    LDAPModification mod = new LDAPModification(oper, attrs[i]);
                    mc.addElement( mod );
                }
            // if there is no attrval-spec, go into the else statement
            } else {
                int index = line.indexOf(":");
                if (index == -1)
                    throw new IOException("colon missing in "+line);

                String attrName = line.substring(index+1).trim();

                if (oper == LDAPModification.ADD)
                    throw new IOException("add operation needs the value for attribute "+attrName);
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

        return mc;
    }

    /**
     * Parses moddn/modrdn modification
     * @param d data input stream
     */
    private LDIFModDNContent parse_moddn_spec(LineReader d)
                  throws IOException {
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
                    throw new IOException("Incorrect input for deleteOldRdn ");
            } else if (line.startsWith("newsuperior:") &&
              (line.length() > ("newsuperior:".length()+1))) {
                mc.setNewParent(line.substring("newsuperior:".length()).trim());
            }
            line = d.readLine();
        } while (line != null && !line.equals(""));

        return mc;
    }

    /**
     * Return true if all the bytes in the given array are valid for output as a
     * String according to the LDIF specification. If not, the array should
     * output base64-encoded.
     * @return true if all the bytes in the given array are valid for output as a
     * String according to the LDIF specification; otherwise, false.
     * false.
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
     * @param pw The printer writer.
     * @param value The given string being printed out.
     * @param max The maximum characters allowed in the line.
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
     * @return Version of LDIF used in the data.
     */
    public int getVersion() {
        return m_version;
    }

    /**
     * Gets the string representation of the
     * entire LDIF file.
     * @return The string representation of the entire LDIF data.
     */
    public String toString() {
        return "LDIF {" + m_source + "}";
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

    /* Concatenate continuation lines, if present */
    class LineReader {
        LineReader( BufferedReader d ) {
            _d = d;
        }
        /**
         * Reads a non-comment line.
         * @return  A string, or null
         */
        String readLine() throws IOException {
            String line = null;
            String result = null;
            do {
                /* Leftover line from last time? */
                if ( _next != null ) {
                    line = _next;
                    _next = null;
                } else {
                    line = _d.readLine();
                }
                if (line != null) {
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
                        if ( result == null )
                            throw new IOException("continuation out of " +
                                                  "nowhere <" +
                                                  line + ">");
                        result += line.substring(1);
                    }
                } else {
                    /* End of file */
                    break;
                }
            } while ( true );
            if ( line == null )
                m_done = true;
            return result;
        }
        private BufferedReader _d;
        String _next = null;
    }
}
