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
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */

#ifndef nsMathMLParts_h___
#define nsMathMLParts_h___

#include "nscore.h"
#include "nsISupports.h"

// Factory methods for creating MathML objects
extern nsresult NS_NewMathMLmrowFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmiFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmoFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmphantomFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmpaddedFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmfencedFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmfracFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmsubFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmsupFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmsubsupFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmunderFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmoverFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmunderoverFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmmultiscriptsFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmstyleFrame ( nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmtdFrame ( nsIFrame** aNewFrame );

#endif /* nsMathMLParts_h___ */
