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


#+allegro (shadow 'state)
#+allegro (shadow 'type)
#+lispworks (shadow 'define-action)
#+lispworks (shadow 'type)

(defparameter *semantic-engine-filenames*
  '("Utilities" "Markup" "RTF" "HTML" "GrammarSymbol" "Grammar" "Parser" "Metaparser" "Lexer" "Calculus" "CalculusMarkup"))

(defparameter *semantics-filenames*
  '("JS20/Parser" "JS20/Lexer" "JS20/Units" "JS20/RegExp" "JS20/Kernel"))

(defparameter *semantic-engine-directory*
  (make-pathname
   #+lispworks :host #+lispworks (pathname-host *load-truename*)
   :directory (pathname-directory #-mcl *load-truename*
                                  #+mcl (truename *loading-file-source-file*))))


; Convert a filename string possibly containing slashes into a Lisp relative pathname.
(defun filename-to-relative-pathname (filename)
  (let ((directories nil))
    (loop
      (let ((slash (position #\/ filename)))
        (if slash
          (progn
            (push (subseq filename 0 slash) directories)
            (setq filename (subseq filename (1+ slash))))
          (return (if directories
                    (make-pathname :directory (cons ':relative (nreverse directories)) :name filename #+lispworks :type #+lispworks "lisp")
                    #-lispworks filename
                    #+lispworks (make-pathname :name filename :type "lisp"))))))))


; Convert a filename string possibly containing slashes relative to *semantic-engine-directory*
; into a Lisp absolute pathname.
(defun filename-to-semantic-engine-pathname (filename)
  (merge-pathnames (filename-to-relative-pathname filename) *semantic-engine-directory*))


(defun operate-on-files (f files &rest options)
  (with-compilation-unit ()
    (dolist (filename files)
      (apply f (filename-to-semantic-engine-pathname filename) :verbose t options))))

(defun compile-semantic-engine ()
  (operate-on-files #'compile-file *semantic-engine-filenames* :load t))

(defun load-semantic-engine ()
  (operate-on-files #-allegro #'load #+allegro #'load-compiled *semantic-engine-filenames*))

(defun load-semantics ()
  (operate-on-files #-allegro #'load #+allegro #'load-compiled *semantics-filenames*))


(defmacro with-local-output ((stream filename) &body body)
  `(with-open-file (,stream (filename-to-semantic-engine-pathname ,filename)
                            :direction :output
                            :if-exists :supersede)
     ,@body))


(load-semantic-engine)
(load-semantics)
