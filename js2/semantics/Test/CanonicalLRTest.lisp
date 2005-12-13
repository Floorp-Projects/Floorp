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
