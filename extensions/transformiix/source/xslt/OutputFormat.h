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
 * $Id: OutputFormat.h,v 1.1 2000/04/06 07:46:40 kvisco%ziplink.net Exp $
 */


#ifndef TRANSFRMX_OUTPUTFORMAT_H
#define TRANSFRMX_OUTPUTFORMAT_H

#include "String.h"
#include "baseutils.h"

/**
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.1 $ $Date: 2000/04/06 07:46:40 $
**/
class OutputFormat {

 public:

    /**
     * Creates a new OutputFormat with the default values.
    **/
    OutputFormat();


    /**
     * Deletes this OutputFormat
    **/
    virtual ~OutputFormat();


   /**
    * Returns the publicId for use when creating a DOCTYPE in the output.
    * @param dest the destination String to set equal to the value of the 
    * public identifier. 
    * @return the dest String containing the public identifier
   **/ 
   String& getDoctypePublic(String& dest);

   /**
    * Returns the systemId for use when creating a DOCTYPE in the output.
    * @param dest the destination String to set equal to the value of the 
    * system identifier. 
    * @return the dest String containing the system identifier
   **/ 
   String& getDoctypeSystem(String& dest);

   /**
    * Gets the XML output encoding that should be use when serializing
    * XML documents,and appends it to the destination String.
    * The destination String will be cleared before the encoding is
    * appended.   
    * @param dest the String to append the output encoding to
    * @return the given dest String now containing the output encoding
   **/ 
   String& getEncoding(String& dest);

   /**
    * @return whether or not indentation is allowed during serialization of
    * XML or HTML documents. If this value is not explicitly set using
    * ::setIndent, then a default indent flag is calculated based on
    * the value of the output method. By default "xml" output method will
    * return a value of MB_FALSE, while "html" output is MB_TRUE.
   **/
   MBool getIndent();

   /**
    * Gets the output method and appends it to the destination String.
    * The destination String will be cleared before the method is
    * appended.   
    * @param dest the String to append the output method to
    * @return the given dest String now containing the output method
   **/ 
   String& getMethod(String& dest);

   /**
    * Gets the XML output version that should be used when serializing
    * XML documents,and appends it to the destination String.
    * The destination String will be cleared before the version is
    * appended.   
    * @param dest the String to append the output version to
    * @return the given dest String now containing the output version
   **/ 
   String& getVersion(String& dest);

   /**
    * @return true if the output method is equal to "html".
   **/
   MBool isHTMLOutput();


   /**
    * @return true if allowing indentation  was explicitly specified.
   **/
   MBool isIndentExplicit();

   /**
    * @return true if the output method was explicitly specified.
   **/
   MBool isMethodExplicit();

   /**
    * @return true if the output method is equal to "xml".
   **/
   MBool isXMLOutput();

   /**
    * @return true if the output method is equal to "text".
   **/
   MBool isTextOutput();


   /**
    * Sets the publicId for use when creating a DOCTYPE in the output.
    * @param publicId the value of the DOCTYPE's public identifier.. 
   **/ 
   void setDoctypePublic(const String& publicId);

   /**
    * Sets the systemId for use when creating a DOCTYPE in the output.
    * @param systemId the value of the DOCTYPE's system identifier.. 
   **/ 
   void setDoctypeSystem(const String& publicId);

   /**
    * Sets the xml output encoding that should be used when serializing
    * XML documents.
    * @param encoding the value to set the XML output encoding to. 
   **/ 
   void setEncoding(const String& encoding);

   /**
    * Sets whether or not indentation is allowed during serialization
    * @param allowIndentation the flag that specifies whether or not
    * indentation is allowed during serialization
   **/
   void setIndent(MBool allowIndentation);

   /**
    * Sets the output method. Valid output method options are,
    * "xml", "html", or "text".
    * @param method the value to set the XML output method to. If
    * the given String is not a valid method, the method will be
    * set to "xml".
   **/ 
   void setMethod(const String& method);

   /**
    * Sets the xml output version that should be used when serializing
    * XML documents.
    * @param version the value to set the XML output version to. 
   **/ 
   void setVersion(const String& version);

 private:

   //-- The xml character encoding that should be used when serializing
   //-- xml documents
   String encoding;

   MBool explicitIndent;
   MBool indent;

   //-- The XSL output method, which can be "xml", "html", or "text"
   String method;

   MBool explicitMethod;

   //-- The public Id for creating a DOCTYPE
   String publicId;

   //-- The System Id for creating a DOCTYPE
   String systemId;

   //-- The xml version number that should be used when serializing
   //-- xml documents
   String version;

 

};

#endif
