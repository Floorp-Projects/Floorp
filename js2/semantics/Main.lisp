;;; The contents of this file are subject to the Netscape Public License
;;; Version 1.0 (the "NPL"); you may not use this file except in
;;; compliance with the NPL.  You may obtain a copy of the NPL at
;;; http://www.mozilla.org/NPL/
;;;
;;; Software distributed under the NPL is distributed on an "AS IS" basis,
;;; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
;;; for the specific language governing rights and limitations under the
;;; NPL.
;;;
;;; The Initial Developer of this code under the NPL is Netscape
;;; Communications Corporation.  Portions created by Netscape are
;;; Copyright (C) 1998 Netscape Communications Corporation.  All Rights
;;; Reserved.

;;;
;;; ECMAScript semantic loader
;;;
;;; Waldemar Horwat (waldemar@netscape.com)
;;;


(defparameter *semantic-engine-filenames*
  '("Utilities" "Markup" "RTF" "HTML" "GrammarSymbol" "Grammar" "Parser" "Metaparser" "Lexer" "Calculus" "CalculusMarkup" "JS20" "ECMALexer" "ECMAGrammar"))

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
