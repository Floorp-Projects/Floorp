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

(defun bin-to-pc (input output)
  (save-excursion
    (set-buffer (let ((font-lock-mode nil))
		  (find-file-noselect output)))
    (fundamental-mode)
    (erase-buffer)
    (insert-file-contents input)
    (goto-char (point-min))
    (let ((count 0))
      (while (not (eobp))
	(let ((c1 (or (char-after (point)) 0))
	      (c2 (or (char-after (1+ (point))) 0)))
	  (insert (format "0x%02x%02x, " c2 c1))
	  (or (eobp) (delete-char 1))
	  (or (eobp) (delete-char 1))
	  (cond ((> (setq count (1+ count)) 8)
		 (setq count 0)
		 (insert "\n")))
	  )))
    (delete-char -2)
    (insert "\n")
    (save-buffer)
    ))

(defun batch-bin-to-pc ()
  (defvar command-line-args-left)	;Avoid 'free variable' warning
  (if (not noninteractive)
      (error "batch-bin-to-pc is to be used only with -batch"))
  (let ((in  (expand-file-name (nth 0 command-line-args-left)))
	(out (expand-file-name (nth 1 command-line-args-left)))
	(version-control 'never))
    (or (and in out)
	(error
	 "usage: emacs -batch -f batch-bin-to-pc input-file output-file"))
    (setq command-line-args-left (cdr (cdr command-line-args-left)))
    (let ((auto-mode-alist nil))
      (bin-to-pc in out))))
