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
;;; Custom HTML-to-RTF Converter
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(defparameter *html-to-rtf-filenames*
  '("../Utilities" "../Markup" "../RTF" "Convert"))

(defparameter *html-to-rtf-directory*
  (make-pathname
   #+lispworks :host #+lispworks (pathname-host *load-truename*)
   :directory (pathname-directory #-mcl *load-truename*
                                  #+mcl (truename *loading-file-source-file*))))

(defparameter *semantic-engine-directory*
  (merge-pathnames (make-pathname :directory '(:relative :up)) *html-to-rtf-directory*))


; Convert a filename string possibly containing slashes into a Lisp relative pathname.
(defun filename-to-relative-pathname (filename)
  (let ((directories nil))
    (loop
      (let ((slash (position #\/ filename)))
        (if slash
          (let ((dir-name (subseq filename 0 slash)))
            (push (if (equal dir-name "..") :up dir-name) directories)
            (setq filename (subseq filename (1+ slash))))
          (return (if directories
                    (make-pathname :directory (cons :relative (nreverse directories)) :name filename #+lispworks :type #+lispworks "lisp")
                    #-lispworks filename
                    #+lispworks (make-pathname :name filename :type "lisp"))))))))


; Convert a filename string possibly containing slashes relative to *html-to-rtf-directory*
; into a Lisp absolute pathname.
(defun filename-to-html-to-rtf-pathname (filename)
  (merge-pathnames (filename-to-relative-pathname filename) *html-to-rtf-directory*))


; Convert a filename string possibly containing slashes relative to *semantic-engine-directory*
; into a Lisp absolute pathname.
(defun filename-to-semantic-engine-pathname (filename)
  (merge-pathnames (filename-to-relative-pathname filename) *semantic-engine-directory*))


(defun operate-on-files (f files &rest options)
  (with-compilation-unit ()
    (dolist (filename files)
      (apply f (filename-to-html-to-rtf-pathname filename) :verbose t options))))

(defun compile-html-to-rtf ()
  (operate-on-files #'compile-file *html-to-rtf-filenames* :load t))

(defun load-html-to-rtf ()
  (operate-on-files #-allegro #'load #+allegro #'load-compiled *html-to-rtf-filenames*))


(defmacro with-local-output ((stream filename) &body body)
  `(with-open-file (,stream (filename-to-html-to-rtf-pathname ,filename)
                            :direction :output
                            :if-exists :supersede)
     ,@body))


(load (filename-to-html-to-rtf-pathname "../HTML-Parser/mac-sysdcl"))
(html-parser:initialize-parser)
(import '(html-parser:file->string
          html-parser:instance-of
          html-parser:parts
          html-parser:part-of
          html-parser:attr-values
          html-parser:html-entity-token
          html-parser:html-tag-instance))

(load-html-to-rtf)
