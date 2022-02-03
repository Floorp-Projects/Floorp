NSPR provides functions that convert double-precision floating point
numbers to and from their character string representations.

These conversion functions were originally written by David M. Gay of
AT&T. They use IEEE double-precision (not IEEE double-extended)
arithmetic.

The header file ``prdtoa.h`` declares these functions. The functions
are:

 - :ref:`PR_strtod`
 - :ref:`PR_dtoa`
 - :ref:`PR_cnvtf`

References
----------

Gay's implementation is inspired by these two papers.

[1] William D. Clinger, "How to Read Floating Point Numbers Accurately,"
Proc. ACM SIGPLAN '90, pp. 92-101.

[2] Guy L. Steele, Jr. and Jon L. White, "How to Print Floating-Point
Numbers Accurately," Proc. ACM SIGPLAN '90, pp. 112-126.
