;;; moz-jssh.el 
;;; utilities for connecting to a live mozilla jssh server
;;;
;;; copyright (c)2003 Alex Fritze <alex@croczilla.com>

(provide 'moz-jssh)
(require 'cl)

(defvar moz-jssh-host "localhost" "*Host that JSSh server is running on")
(defvar moz-jssh-port 9997 "*Port that JSSh service is running on")

;; ----------------------------------------------------------------------
;; Functions for evaluating js code

(defun* moz-jssh-eval-internal (command process &key obj) 
  "Execute js command line 'command' for jssh process 'process'. If the
optional arg :obj is specified, the command will be executed on the
given object. Newlines in 'command' will be replaced with a space and comments will be properly escaped."
  (let ((finished nil)
        (result "")
        (unparsed "")
        (parse-reply (function (lambda (str)
                                 (cond ((string-match "^\\[\\([^\]]*\\)\\]" str)
                                        (let ((l (string-to-number 
                                                  (substring str 
                                                             (match-beginning 1)
                                                             (match-end 1))))
                                              (start (+ 1 (match-end 1))))
                                          (cond ((> (+ start l) (length str))
                                                 (setq unparsed str))
                                                (t
                                                 (setq result
                                                       (concat result
                                                               (substring str 
                                                                          start (+ start l))))
                                                 (funcall parse-reply (substring str (+ start l)))))))
                                       ((string-match "\n> $" str) 
                                        (setq finished t))
                                       (t
                                        (setq unparsed str))))))
        (null-filter (function (lambda (proc mess) nil)))
        (receive-filter (function (lambda (proc mess) 
                                    (setq mess (concat unparsed mess))
                                    (setq unparsed "")
                                    (funcall parse-reply mess)))))
    (unwind-protect
        (progn
          ;; set up the session
          (set-process-filter process null-filter)
          (process-send-string process (concat "__JSSH_protocol__ = getProtocol();"
                                               "setProtocol('synchronous');"
                                               (if obj (concat obj 
                                                               ".__JSSH_shell__ = this;"
                                                               "setContextObj(" obj ");"))
                                               "\n"))
          ;; eat prompt
          (accept-process-output process)
          ;; send the command
          ;; XXX our protocol currently requires that there is a single
          ;; newline at the end of the command only (have to be careful with 
          ;; '//'-style comments and '//' in strings):
          (let ((commandlist (split-string command "\n")))
            (setq commandlist (mapcar (lambda (s) 
                                        (replace-in-string s "^\\(\\(\\(/[^/]\\)\\|[^\"/]\\|\\(\"[^\"]*\"\\)\\)*\\)\\(//.*\\)$" "\\1 ")) 
                                      commandlist))
            (setq command (apply 'concat commandlist)))
          ;;(setq result (concat result "Command:\n" command "\n\nResult:\n"))
          (set-process-filter process receive-filter)
          (process-send-string process (concat command "\n"))
          ;; wait for output
          (while (not finished) (accept-process-output process))
          ;; now restore the session
          (set-process-filter process null-filter)
          (if obj
              (progn
                (process-send-string process "__JSSH_shell__.setContextObj(__JSSH_shell__)\n")
                ;; eat prompt
                (accept-process-output process)))
          (process-send-string process (concat "setProtocol(__JSSH_protocol__);"
                                               "delete __JSSH_protocol__;"
                                               "delete " (if obj obj "this") ".__JSSH_shell__;\n"))
          ;; eat prompt
          (accept-process-output process))
      
      ;; remove filter, so that future output goes whereever it is supposed to:
      (set-process-filter process nil))
    result))

(defun* moz-jssh-eval-anonymous (command &rest rest &key &allow-other-keys)
  "Evaluate a command in a temporary anonymous Mozilla JavaScript
shell. Optional keyed parameters will be passed to
`moz-jssh-eval-internal'."
  (interactive "sCommand: ")
  (let ((initialized nil)
        (result nil)
        (conn (open-network-stream "moz-jssh"
                                   (buffer-name)
                                   moz-jssh-host moz-jssh-port)))
    (set-process-filter conn (function (lambda (proc mess)
                                         (if (string-match "\n> $" mess)
                                             (setq initialized t)))))
    (unwind-protect
        (progn
          (while (not initialized) (accept-process-output conn))
          (setq result (apply 'moz-jssh-eval-internal command conn :allow-other-keys t rest)))
      (delete-process conn))
    (if (interactive-p)
        (if (string-match "\n" result) ; more than one line
            (with-output-to-temp-buffer "*moz-jssh-eval-anonymous*"
              (princ result)) 
          (message "%s" result))
      result)))

(defun get-region-string ()
  "Helper to return the current region as a string, or nil otherwise."
  (let ((buf (zmacs-region-buffer)))
    (if buf
        (buffer-substring (mark t buf) (point buf)))))

(defun* moz-jssh-eval (&optional &rest rest &key cmd buffer &allow-other-keys)
  "Evaluate the jssh command line given by the active region, the cmd
argument or interactively prompted for. If the optional argument
buffer is given, the command will be executed for the jssh-process
attached to that buffer. If the buffer doesn't have an attached
jssh-process, or the arg doesn't name a valid buffer, then the command
will be executed in a temporary shell.  If the argument buffer is not
given, the command will be executed for the current buffer if that
buffer has an attached jssh-process.  If it hasn't, the command will
be executed in the buffer *moz-jssh* if it exists or in a temporary
shell if there is no buffer called *moz-jssh*. Additional (keyed)
arguments can be given and will be passed to
`moz-jssh-eval-internal'."
  (interactive)
  (or cmd (setq cmd (get-region-string)))
  (if (and (interactive-p) (not cmd))
      (setq cmd (read-input "Command: ")))
  (let ((result (cond ((and (or (stringp buffer) (bufferp buffer))
                            (get-buffer-process buffer) 
                            (string-match "moz-jssh" (process-name (get-buffer-process buffer))))
                       (apply 'moz-jssh-eval-internal 
                              cmd 
                              (get-buffer-process buffer) 
                              :allow-other-keys t rest))
                      ((and (not buffer)
                            (get-buffer-process (current-buffer))
                            (string-match "moz-jssh" (process-name (get-buffer-process (current-buffer)))))
                       (apply 'moz-jssh-eval-internal
                              cmd
                              (get-buffer-process (current-buffer))
                              :allow-other-keys t rest))
                      ((and (not buffer) 
                            (get-buffer-process "*moz-jssh*")
                            (string-match "moz-jssh" (process-name (get-buffer-process "*moz-jssh*"))))
                       (apply 'moz-jssh-eval-internal 
                              cmd 
                              (get-buffer-process "*moz-jssh*")
                              :allow-other-keys t rest))
                      (t 
                       (apply 'moz-jssh-eval-anonymous 
                              cmd
                              :allow-other-keys t rest)))))
    (if (interactive-p)
        (if (string-match "\n" result) ; more than one line
            (with-output-to-temp-buffer "*moz-jssh-eval*"
              (princ result)) 
          (message "%s" result)) result)))


(defvar moz-jssh-buffer-globalobj nil "js object that a call to
`moz-jssh-eval-buffer' will be evaluted on.")

(defun* moz-jssh-eval-buffer (&optional &rest rest &allow-other-keys)
"Evaluate the current buffer in a jssh shell. If the variable
`moz-jssh-buffer-globalobj' is not null and there is a jssh shell
buffer called *moz-jssh*, then this buffer will be used as the
executing shell. Otherwise a temporary shell will be created. The
buffer content will be executed on the object given by
`moz-jssh-buffer-globalobj' or on the shell's global object if
`moz-jssh-buffer-globalobj' is null. Output will be shown in the
buffer *moz-jssh-eval*. Additional (keyed) arguments can be given and
will be passed to `moz-jssh-eval-internal'."
  (interactive)
  (let ((result (cond ((and (not moz-jssh-buffer-globalobj)
                            (get-buffer-process "*moz-jssh*")
                            (string-match "moz-jssh" (process-name (get-buffer-process "*moz-jssh*"))))
                       (apply 'moz-jssh-eval-internal
                              (buffer-string)
                              (get-buffer-process "*moz-jssh*")
                              :allow-other-keys t rest))
                      (t
                       (apply 'moz-jssh-eval-anonymous
                              (buffer-string)
                              :obj moz-jssh-buffer-globalobj
                              :allow-other-keys t rest)))))
    (if (interactive-p)
        (if (string-match "\n" result) ; more than one line
            (with-output-to-temp-buffer "*moz-jssh-eval*"
              (princ result)) 
          (message "%s" result)) result)))

  

;; ----------------------------------------------------------------------
;; some inspection functions:
   
(defun* moz-jssh-inspect (obj &rest rest &allow-other-keys)
  "Inspect the given object"
  (interactive "sObject to inspect: ")
  (let ((result (apply 'moz-jssh-eval 
                       :cmd (concat "inspect(" obj ");") 
                       :allow-other-keys t rest)))
    (if (interactive-p)
        (if (string-match "\n" result) ; multi-line result
            (with-output-to-temp-buffer "*moz-jssh-inspect*"
              (save-excursion
                (set-buffer "*moz-jssh-inspect*")
                (javascript-mode)
                (font-lock-mode))
              (princ (concat "inspect(" obj ") :\n" result)))
          (message "%s" (concat "inspect(" obj ") -> " result)))
      result)))

(defun* moz-jssh-inspect-interface (itf &rest rest &allow-other-keys)
  "Shows the IDL definition of the given interface"
  (interactive "sInterface name: ")
  (let ((result (apply 'moz-jssh-eval 
                       :cmd (concat "dumpIDL(\"" itf "\");") 
                       :allow-other-keys t rest)))
    (if (interactive-p)
        (if (string-match "\n" result) ; multi-line result
            (with-output-to-temp-buffer "*moz-jssh-inspect*"
              (save-excursion
                (set-buffer "*moz-jssh-inspect*")
                (idl-mode)
                (font-lock-mode))
              (princ result))
          (message "%s" result))
      result)))
  

;; ----------------------------------------------------------------------
;; Shell for interacting with Mozilla through a JavaScript shell
(defun moz-jssh () 
  "Connect to a running Mozilla JavaScript shell (jssh) server. "

  (interactive)
  (require 'comint)

  (unless (comint-check-proc "*moz-jssh*")
    (set-buffer
     (make-comint "moz-jssh" (cons moz-jssh-host moz-jssh-port)))
    (moz-jssh-mode))
  (pop-to-buffer "*moz-jssh*"))

;; ----------------------------------------------------------------------
;; Open a shell for current buffer.
(defun moz-jssh-buffer-shell () 
  "Connect to a running Mozilla JavaScript shell (jssh) server.  Same
as `moz-jssh', but honours the variable moz-jssh-buffer-globalobj if
defined."

  (interactive)
  (require 'comint)
  
  (let* ((procname (concat "moz-jssh" 
                            (if moz-jssh-buffer-globalobj 
                                (concat "-" (buffer-name)))))
         (buffername (concat "*" procname "*"))
         ;; need to save gobj because it is buffer local & we're going
         ;; to switch buffers:
         (gobj moz-jssh-buffer-globalobj))
    (unless (comint-check-proc buffername)
      (set-buffer
       (make-comint procname (cons moz-jssh-host moz-jssh-port)))
      (moz-jssh-mode)
      (if (not gobj)
          ()
        ;; eat greeting & prompt before sending command. XXX this is
        ;; getting a bit dodgy.time to rewrite the protocol.
        (accept-process-output (get-buffer-process buffername) 1)
        (accept-process-output (get-buffer-process buffername) 1)
        (message "%s" (moz-jssh-eval :cmd (concat "pushContext(" gobj ")")))))
    (pop-to-buffer buffername)))

;; ----------------------------------------------------------------------
;; Major mode for moz-jssh buffers

(defvar moz-jssh-mode-map nil)

(defun moz-jssh-mode-commands (map)
  (define-key map [(home)] 'comint-bol)
  (define-key map [(control c)(i)] 'moz-jssh-inspect))

(defun moz-jssh-mode ()
  "Major mode for interacting with a Mozilla JavaScript shell.
\\{moz-jssh-mode-map}
"
  (comint-mode)
  (setq comint-prompt-regexp "^> *"
        mode-name "moz-jssh"
        major-mode 'moz-jssh-mode)
  (if moz-jssh-mode-map 
      nil
    (setq moz-jssh-mode-map (copy-keymap comint-mode-map)) ; XXX could inherit instead of copying
    (moz-jssh-mode-commands moz-jssh-mode-map))
  (use-local-map moz-jssh-mode-map))

;;----------------------------------------------------------------------
;; global keybindings

(if (keymapp 'moz-prefix)
    ()
  (define-prefix-command 'moz-prefix)
  (global-set-key [(control c) m] 'moz-prefix))

(global-set-key [(control c) m j] 'moz-jssh)
(global-set-key [(control c) m s] 'moz-jssh-buffer-shell)
(global-set-key [(control c) m e] 'moz-jssh-eval-buffer)
