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
;;; Constraint test grammar
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(progn
  (defparameter *ctw*
    (generate-world
     "T"
     '((grammar constraint-test-grammar :lr-1 :start)
       
       (production :start (:string) start-string)
       (production :start ((:- letter digit) :chars) start-escape)
       (production :start ((:- escape) :char) start-letter-digit)
       
       (production :string (begin :chars end) string)
       
       (production :chars () chars-none)
       (production :chars (:chars :char) chars-some)
       
       (production :char (letter (:- letter)) char-letter)
       (production :char (digit) char-digit)
       (production :char (escape digit (:- digit)) char-escape-1)
       (production :char (escape digit digit) char-escape-2)
       )))
  
  (defparameter *ctg* (world-grammar *ctw* 'constraint-test-grammar)))


#|
(depict-rtf-to-local-file
 "Test/ConstraintTestGrammar.rtf"
 "Constraint Test Grammar"
 #'(lambda (markup-stream)
     (depict-world-commands markup-stream *ctw* :heading-offset 1 :visible-semantics nil)))

(depict-html-to-local-file
 "Test/ConstraintTestGrammar.html"
 "Constraint Test Grammar"
 t
 #'(lambda (markup-stream)
     (depict-world-commands markup-stream *ctw* :heading-offset 1 :visible-semantics nil)))

(with-local-output (s "Test/ConstraintTestGrammar.txt") (print-grammar *ctg* s))

(pprint (parse *ctg* #'identity '(begin letter letter letter digit end)))
|#

(length (grammar-states *ctg*))
