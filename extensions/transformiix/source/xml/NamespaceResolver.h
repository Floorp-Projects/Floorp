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
 * $Id: NamespaceResolver.h,v 1.2 2000/09/04 16:25:24 axel%pike.org Exp $
 */


#include "TxString.h"

#ifndef TRANSFRMX_NAMESPACE_RESOLVER_H
#define TRANSFRMX_NAMESPACE_RESOLVER_H

/**
 * A class that returns the relevant namespace URI for a node.
**/
class NamespaceResolver {

public:

    /**
     * Returns the namespace URI for the given name
    **/ 
    virtual void getNameSpaceURI(const String& name, String& nameSpaceURI) = 0;

}; //-- NamespaceResolver

/* */
#endif


