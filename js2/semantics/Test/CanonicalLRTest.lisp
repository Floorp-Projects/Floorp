;;; The contents of this file are subject to the Mozilla Public
;;; License Version 1.1 (the "License"); you may not use this file
;;; except in compliance with the License. You may obtain a copy of
;;; the License at http://www.mozilla.org/MPL/
;;; 
;;; Software distributed under the License is distributed on an "AS
;;; IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
;;; implied. See the License for the specific language governing
;;; rights and limitations under the License.
;;; 
;;; The Original Code is the Language Design and Prototyping Environment.
;;; 
;;; The Initial Developer of the Original Code is Netscape Communications
;;; Corporation.  Portions created by Netscape Communications Corporation are
;;; Copyright (C) 1999 Netscape Communications Corporation.  All
;;; Rights Reserved.
;;; 
;;; Contributor(s):   Waldemar Horwat <waldemar@acm.org>

;;;
;;; Canonical LR(1) test grammar
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(progn
  (defparameter *clrtw*
    (generate-world
     "T"
     '((grammar canonical-lr-test-grammar :canonical-lr-1 :start)
       
       (production :start (:expr) start-expr)
       (production :start (:expr !) start-expr-!)
       
       (production :expr (id) expr-id)
       (production :expr (:expr + id) expr-plus)
       (production :expr (:expr - id (:- -)) expr-minus)
       (production :expr (\( :expr \)) expr-parens)
       )))
  
  (defparameter *clrtg* (world-grammar *clrtw* 'canonical-lr-test-grammar)))


#|
(depict-rtf-to-local-file
 "Test/CanonicalLRTestGrammar.rtf"
 "Canonical LR(1) Test Grammar"
 #'(lambda (markup-stream)
     (depict-world-commands markup-stream *clrtw* :heading-offset 1 :visible-semantics nil)))

(depict-html-to-local-file
 "Test/CanonicalLRTestGrammar.html"
 "Canonical LR(1) Test Grammar"
 t
 #'(lambda (markup-stream)
     (depict-world-commands markup-stream *clrtw* :heading-offset 1 :visible-semantics nil)))

(print-grammar *clrtg*)
(with-local-output (s "Test/CanonicalLRTestGrammar.txt") (print-grammar *clrtg* s))

(pprint (parse *clrtg* #'identity '(begin letter letter letter digit end)))
|#

(length (grammar-states *clrtg*))
