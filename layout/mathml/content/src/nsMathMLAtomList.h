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

/******

  This file contains the list of all MathML nsIAtoms and their values

  It is designed to be used as inline input to nsMathMLAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro MATHML_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to MATHML_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/

MATHML_ATOM(abs, "abs")
MATHML_ATOM(accent, "accent")
MATHML_ATOM(and, "and")
MATHML_ATOM(annotation, "annotation")
MATHML_ATOM(apply, "apply")
MATHML_ATOM(arccos, "arccos")
MATHML_ATOM(arcsin, "arcsin")
MATHML_ATOM(arctan, "arctan")
MATHML_ATOM(bvar, "bvar")
MATHML_ATOM(ci, "ci")
MATHML_ATOM(cn, "cn")
MATHML_ATOM(compose, "compose")
MATHML_ATOM(condition, "condition")
MATHML_ATOM(conjugate, "conjugate")
MATHML_ATOM(cos, "cos")
MATHML_ATOM(cosh, "cosh")
MATHML_ATOM(cot, "cot")
MATHML_ATOM(coth, "coth")
MATHML_ATOM(csc, "csc")
MATHML_ATOM(csch, "csch")
MATHML_ATOM(declare, "declare")
MATHML_ATOM(degree, "degree")
MATHML_ATOM(determinant, "determinant")
MATHML_ATOM(diff, "diff")
MATHML_ATOM(displaystyle, "displaystyle")
MATHML_ATOM(divide, "divide")
MATHML_ATOM(eq, "eq")
MATHML_ATOM(exists, "exists")
MATHML_ATOM(exp, "exp")
MATHML_ATOM(factorial, "factorial")
MATHML_ATOM(fence, "fence")
MATHML_ATOM(fn, "fn")
MATHML_ATOM(forall, "forall")
MATHML_ATOM(form, "form")
MATHML_ATOM(geq, "geq")
MATHML_ATOM(gt, "gt")
MATHML_ATOM(ident, "ident")
MATHML_ATOM(implies, "implies")
MATHML_ATOM(in, "in")
MATHML_ATOM(_int, "int")
MATHML_ATOM(intersect, "intersect")
MATHML_ATOM(interval, "interval")
MATHML_ATOM(inverse, "inverse")
MATHML_ATOM(lambda, "lambda")
MATHML_ATOM(largeop, "largeop")
MATHML_ATOM(leq, "leq")
MATHML_ATOM(limit, "limit")
MATHML_ATOM(linethickness, "linethickness")
MATHML_ATOM(list, "list")
MATHML_ATOM(ln, "ln")
MATHML_ATOM(log, "log")
MATHML_ATOM(logbase, "logbase")
MATHML_ATOM(lowlimit, "lowlimit")
MATHML_ATOM(lt, "lt")
MATHML_ATOM(maction, "maction")
MATHML_ATOM(maligngroup, "maligngroup")
MATHML_ATOM(malignmark, "malignmark")
MATHML_ATOM(math, "math")
MATHML_ATOM(matrix, "matrix")
MATHML_ATOM(matrixrow, "matrixrow")
MATHML_ATOM(max, "max")
MATHML_ATOM(mean, "mean")
MATHML_ATOM(median, "median")
MATHML_ATOM(merror, "merror")
MATHML_ATOM(mfenced, "mfenced")
MATHML_ATOM(mfrac, "mfrac")
MATHML_ATOM(mi, "mi")
MATHML_ATOM(min, "min")
MATHML_ATOM(minus, "minus")
MATHML_ATOM(mmultiscripts, "mmultiscripts")
MATHML_ATOM(mn, "mn")
MATHML_ATOM(mo, "mo")
MATHML_ATOM(mode, "mode")
MATHML_ATOM(moment, "moment")
MATHML_ATOM(movablelimits, "movablelimits")
MATHML_ATOM(mover, "mover")
MATHML_ATOM(mpadded, "mpadded")
MATHML_ATOM(mphantom, "mphantom")
MATHML_ATOM(mprescripts, "mprescripts")
MATHML_ATOM(mroot, "mroot")
MATHML_ATOM(mrow, "mrow")
MATHML_ATOM(ms, "ms")
MATHML_ATOM(mspace, "mspace")
MATHML_ATOM(msqrt, "msqrt")
MATHML_ATOM(mstyle, "mstyle")
MATHML_ATOM(msub, "msub")
MATHML_ATOM(msubsup, "msubsup")
MATHML_ATOM(msup, "msup")
MATHML_ATOM(mtable, "mtable")
MATHML_ATOM(mtd, "mtd")
MATHML_ATOM(mtext, "mtext")
MATHML_ATOM(mtr, "mtr")
MATHML_ATOM(munder, "munder")
MATHML_ATOM(munderover, "munderover")
MATHML_ATOM(neq, "neq")
MATHML_ATOM(none, "none")
MATHML_ATOM(not, "not")
MATHML_ATOM(notin, "notin")
MATHML_ATOM(notprsubset, "notprsubset")
MATHML_ATOM(notsubset, "notsubset")
MATHML_ATOM(or, "or")
MATHML_ATOM(partialdiff, "partialdiff")
MATHML_ATOM(plus, "plus")
MATHML_ATOM(power, "power")
MATHML_ATOM(product, "product")
MATHML_ATOM(prsubset, "prsubset")
MATHML_ATOM(quotient, "quotient")
MATHML_ATOM(reln, "reln")
MATHML_ATOM(rem, "rem")
MATHML_ATOM(root, "root")
MATHML_ATOM(scriptlevel, "scriptlevel")
MATHML_ATOM(sdev, "sdev")
MATHML_ATOM(sec, "sec")
MATHML_ATOM(sech, "sech")
MATHML_ATOM(select, "select")
MATHML_ATOM(semantics, "semantics")
MATHML_ATOM(sep, "sep")
MATHML_ATOM(separator, "separator")
MATHML_ATOM(set, "set")
MATHML_ATOM(setdiff, "setdiff")
MATHML_ATOM(sin, "sin")
MATHML_ATOM(sinh, "sinh")
MATHML_ATOM(stretchy, "stretchy")
MATHML_ATOM(subset, "subset")
MATHML_ATOM(sum, "sum")
MATHML_ATOM(tan, "tan")
MATHML_ATOM(tanh, "tanh")
MATHML_ATOM(tendsto, "tendsto")
MATHML_ATOM(times, "times")
MATHML_ATOM(transpose, "transpose")
MATHML_ATOM(_union, "union")
MATHML_ATOM(uplimit, "uplimit")
MATHML_ATOM(var, "var")
MATHML_ATOM(vector, "vector")
MATHML_ATOM(xor, "xor")
