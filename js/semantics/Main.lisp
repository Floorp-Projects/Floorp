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
;;; ECMAScript semantic loader
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(defparameter *semantic-engine-filenames*
  '("Utilities" "Markup" "RTF" "HTML" "GrammarSymbol" "Grammar" "Parser" "Metaparser" "Lexer" "Calculus" "CalculusMarkup"
    ";JS20;Parser" ";JS20;Lexer" ";JS20;RegExp" #|"JSECMA;Lexer" "JSECMA;Parser"|# ))

(defparameter *semantic-engine-directory*
  (make-pathname 
   :directory (pathname-directory (truename *loading-file-source-file*))))

(defun load-semantic-engine ()
  (dolist (filename *semantic-engine-filenames*)
    (let ((pathname (merge-pathnames filename *semantic-engine-directory*)))
      (load pathname :verbose t))))

(defmacro with-local-output ((stream filename) &body body)
  `(with-open-file (,stream (merge-pathnames ,filename *semantic-engine-directory*)
                            :direction :output
                            :if-exists :supersede)
     ,@body))


(load-semantic-engine)
