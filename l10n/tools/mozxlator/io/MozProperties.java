/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/

 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MozillaTranslator (Mozilla Localization Tool)
 *
 * The Initial Developer of the Original Code is Henrik Lynggaard Hansen
 *
 * Portions created by Henrik Lynggard Hansen are
 * Copyright (C) Henrik Lynggaard Hansen.
 * All Rights Reserved.
 *
 * Contributor(s):
 * Kazu Yamamoto (progre@tky2.3web.ne.jp)
 */

package org.mozilla.translator.io;

import java.io.*;
import java.util.*;

/**
 * @author  Kazu Yamamoto
 * @version 4.2+
 */

public class MozProperties extends Properties
{
    /**
     * A property list that contains default values for any keys not
     * found in this property list.
     *
     * @serial
     */
    protected MozProperties defaults;

    public MozProperties() {
        this(null);
    }

    public MozProperties(MozProperties defaults) {
        this.defaults = defaults;
    }

    public synchronized void store(OutputStream out, String header)
    throws IOException
    {
        BufferedWriter awriter;
        awriter = new BufferedWriter(new OutputStreamWriter(out, "8859_1"));
        if (header != null)
            writeln(awriter, "#" + header);
        writeln(awriter, "#" + new Date().toString());
        for (Enumeration e = keys(); e.hasMoreElements();) {
            String key = (String)e.nextElement();
            String val = (String)get(key);
            key = saveConvert(key, true);

	    /* No need to escape embedded and trailing spaces for value, hence
	     * pass false to flag.
	     */
            val = saveConvert(val, false);
            writeln(awriter, key + "=" + val);
        }
        awriter.flush();
    }

    private static void writeln(BufferedWriter bw, String s) throws IOException {
        bw.write(s);
        bw.newLine();
    }

    /*
     * Converts unicodes to encoded &#92;uxxxx
     */
    private String saveConvert(String theString, boolean escapeSpace) {
        int len = theString.length();
        StringBuffer outBuffer = new StringBuffer(len*2);

        for(int x=0; x<len; x++) {
            char aChar = theString.charAt(x);
            if ((aChar < 0x0020) || (aChar > 0x007e)) {
                outBuffer.append('\\');
                outBuffer.append('u');
                outBuffer.append(toHex((aChar >> 12) & 0xF));
                outBuffer.append(toHex((aChar >>  8) & 0xF));
                outBuffer.append(toHex((aChar >>  4) & 0xF));
                outBuffer.append(toHex( aChar        & 0xF));
            } else {
                outBuffer.append(aChar);
            }
        }
        return outBuffer.toString();
    }

    /**
     * Convert a nibble to a hex character
     * @param	nibble	the nibble to convert.
     */
    private static char toHex(int nibble) {
        return hexDigit[(nibble & 0xF)];
    }

    /** A table of hex digits */
    private static final char[] hexDigit = {
        '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
    };
}
