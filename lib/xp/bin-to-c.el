;;; -*- Mode: Emacs-Lisp -*-
;;;
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

(defun bin-to-c (name input output)
  (save-excursion
    (set-buffer (let ((font-lock-mode nil))
		  (find-file-noselect output)))
    (fundamental-mode)
    (erase-buffer)
    (insert-file-contents input)
    (goto-char (point-min))
    (insert "static unsigned char " name "[] = {\n \"")
    (while (not (eobp))
      (cond ((or (< (following-char) 32)
		 (> (following-char) 126))
	     (insert ?\\
		     (+ ?0 (lsh (logand (following-char) ?\700) -6))
		     (+ ?0 (lsh (logand (following-char) ?\070) -3))
		     (+ ?0      (logand (following-char) ?\007)))
	     (delete-char 1))
	    ((or (= (following-char) ?\")
		 (= (following-char) ?\?)
		 (= (following-char) ?\\))
	     (insert ?\\)
	     (forward-char 1))
	    (t
	     (forward-char 1)))
      (if (> (current-column) 70)
	  (insert "\"\n \""))
      )
    (insert "\"\n};\n")
    (save-buffer)
    ))

(defun batch-bin-to-c ()
  (defvar command-line-args-left)	;Avoid 'free variable' warning
  (if (not noninteractive)
      (error "batch-bin-to-c is to be used only with -batch"))
  (let ((name (nth 0 command-line-args-left))
	(in  (and (nth 1 command-line-args-left)
		  (expand-file-name (nth 1 command-line-args-left))))
	(out (and (nth 2 command-line-args-left)
		  (expand-file-name (nth 2 command-line-args-left))))
	(version-control 'never))
    (or (and name in out)
	(error
	 "usage: emacs -batch -f batch-bin-to-c name input-file output-file"))
    (setq command-line-args-left (cdr (cdr command-line-args-left)))
    (let ((auto-mode-alist nil))
      (bin-to-c name in out))))
