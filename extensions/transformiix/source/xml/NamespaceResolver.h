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
 * The Original Code is TransforMiiX XSLT processor.
 *
 *
 * Contributor(s):
 * Peter Van der Beken, Peter.VanderBeken@pandora.be
 *   -- original author.
 *
 * $Id: NamespaceResolver.h,v 1.6 2001/04/08 14:37:03 peterv%netscape.com Exp $
 */


#ifndef TRANSFRMX_NAMESPACE_RESOLVER_H
#define TRANSFRMX_NAMESPACE_RESOLVER_H

#include "TxString.h"

/**
 * A class that returns the relevant namespace URI for a node.
**/
class NamespaceResolver {

public:

    /**
     * Returns the namespace URI for the given name, this method should only be
     * called for returning a namespace declared within in the result document.
    **/ 
    virtual void getResultNameSpaceURI(const String& name, String& nameSpaceURI) = 0;

    /**
     * Returns the namespace URI for the given name, this method should only be
     * called for determining a namespace declared within the context (ie. the stylesheet)
    **/ 
    virtual void getNameSpaceURI(const String& name, String& nameSpaceURI) = 0;

    /**
     * Returns the namespace URI for the given namespace prefix, this method should
     * only be called for determining a namespace declared within the context
     * (ie. the stylesheet)
    **/
    virtual void getNameSpaceURIFromPrefix(const String& prefix, String& nameSpaceURI) = 0;

}; //-- NamespaceResolver

/* */
#endif


