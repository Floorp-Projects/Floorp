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

(defun html-to-pc (name input output)
  (save-excursion
    (set-buffer (let ((font-lock-mode nil))
		  (find-file-noselect output)))
    (fundamental-mode)
    (erase-buffer)
    (insert-file-contents input)
    (goto-char (point-min))
    (while (search-forward "x_cbug" nil t)
      (goto-char (match-beginning 0))
      (delete-char 1)
      (insert "win"))
    (goto-char (point-min))

    ;; refill the paragraphs for small PC screens...
    (if (equal name "LICENSE")
	(let ((fill-column 67))
	  (save-restriction
	    (goto-char (point-max))
	    (forward-paragraph -1) ; skip last one
	    (narrow-to-region (point-min) (point))
	    (goto-char (point-min))
	    (while (not (eobp))
	      (fill-paragraph nil)
	      (forward-paragraph 1)))))

    (goto-char (point-min))
    (let ((line 0)
	  (char 0))
      (while (not (eobp))
	(cond ((= char 0)
	       (if (= line 0)
		   (insert "STRINGTABLE DISCARDABLE\nBEGIN\n")
		  ;else
		   (insert "\"\n"))
	       (insert (upcase (format "IDS_%s_%x\t\"!" name line)))
	       (setq line (1+ line))))
	(cond ((= (following-char) ?\")
	       (setq char (1+ char))
	       (insert "\"")
	       (forward-char 1))
	      ((= (following-char) ?\n)
	       (delete-char 1)

	       (insert "\\r\\n")

;	       (cond ((= (following-char) ?\n)
;		      (delete-char 1)
;		      (insert "\\r\\n\\r\\n"))
;		     (t
;		      (insert " ")))
	       )
	      (t
	       (forward-char 1)))
	(setq char (1+ char))
	(if (> char 190)
	    (setq char 0))))
    (insert "\"\nEND\n")
    (forward-line -2)
    (search-forward "\"" nil t)
    (delete-char 1)
    (save-buffer)
    ))

(defun batch-html-to-pc ()
  (defvar command-line-args-left)	;Avoid 'free variable' warning
  (if (not noninteractive)
      (error "batch-html-to-pc is to be used only with -batch"))
  (let ((name (nth 0 command-line-args-left))
	(in  (expand-file-name (nth 1 command-line-args-left)))
	(out (expand-file-name (nth 2 command-line-args-left)))
	(version-control 'never))
    (or (and in out)
	(error
	 "usage: emacs -batch -f batch-html-to-pc input-file output-file"))
    (setq command-line-args-left (cdr (cdr command-line-args-left)))
    (let ((auto-mode-alist nil))
      (html-to-pc name in out))))
