;;; ***** BEGIN LICENSE BLOCK *****
;;; Version: MPL 1.1/GPL 2.0/LGPL 2.1
;;;
;;; The contents of this file are subject to the Mozilla Public License Version
;;; 1.1 (the "License"); you may not use this file except in compliance with
;;; the License. You may obtain a copy of the License at
;;; http://www.mozilla.org/MPL/
;;;
;;; Software distributed under the License is distributed on an "AS IS" basis,
;;; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
;;; for the specific language governing rights and limitations under the
;;; License.
;;;
;;; The Original Code is the Language Design and Prototyping Environment.
;;;
;;; The Initial Developer of the Original Code is
;;; Netscape Communications Corporation.
;;; Portions created by the Initial Developer are Copyright (C) 1999-2002
;;; the Initial Developer. All Rights Reserved.
;;;
;;; Contributor(s):
;;;   Waldemar Horwat <waldemar@acm.org>
;;;
;;; Alternatively, the contents of this file may be used under the terms of
;;; either the GNU General Public License Version 2 or later (the "GPL"), or
;;; the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
;;; in which case the provisions of the GPL or the LGPL are applicable instead
;;; of those above. If you wish to allow use of your version of this file only
;;; under the terms of either the GPL or the LGPL, and not to allow others to
;;; use your version of this file under the terms of the MPL, indicate your
;;; decision by deleting the provisions above and replace them with the notice
;;; and other provisions required by the GPL or the LGPL. If you do not delete
;;; the provisions above, a recipient may use your version of this file under
;;; the terms of any one of the MPL, the GPL or the LGPL.
;;;
;;; ***** END LICENSE BLOCK *****

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
