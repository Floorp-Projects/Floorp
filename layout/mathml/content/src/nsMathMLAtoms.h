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
 *   (Following the model of the Gecko team)
 */

#ifndef nsMathMLAtoms_h___
#define nsMathMLAtoms_h___

#include "nsIAtom.h"

/**
 * This class wraps up the creation and destruction of the standard
 * set of MathML atoms used during normal MathML handling. This object
 * is created when the first MathML content object is created, and
 * destroyed when the last such content object is destroyed.
 */
class nsMathMLAtoms {
public:

  static void AddRefAtoms();
  static void ReleaseAtoms();

  /* Declare all atoms

     The atom names and values are stored in nsMathMLAtomList.h and
     are brought to you by the magic of C preprocessing

     Add new atoms to nsMathMLAtomList and all support logic will be auto-generated
     after a clobber-build within this directory
   */
#define MATHML_ATOM(_name, _value) static nsIAtom* _name;
#include "nsMathMLAtomList.h"
#undef MATHML_ATOM

};

#endif /* nsMathMLAtoms_h___ */
