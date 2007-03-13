/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

package org.mozilla.dom;

import org.w3c.dom.DOMImplementation;
import org.w3c.dom.Document;
import org.w3c.dom.DocumentType;
import org.w3c.dom.DOMException;

import org.mozilla.util.ReturnRunnable;

public class DOMImplementationImpl implements DOMImplementation {

    private long p_nsIDOMDOMImplementation = 0;

    // instantiated from JNI only
    private DOMImplementationImpl() {}

    public boolean equals(Object o) {
	if (!(o instanceof NodeListImpl))
	    return false;
	else
	    return (XPCOM_equals(o));
    }

    public int hashCode(){
	return XPCOM_hashCode();
    }

    public boolean hasFeature(String feature, String version) {
	final String finalFeature = feature;
	final  String finalVersion = version;
	Boolean bool = (Boolean)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			boolean booleanResult = 
			    nativeHasFeature(finalFeature, finalVersion);
			return booleanResult ? Boolean.TRUE : Boolean.FALSE;
		    }
		    public String toString() {
			return "DOMImplementation.hasFeature";
		    }
		});
	return bool.booleanValue(); 

    }
    native boolean nativeHasFeature(String feature, String version);

    protected void finalize() {
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeFinalize();
			return null;
		    }
		    public String toString() {
			return "DOMImplementation.finalize";
		    }
		});
    }

    protected native void nativeFinalize();

    private boolean XPCOM_equals(Object o) {
	final Object finalO = o;
	Boolean bool = (Boolean)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			boolean booleanResult = nativeXPCOM_equals(finalO);
			return booleanResult ? Boolean.TRUE : Boolean.FALSE;
		    }
		    public String toString() {
			return "DOMImplementation.XPCOM_equals";
		    }
		});
	return bool.booleanValue();

    }
    native boolean nativeXPCOM_equals(Object o);

    private int XPCOM_hashCode() {
	Integer result = (Integer)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			int intResult = nativeXPCOM_hashCode();
			return Integer.valueOf(intResult);
		    }
		    public String toString() {
			return "DOMImplementation.XPCOM_hashCode";
		    }
		});
	return result.intValue();

    }
    private native int nativeXPCOM_hashCode();

    //since DOM2
    public DocumentType createDocumentType(String qualifiedName, String publicID, String systemID) {
	final String finalQualifiedName = qualifiedName;
	final  String finalPublicID = publicID;
	final String finalSystemID = systemID;
	DocumentType result = (DocumentType)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			
	return nativeCreateDocumentType(finalQualifiedName, finalPublicID, finalSystemID);
		    }
		    public String toString() {
			return "DOMImplementation.createDocumentType";
		    }
		});
	return result; 

    }
    native DocumentType nativeCreateDocumentType(String qualifiedName, String publicID, String systemID);

    public Document createDocument(String namespaceURI,  String qualifiedName, DocumentType doctype) {
	final String finalNamespaceURI = namespaceURI;
	final   String finalQualifiedName = qualifiedName;
	final DocumentType finalDoctype = doctype;
	Document result = (Document)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateDocument(finalNamespaceURI, finalQualifiedName, finalDoctype);
		    }
		    public String toString() {
			return "DOMImplementation.createDocument";
		    }
		});
	return result;

    }
    native Document nativeCreateDocument(String namespaceURI,  String qualifiedName, DocumentType doctype);

    public Object getFeature(String feature, String version) {
        throw new UnsupportedOperationException();
    }
}
