;;; -*- Mode: Emacs-Lisp -*-
;;;
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
;;; The Original Code is the Grendel mail/news client.
;;;
;;; The Initial Developer of the Original Code is Netscape Communications
;;; Corporation.  Portions created by Netscape are
;;; Copyright (C) 1997 Netscape Communications Corporation. All
;;; Rights Reserved.
;;;
;;; Contributor(s): 
;;;
;;; Created: 25-Sep-97 by Jamie Zawinski <jwz@netscape.com>.


(defun map-folder (file function)
  (cond ((file-directory-p file)
	 (message "Listing %s..." file)
	 (let* ((default-directory (expand-file-name file))
		(files (directory-files "." nil)))
	   (while files
	     (or (string-match "\\`\\.\\|\\.summary\\'" (car files))
		 (map-folder (car files) function))
	     (setq files (cdr files)))))
	(t
	 (message "%s (loading)..." file)
	 (let ((b (find-file-noselect file)))
	   (save-excursion
	     (set-buffer b)
	     (goto-char (point-min))
	     (if (looking-at "\n*From ")
		 (funcall function)))
	   (kill-buffer b))
	 (message "%s ...done." file)))
  nil)

(defun map-messages (function)
  (goto-char (point-min))
  (let (start header-end end)
    (while (not (eobp))
      (forward-line 1)
      (setq start (point))
      (let ((case-fold-search nil))
	(if (search-forward "\nFrom " nil t)
	    (beginning-of-line)
	  (goto-char (point-max))))
      (setq end (point))
      (goto-char start)
      (if (search-forward "\n\n" nil t)
	  (forward-char -1)
	(goto-char end))
      (setq header-end (point))
      (funcall function start header-end end)
      (goto-char end)))
  nil)

(defun get-header (field &optional limit)
  (let ((case-fold-search t))
    (save-excursion
      (if (re-search-forward (concat "^" field ":[ \t]*") limit t)
	  (let ((start (point)))
	    (end-of-line)
	    (while (looking-at "\n[ \t]")
	      (forward-line 1)
	      (end-of-line))
	    (buffer-substring start (point)))))))


(require 'mail-extr)
(defun strip-address (addr)
  (let ((fn (symbol-function 'mail-extr-voodoo)))
    (unwind-protect
	(progn
	  (fset 'mail-extr-voodoo #'(lambda (x y z) nil))
	  (let ((result (mail-extract-address-components addr)))
	    (or (car result) (nth 1 result) "")))
      (fset 'mail-extr-voodoo fn))))

(defun strip-subject (subj)
  (let ((case-fold-search t))
    (while (or (string-match "\\`re:[ \t]*" subj)
	       (string-match "\\`re\\[[0-9]+\\]:[ \t]*" subj))
      (setq subj (substring subj (match-end 0)))))
  subj)

(defun parse-refs (refs)
  (let ((result nil)
	(start 0))
    (while (string-match "<[^<>]+>" refs start)
      (setq result (cons (match-string 0 refs) result)
	    start (match-end 0)))
    result))


(defun table-count (table)
  (let ((i 0.0))
    (mapatoms #'(lambda (x) (setq i (1+ i))) table)
    i))

(defun table-bytes (table)
  (let ((i 0.0))
    (mapatoms #'(lambda (x) (setq i (+ i 1 (length (symbol-name x))))) table)
    i))

(defvar total-messages)
(defvar total-size)
(defvar author-table)
(defvar recipient-table)
(defvar id-table)
(defvar refs-table)
(defvar all-ids-table)
(defvar subject-table)
(defvar simple-subject-table)
(defvar all-strings-table)
(defvar re-count)
(defvar refs-count)

(defun reset ()
  (setq total-messages 0.0
	total-size 0.0
	author-table (make-vector 511 0)
	recipient-table (make-vector 511 0)
	id-table (make-vector 511 0)
	refs-table (make-vector 511 0)
	all-ids-table (make-vector 511 0)
	all-strings-table (make-vector 511 0)
	subject-table (make-vector 511 0)
	simple-subject-table (make-vector 511 0)
	refs-count 0.0
	re-count 0.0))

(defun collect-stats ()
  (list
   'messages total-messages
   'size total-size
   'authors (table-count author-table)
   'authors-bytes (table-bytes author-table)
   'recipients (table-count recipient-table)
   'recipients-bytes (table-bytes recipient-table)
   'ids (table-count id-table)
   'ids-bytes (table-bytes id-table)
   'refs (table-count refs-table)
   'refs-bytes (table-bytes refs-table)
   'all-strings (table-count all-strings-table)
   'all-strings-bytes (table-bytes all-strings-table)
   'all-ids (table-count all-ids-table)
   'all-ids-bytes (table-bytes all-ids-table)
   'subjects (table-count subject-table)
   'subjects-bytes (table-bytes subject-table)
   'subjects2 (table-count simple-subject-table)
   'subjects2-bytes (table-bytes simple-subject-table)
   're re-count
   'refs refs-count
   ))

(defun message-stats (start header-end end)
  (goto-char start)
  (setq total-messages (1+ total-messages)
	total-size (+ total-size (- end start)))
  (if (= 0 (% (floor total-messages) 50))
      (message "%s (%d%%)..." (buffer-name)
	       (/ (* 100.0 total-size) (buffer-size))))
  (let* (
	 (author (strip-address (or (get-header "from" header-end)
				    (get-header "sender" header-end)
				    "")))
	 (recip (strip-address (or (get-header "to" header-end)
				   (get-header "cc" header-end)
				   (get-header "newsgroups" header-end)
				   "")))
	 (id (or (get-header "message-id" header-end)
		 ""))
	 (refs (parse-refs (or (get-header "references" header-end)
			       (get-header "in-reply-to" header-end)
			       "")))
	 (subj (or (get-header "subject" header-end)
		   ""))
	 (subj2 (strip-subject subj))
	 )
    (intern author author-table)
    (intern author all-strings-table)
    (intern recip recipient-table)
    (intern recip all-strings-table)
    (intern id id-table)
    (intern id all-ids-table)
    (intern id all-strings-table)
    (intern subj subject-table)
    (intern subj2 simple-subject-table)
    (intern subj2 all-strings-table)
    (if (not (equal subj subj2))
	(setq re-count (1+ re-count)))
    (setq refs-count (+ refs-count (length refs)))
    (while refs
      (intern (car refs) refs-table)
      (intern (car refs) all-ids-table)
      (intern (car refs) all-strings-table)
      (setq refs (cdr refs)))
    ))

(defun merge-stats (total s)
  (let ((r1 (cdr total))
	(r2 (cdr s)))
    (while r1
      (setcar r1 (+ (car r1) (car r2)))
      (setq r1 (cdr (cdr r1))
	    r2 (cdr (cdr r2))))))

(defvar all-stats nil)

(defun folder-stats (directory)
  (setq all-stats nil)
  (reset)
  (map-folder directory
	      #'(lambda ()
		  (message "%s..." (buffer-name))
		  (map-messages 'message-stats)
		  (let ((s (cons (buffer-name) (collect-stats))))
		    (cond (all-stats
			   (setcdr all-stats (cons s (cdr all-stats)))
			   (merge-stats (cdr (car all-stats)) (cdr s)))
			  (t (setq all-stats
				   (list (cons nil (copy-list (cdr s)))
					 s)))))
		  (reset)
		  ))
  all-stats)
