/*
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
 * The Original Code is XSL:P XSLT processor.
 * 
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999 Keith Visco.
 * All Rights Reserved..
 *
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * $Id: OutputFormat.cpp,v 1.1 2000/04/06 07:46:36 kvisco%ziplink.net Exp $
 */


#include "OutputFormat.h"

/**
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.1 $ $Date: 2000/04/06 07:46:36 $
**/


/**
 * Creates a new OutputFormat with the default values.
**/
OutputFormat::OutputFormat() {
    method.append("xml");
    explicitMethod = MB_FALSE;
    explicitIndent = MB_FALSE;
    indent = MB_FALSE;
} //-- OutputFormat


/**
 * Deletes this OutputFormat
**/
OutputFormat::~OutputFormat() {};


/**
 * Returns the publicId for use when creating a DOCTYPE in the output.
 * @param dest the destination String to set equal to the value of the 
 * public identifier. 
 * @return the dest String containing the public identifier
**/ 
String& OutputFormat::getDoctypePublic(String& dest) {
   dest.clear();
   dest.append(publicId);
   return dest;
} //-- getDoctypePublic

/**
 * Returns the systemId for use when creating a DOCTYPE in the output.
 * @param dest the destination String to set equal to the value of the 
 * system identifier. 
 * @return the dest String containing the system identifier
**/ 
String& OutputFormat::getDoctypeSystem(String& dest) {
   dest.clear();
   dest.append(systemId);
   return dest;
} //-- getDoctypeSystem

/**
 * Gets the XML output encoding that should be use when serializing
 * XML documents,and appends it to the destination String.
 * The destination String will be cleared before the encoding is
 * appended.   
 * @param dest the String to append the output encoding to
 * @return the given dest String now containing the output encoding
**/ 
String& OutputFormat::getEncoding(String& dest) {
   dest.clear();
   dest.append(encoding);
   return dest;
} //-- getEncoding

/**
 * @return whether or not indentation is allowed during serialization of
 * XML or HTML documents. If this value is not explicitly set using
 * ::setIndent, then a default indent flag is calculated based on
 * the value of the output method. By default "xml" output method will
 * return a value of MB_FALSE, while "html" output is MB_TRUE.
**/
MBool OutputFormat::getIndent() {
  if (explicitIndent) return indent;  
  if (method.isEqual("html")) return MB_TRUE;
  return MB_FALSE;
} //-- getIndent

   /**
    * Gets the output method and appends it to the destination String.
    * The destination String will be cleared before the method is
    * appended.   
    * @param dest the String to append the output method to
    * @return the given dest String now containing the output method
   **/ 
String& OutputFormat::getMethod(String& dest) {
   dest.clear();
   dest.append(method);
   return dest;
} //-- getMethod

/**
 * Gets the XML output version that should be used when serializing
 * XML documents,and appends it to the destination String.
 * The destination String will be cleared before the version is
 * appended.   
 * @param dest the String to append the output version to
 * @return the given dest String now containing the output version
**/ 
String& OutputFormat::getVersion(String& dest) {
   dest.clear();
   dest.append(version);
   return dest;
}

/**
 * @return true if the output method is equal to "html".
**/
MBool OutputFormat::isHTMLOutput() {
    return (MBool) method.isEqual("html");
} //-- isHTMLOutput


/**
 * @return true if allowing indentation  was explicitly specified.
**/
MBool OutputFormat::isIndentExplicit() {
    return explicitIndent;
} //-- isIndentExplicit


/**
 * @return true if the output method was explicitly specified.
**/
MBool OutputFormat::isMethodExplicit() {
    return explicitMethod;
} //-- isMethodExplicit

/**
 * @return true if the output method is equal to "text".
**/
MBool OutputFormat::isTextOutput() {
    return (MBool) method.isEqual("text");
} //-- isTextOuput

/**
 * @return true if the output method is equal to "xml".
**/
MBool OutputFormat::isXMLOutput() {
    return (MBool) method.isEqual("xml");
} //-- isXMLOutput



/**
 * Sets the publicId for use when creating a DOCTYPE in the output.
 * @param publicId the value of the DOCTYPE's public identifier.. 
**/ 
void OutputFormat::setDoctypePublic(const String& publicId) {
    this->publicId = publicId;
} //-- setDoctypePublic

/**
 * Sets the systemId for use when creating a DOCTYPE in the output.
 * @param systemId the value of the DOCTYPE's system identifier.. 
**/ 
void OutputFormat::setDoctypeSystem(const String& systemId) {
    this->systemId = systemId;
} //-- setDoctypeSystem

/**
 * Sets the xml output encoding that should be used when serializing
 * XML documents.
 * @param encoding the value to set the XML output encoding to. 
**/ 
void OutputFormat::setEncoding(const String& encoding) {
    this->encoding = encoding;
} //-- setEncoding

/**
 * Sets whether or not indentation is allowed during serialization
 * @param allowIndentation the flag that specifies whether or not
 * indentation is allowed during serialization
**/
void OutputFormat::setIndent(MBool allowIndentation) {
    explicitIndent = MB_TRUE;
    indent = allowIndentation;
} //-- setIndent

/**
 * Sets the output method. Valid output method options are,
 * "xml", "html", or "text".
 * @param method the value to set the XML output method to. If
 * the given String is not a valid method, the method will be
 * set to "xml".
**/ 
void OutputFormat::setMethod(const String& method) {

    explicitMethod = MB_TRUE;
    if (method.isEqual("html")) this->method = method;
    else if (method.isEqual("text")) this->method = method;
    else this->method = "xml";

} //-- setMethod
 
/**
 * Sets the xml output version that should be used when serializing
 * XML documents.
 * @param version the value to set the XML output version to. 
**/ 
void OutputFormat::setVersion(const String& version) {
    this->version = version;
} //-- setVersion

