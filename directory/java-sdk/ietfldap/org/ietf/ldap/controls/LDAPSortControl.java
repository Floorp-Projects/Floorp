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
package org.ietf.ldap.controls;

import java.io.*;
import org.ietf.ldap.LDAPControl;
import org.ietf.ldap.LDAPSortKey;
import org.ietf.ldap.client.JDAPBERTagDecoder;
import org.ietf.ldap.LDAPException;
import org.ietf.ldap.ber.stream.*;

/**
 * Represents an LDAP v3 server control that specifies that you want
 * the server to return sorted search results.  (The OID for this
 * control is 1.2.840.113556.1.4.473.)
 * <P>
 *
 * When constructing an <CODE>LDAPSortControl</CODE> object, you can
 * specify the order in which you want the results sorted.
 * You can also specify whether or not this control is critical
 * to the search operation.
 * <P>
 *
 * To specify the sort order, you construct an <CODE>LDAPSortKey</CODE>
 * object and pass it to the <CODE>LDAPSortControl</CODE> constructor.
 * The <CODE>LDAPSortKey</CODE> object represents a list of the attribute
 * types used for sorting (a "sort key list").
 * <P>
 *
 * You can include the control in a search request by constructing
 * an <CODE>LDAPSearchConstraints</CODE> object and calling the
 * <CODE>setServerControls</CODE> method.  You can then pass this
 * <CODE>LDAPSearchConstraints</CODE> object to the <CODE>search</CODE>
 * method of an <CODE>LDAPConnection</CODE> object.
 * <P>
 *
 * For example:
 * <PRE>
 * ...
 * LDAPConnection ld = new LDAPConnection();
 * try {
 *     // Connect to server.
 *     ld.connect( 3, hostname, portnumber, "", "" );
 *
 *     // Create a sort key that specifies the sort order.
 *     LDAPSortKey sortOrder = new LDAPSortKey( attrname );
 *
 *     // Create a "critical" server control using that sort key.
 *     LDAPSortControl sortCtrl = new LDAPSortControl(sortOrder,true);
 *
 *     // Create search constraints to use that control.
 *     LDAPSearchConstraints cons = new LDAPSearchConstraints();
 *     cons.setServerControls( sortCtrl );
 *
 *     // Send the search request.
 *     LDAPSearchResults res = ld.search( "o=Airius.com",
 *        LDAPConnection.SCOPE_SUB, "(cn=Barbara*)", null, false, cons );
 *
 *     ...
 *
 * </PRE>
 *
 * The LDAP server sends back a sort response control to indicate
 * the result of the sorting operation. (The OID for this control
 * is 1.2.840.113556.1.4.474.)
 * <P>
 *
 * This control contains:
 * <P>
 *
 * <UL>
 * <LI>the result code from the sorting operation
 * <LI>optionally, the first attribute type in the sort key list
 * that resulted in an error (for example, if the attribute does
 * not exist)
 * </UL>
 * <P>
 *
 * To retrieve the data from this control, use the <CODE>getResultCode</CODE>
 * and <CODE>getFailedAttribute</CODE> methods.
 * <P>
 *
 * The following table lists what kinds of results to expect from the
 * LDAP server under different situations:
 * <P>
 *
 * <TABLE BORDER=1 COLS=4>
 * <TR VALIGN=BASELINE>
 *     <TH>Does the Server Support the Sorting Control?</TH>
 *     <TH>Is the Sorting Control Marked As Critical?</TH>
 *     <TH>Other Conditions</TH>
 *     <TH>Results from LDAP Server</TH>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD ROWSPAN=2>
 *     No
 *     </TD>
 *     <TD>
 *     Yes
 *     </TD>
 *     <TD>
 *     None
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server does not send back any entries.
 *     <LI>An LDAPException.UNAVAILABLE_CRITICAL_EXTENSION
 *     exception is thrown.
 *     </UL>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD>
 *     No
 *     </TD>
 *     <TD>
 *     None
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server ignores the sorting control and
 *     returns the entries unsorted.
 *     </UL>
 *     <P>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD ROWSPAN=4>
 *     Yes
 *     </TD>
 *     <TD>
 *     Yes
 *     </TD>
 *     <TD ROWSPAN=2>
 *     The server cannot sort the results using the specified
 *     sort key list.
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server does not send back any entries.
 *     <LI>An LDAPException.UNAVAILABLE_CRITICAL_EXTENSION
 *     exception is thrown.
 *     <LI>The server sends back the sorting response control, which
 *     specifies the result code of the sort attempt and (optionally)
 *     the attribute type that caused the error.
 *     </UL>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD>
 *     No
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server returns the entries unsorted.
 *     <LI>The server sends back the sorting response control, which
 *     specifies the result code of the sort attempt and (optionally)
 *     the attribute type that caused the error.
 *     </UL>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD ROWSPAN=2>
 *     N/A (could either be marked as critical or not)
 *     </TD>
 *     <TD>
 *     The server successfully sorted the entries.
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server sends back the sorted entries.
 *     <LI>The server sends back the sorting response control, which
 *     specifies the result code of the sort attempt
 *     (LDAPException.SUCCESS).
 *     </UL>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD>
 *     The search itself failed (for any reason).
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server sends back a result code for the search
 *     operation.
 *     <LI>The server does not send back the sorting response control.
 *     </UL>
 *     </TD>
 * </TR>
 * </TABLE>
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPSortKey
 * @see org.ietf.ldap.LDAPControl
 * @see org.ietf.ldap.LDAPConstraints
 * @see org.ietf.ldap.LDAPSearchConstraints
 * @see org.ietf.ldap.LDAPConstraints#setControls(LDAPControl)
 */
public class LDAPSortControl extends LDAPControl {
    public final static String SORTREQUEST  = "1.2.840.113556.1.4.473";
    public final static String SORTRESPONSE = "1.2.840.113556.1.4.474";

    // Response varibales
    private String _failedAttribute = null;
    private int _resultCode = 0;
    
    // Request variables
    private LDAPSortKey[] _keys;

    /**
     * Constructs a sort response <CODE>LDAPSortControl</CODE> object.
     * This constructor is used by <CODE>LDAPControl.register</CODE> to 
     * instantiate sort response controls.
     * <P>
     * To retrieve the result code of the sort operation, call 
     * <CODE>getResultCode</CODE>.
     * <P> 
     * To retrieve the attribute that caused the sort operation to fail, call
     * <CODE>getFailedAttribute</CODE>.
     * @param oid the oid for this control. This must be 
     * <CODE>LDAPSortControl.SORTRESPONSE</CODE> or an <CODE>LDAPException</CODE> 
     * is thrown.
     * @param critical <code>true</code> if this control is critical to the operation
     * @param value the value associated with this control
     * @exception org.ietf.ldap.LDAPException If oid is not 
     * <CODE>LDAPSortControl.SORTRESPONSE</CODE>.
     * @exception java.io.IOException If value contains an invalid BER sequence.
     * @see org.ietf.ldap.LDAPControl#register
     */
    public LDAPSortControl( String oid, boolean critical, byte[] value ) 
        throws LDAPException, IOException {
	super( oid, critical, value );

	if ( !oid.equals( SORTRESPONSE )) {
	    throw new LDAPException( "oid must be LDAPSortControl.SORTRESPONSE",
				     LDAPException.PARAM_ERROR);
	}

        ByteArrayInputStream inStream = new ByteArrayInputStream( value );
        BERSequence ber = new BERSequence();
        JDAPBERTagDecoder decoder = new JDAPBERTagDecoder();
        int[] nRead = { 0 };

	/* A sequence */
	BERSequence seq = (BERSequence)BERElement.getElement(decoder, inStream,
							     nRead );
	/* First is result code */
	_resultCode = ((BEREnumerated)seq.elementAt( 0 )).getValue();
	/* Then, possibly an attribute that failed sorting */
	if(seq.size() == 1) {
	    return;
	}
	/* If this is not an octet string, let there be an exception */
	BEROctetString t = (BEROctetString)seq.elementAt(1);
	try {
	    _failedAttribute = new String(t.getValue(), "UTF8");
	} catch (UnsupportedEncodingException e) {
	}

    }

    /**
     * @return the attribute that caused the sort operation to fail.
     */
    public String getFailedAttribute() {
	return _failedAttribute;
    }

    /**
     * @return the result code from the search operation.
     */
    public int getResultCode() {
	return _resultCode;
    }
    
    /**
     * Constructs an <CODE>LDAPSortControl</CODE> object with a single
     * sorting key.
     * @param key a single attribute by which to sort
     * @param critical <CODE>true</CODE> if the LDAP operation should be
     * discarded when the server does not support this control (in other
     * words, this control is critical to the LDAP operation)
     * @see org.ietf.ldap.LDAPControl
     * @see org.ietf.ldap.LDAPSortKey
     */
    public LDAPSortControl(LDAPSortKey key,
                           boolean critical) {
        super( SORTREQUEST, critical, null );
        LDAPSortKey[] keys = new LDAPSortKey[1];
        keys[0] = key;
        _value = createSortSpecification( _keys = keys );
        
    }

    /**
     * Constructs an <CODE>LDAPSortControl</CODE> object with an array of
     * sorting keys.
     * @param keys the attributes by which to sort
     * @param critical <CODE>true</CODE> if the LDAP operation should be
     * discarded when the server does not support this control (in other
     * words, this control is critical to the LDAP operation)
     * @see org.ietf.ldap.LDAPControl
     * @see org.ietf.ldap.LDAPSortKey
     */
    public LDAPSortControl(LDAPSortKey[] keys,
                           boolean critical) {
        super( SORTREQUEST, critical, null );
        _value = createSortSpecification( _keys = keys );
    }

    /**
     * Parses the sorting response control sent by the server and
     * retrieves the result code from the sorting operation and
     * the attribute that caused sorting to fail (if specified by
     * the server).
     * <P>
     *
     * You can get the controls returned by the server by using the
     * <CODE>getResponseControls</CODE> method of the
     * <CODE>LDAPConnection</CODE> class.
     * <P>
     *
     * This method returns the attribute that caused the sort operation
     * to fail (or null, if the server did not specify any attribute).
     * The result code of the sorting operation is returned in the
     * <CODE>results</CODE> argument.  This argument is an array of
     * integers; the result code is specified in the first element of
     * this array.
     * <P>
     *
     * For example:
     * <PRE>
     * ...
     * LDAPConnection ld = new LDAPConnection();
     * try {
     *     // Connect to the server, set up the sorting control,
     *     // and set up the search constraints.
     *     ...
     *
     *     // Search the directory.
     *     LDAPSearchResults res = ld.search( "o=Airius.com",
     *         LDAPConnection.SCOPE_SUB, "(cn=Barbara*)", attrs, false, cons );
     *
     *     // Determine if the server sent a control back to you.
     *     LDAPControl[] returnedControls = ld.getResponseControls();
     *     if ( returnedControls != null ) {
     *         int[] resultCodes = new int[1];
     *         String failedAttr = LDAPSortControl.parseResponse(
     *             returnedControls, resultCodes );
     *
     *         // Check if the result code indicated an error occurred.
     *         if ( resultCodes[0] != 0 ) {
     *             System.out.println( "Result code: " + resultCodes[0] );
     *             if ( failedAttr != null ) {
     *                 System.out.println( "Sorting operation failed on " +
     *                     failedAttr );
     *             } else {
     *                 System.out.println( "Server did not indicate which " +
     *                     "attribute caused sorting to fail." );
     *             }
     *         }
     *     }
     *     ...
     * }
     * ...
     *
     * </PRE>
     *
     * The following table lists some of the possible result codes
     * for the sorting operation.
     * <P>
     *
     * <TABLE BORDER=1 COLS=2>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.SUCCESS</CODE>
     *     </TD>
     *     <TD>
     *     The server successfully sorted the results.
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.OPERATION_ERROR</CODE>
     *     </TD>
     *     <TD>
     *     An internal server error occurred.
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.TIME_LIMIT_EXCEEDED</CODE>
     *     </TD>
     *     <TD>
     *     The maximum time allowed for a search was exceeded
     *     before the server finished sorting the results.
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.STRONG_AUTH_REQUIRED</CODE>
     *     </TD>
     *     <TD>
     *     The server refused to send back the sorted search
     *     results because it requires you to use a stronger
     *     authentication method.
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.ADMIN_LIMIT_EXCEEDED</CODE>
     *     </TD>
     *     <TD>
     *     There are too many entries for the server to sort.
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.NO_SUCH_ATTRIBUTE</CODE>
     *     </TD>
     *     <TD>
     *     The sort key list specifies an attribute that does not exist.
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.INAPPROPRIATE_MATCHING</CODE>
     *     </TD>
     *     <TD>
     *     The sort key list specifies a matching rule that is
     *     not recognized or appropriate
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.INSUFFICIENT_ACCESS_RIGHTS</CODE>
     *     </TD>
     *     <TD>
     *     The server did not send the sorted results because the
     *     client has insufficient access rights.
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.BUSY</CODE>
     *     </TD>
     *     <TD>
     *     The server is too busy to sort the results.
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.UNWILLING_TO_PERFORM</CODE>
     *     </TD>
     *     <TD>
     *     The server is unable to sort the results.
     *     </TD>
     * </TR>
     * <TR VALIGN=BASELINE>
     *     <TD>
     *     <CODE>LDAPException.OTHER</CODE>
     *     </TD>
     *     <TD>
     *     This general result code indicates that the server
     *     failed to sort the results for a reason other than
     *     the ones listed above.
     *     </TD>
     * </TR>
     * </TABLE>
     * <P>
     *
     * @param controls an array of <CODE>LDAPControl</CODE> objects,
     * representing the controls returned by the server
     * after a search. To get these controls, use the
     * <CODE>getResponseControls</CODE> method of the
     * <CODE>LDAPConnection</CODE> class.
     * @param results an array of integers.  The first element of this array
     * specifies the result code of the sorting operation.
     * @return the attribute that caused the error, or null if the server did
     * not specify which attribute caused the error.
     * @see org.ietf.ldap.LDAPConnection#getResponseControls
     * @deprecated LDAPSortControl response controls are now automatically 
     * instantiated.
     */
    public static String parseResponse( LDAPControl[] controls, int[] results ) {
        String attr = null;
        LDAPControl sort = null;
        /* See if there is a sort control in the array */
        for( int i = 0; (controls != null) && (i < controls.length); i++ ) {
            if ( controls[i].getID().equals( SORTRESPONSE ) ) {
                sort = controls[i];
                break;
            }
        }
        if ( sort != null ) {
            /* Suck out the data and return it */
            ByteArrayInputStream inStream =
                new ByteArrayInputStream( sort.getValue() );
            BERSequence ber = new BERSequence();
            JDAPBERTagDecoder decoder = new JDAPBERTagDecoder();
            int[] nRead = new int[1];
            nRead[0] = 0;
            try  {
                /* A sequence */
                BERSequence seq = (BERSequence)BERElement.getElement(
                                                     decoder, inStream,
                                                     nRead );
                /* First is result code */
                int result = ((BEREnumerated)seq.elementAt( 0 )).getValue();
                if ( (results != null) && (results.length > 0) )
                    results[0] = result;
                /* Then, possibly an attribute that failed sorting */
                /* If this is not an octet string, let there be an exception */
                BEROctetString t = (BEROctetString)seq.elementAt(1);
                attr = new String(t.getValue(), "UTF8");
            } catch(Exception x) {
            }
        }
        return attr;
    }

    /**
     * Create a "flattened" BER encoding of the requested sort keys,
     * and return it as a byte array.
     * @param keys the attributes by which to sort
     * @return the byte array of encoded data.
     */
    private byte[] createSortSpecification( LDAPSortKey[] keys ) {
        /* A sequence of sequences */
        BERSequence ber = new BERSequence();
        /* Add each sort key as a sequence */
        for( int i = 0; i < keys.length; i++ ) {
            BERSequence seq = new BERSequence();
            /* The attribute name */
            seq.addElement(new BEROctetString(keys[i].getKey()));
            /* Optional matching rule */
            if ( keys[i].getMatchRule() != null )
                seq.addElement(new BERTag(
                    BERTag.CONTEXT|0,
                    new BEROctetString(keys[i].getMatchRule()),
                    true));
            /* Optional reverse-order sort */
            if ( keys[i].getReverse() )
                seq.addElement(new BERTag(
                    BERTag.CONTEXT|1,
                    new BEREnumerated(LDAPSortKey.REVERSE),
                    true));
            ber.addElement( seq );
        }
        /* Suck out the data and return it */
        return flattenBER( ber );
    }
    
    public String toString() {
        return (getID() == SORTREQUEST) ? reqToString() : rspToString();
    }
    
    String reqToString() {
        
        StringBuffer sb = new StringBuffer("{SortCtrl:");
        
        sb.append(" isCritical=");
        sb.append(isCritical());
        
        sb.append(" ");
        for (int i=0; i < _keys.length; i++) {
            sb.append(_keys[i]);
        }            
        
        sb.append("}");

        return sb.toString();
    }

    String rspToString() {
        
        StringBuffer sb = new StringBuffer("{SortResponseCtrl:");
        
        sb.append(" isCritical=");
        sb.append(isCritical());
        
        if (_failedAttribute != null) {
            sb.append(" failedAttr=");
            sb.append(_failedAttribute);
        }
        
        sb.append(" resultCode=");
        sb.append(_resultCode);

        sb.append("}");

        return sb.toString();
    }
}
