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
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Contributor(s):
 * Peter Van der Beken, peter.vanderbeken@pandora.be
 *    -- original author.
 *
 */

#include "nsIGenericFactory.h"

#include "XSLTProcessor.h"

#define MITRE_XSL_PROCESSOR_PROGID \
"component://netscape/document-transformer?type=text/xsl"

// Factory Constructor
NS_GENERIC_FACTORY_CONSTRUCTOR(XSLTProcessor)

// Component Table
static nsModuleComponentInfo components[] = {
    { "Transformiix XSLT Processor",
      MITRE_XSL_PROCESSOR_CID,
      MITRE_XSL_PROCESSOR_PROGID,
      XSLTProcessorConstructor }
};

// NSGetModule implementation.
NS_IMPL_NSGETMODULE("XSLTProcessorModule", components)
