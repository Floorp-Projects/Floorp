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
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Tom Kneeland, tomk@mitre.org
 *    -- added PRUint32 to provide a common unsigned integer
 *
 * $Id: baseutils.h,v 1.8 2001/06/26 14:07:22 peterv%netscape.com Exp $
 */

// Basic Definitions used throughout many of these classes


#ifndef TRANSFRMX_BASEUTILS_H
#define TRANSFRMX_BASEUTILS_H

#ifdef TX_EXE
  // Standalone
  typedef int PRInt32;
  typedef unsigned int PRUint32;

  typedef PRInt32 MBool;

  #define MB_TRUE  (MBool)1
  #define MB_FALSE (MBool)0

  #ifdef DEBUG
    #define NS_ASSERTION(_cond, _msg)                                \
    if(!(_cond)) {                                                   \
      cerr << "ASSERTION (" << #_cond << ") " << _msg << endl;       \
      cerr << "on line " << __LINE__ << " in " << __FILE__ << endl;  \
    }
    #else
      #define NS_ASSERTION(_cond, _msg) {}
    #endif
#else
  // Mozilla module
  #include "prtypes.h"

  typedef PRBool MBool;

  #define MB_TRUE  PR_TRUE
  #define MB_FALSE PR_FALSE
#endif

#endif

