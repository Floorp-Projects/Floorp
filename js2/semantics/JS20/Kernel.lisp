
(defun js-state-transition (action-results)
  (assert-type action-results (tuple t bool))
  (values action-results (if (second action-results) '($re) '($non-re))))

(defun js-metaparse (string &key trace)
  (lexer-metaparse *ll* string :initial-state '($re) :state-transition #'js-state-transition :trace trace))

(defun js-pmetaparse (string &key (stream t) trace)
  (lexer-pmetaparse *ll* string :initial-state '($re) :state-transition #'js-state-transition :stream stream :trace trace))


; Convert the results of the lexer's actions into a token suitable for the parser.
(defun js-lexer-results-to-token (token-value line-break)
  (if (eq token-value :end-of-input)
    (values *end-marker* nil)
    (let ((data (second token-value)))
      (multiple-value-bind (token token-arg)
                           (ecase (first token-value)
                             (l:identifier (values '$identifier data))
                             ((l:keyword l:punctuator) (values (intern (string-upcase data)) nil))
                             (l:number (values '$number data))
                             (l:string (values '$string data))
                             (l:regular-expression (values '$regular-expression data)))
        (when line-break
          (setq token (terminal-lf-terminal token)))
        (values token token-arg)))))


; Lex and parse the input-string of tokens to produce a list of action results.
; If trace is:
;   nil,         don't print trace information
;   :code,       print trace information, including action code
;   :lexer,      print lexer trace information
;   :lexer-code  print lexer trace information, including action code
;   other        print trace information
; Return three values:
;   the list of action results;
;   the list of action results' types;
;   the list of processed tokens.
(defun js-parse (input-string &key (lexer *ll*) (grammar *jg*) trace)
  (let ((lexer-classifier (lexer-classifier lexer))
        (lexer-metagrammar (lexer-metagrammar lexer))
        (lexer-trace (cdr (assoc trace '((:lexer t) (:lexer-code :code)))))
        (state-stack (list (grammar-start-state grammar)))
        (value-stack nil)
        (type-stack nil)
        (prev-number-token nil)
        (input (append (coerce input-string 'list) '($end)))
        (token nil)
        (token-arg nil)
        (token2 nil)
        (token2-arg nil)
        (token-history nil))
    (flet
      ((get-next-token-value (lexer-state)
         (multiple-value-bind (results in-rest)
                              (action-metaparse lexer-metagrammar lexer-classifier (cons lexer-state input) :trace lexer-trace)
           (assert-true (null (cdr results)))
           (setq input in-rest)
           (car results))))
      
      (loop
        (let* ((state (car state-stack))
               (transition (state-only-transition state)))
          (unless transition
            (unless token
              (if token2
                (setq token token2
                      token-arg token2-arg
                      token2 nil
                      token2-arg nil)
                (let* ((lexer-state (cond
                                     (prev-number-token '$unit)
                                     ((or (state-transition state '/) (state-transition state '/=)) '$non-re)
                                     (t '$re)))
                       (token-value (get-next-token-value lexer-state))
                       (line-break nil))
                  (when (eq token-value :line-break)
                    (when (eq lexer-state '$unit)
                      (setq lexer-state '$non-re))
                    (setq token-value (get-next-token-value lexer-state))
                    (setq line-break t))
                  (setq prev-number-token (and (consp token-value) (eq (car token-value) 'l:number)))
                  (multiple-value-setq (token token-arg) (js-lexer-results-to-token token-value line-break)))))
            (setq transition (state-transition state token))
            (unless transition
              (when (lf-terminal? token)
                (setq transition (state-transition state '$virtual-semicolon)))
              (if transition
                (progn
                  (when trace
                    (format *trace-output* "Inserted virtual semicolon~@:_"))
                  (setq token2 token
                        token2-arg token-arg
                        token '$virtual-semicolon
                        token-arg nil))
                (syntax-error "Parse error on ~S followed by ~S ..." token (coerce (butlast (ldiff input (nthcdr 31 input))) 'string)))))
          
          (when trace
            (format *trace-output* "S~D: ~@_" (state-number state))
            (print-values (reverse value-stack) (reverse type-stack) *trace-output*)
            (pprint-newline :mandatory *trace-output*))
          
          (ecase (transition-kind transition)
            (:shift
             (push (if token-arg (cons token token-arg) token) token-history)
             (when trace
               (format *trace-output* "  shift ~W ~W~:@_" token token-arg)
               (dolist (action-signature (grammar-symbol-signature grammar token))
                 (push (cdr action-signature) type-stack)))
             (dolist (action-function-binding (gethash token (grammar-terminal-actions grammar)))
               (push (funcall (cdr action-function-binding) token-arg) value-stack))
             (push (transition-state transition) state-stack)
             (setq token nil))
            
            (:reduce
             (let ((production (transition-production transition)))
               (when trace
                 (write-string "  reduce " *trace-output*)
                 (if (eq trace :code)
                   (write production :stream *trace-output* :pretty t)
                   (print-production production *trace-output*))
                 (pprint-newline :mandatory *trace-output*))
               (setq state-stack (nthcdr (production-rhs-length production) state-stack)
                     state (assert-non-null
                            (cdr (assoc (production-lhs production) (state-gotos (car state-stack)) :test *grammar-symbol-=*)))
                     value-stack (funcall (production-evaluator production) value-stack))
               (push state state-stack)
               (when trace
                 (setq type-stack (nthcdr (production-n-action-args production) type-stack))
                 (dolist (action-signature (grammar-symbol-signature grammar (production-lhs production)))
                   (push (cdr action-signature) type-stack)))))
            
            (:accept
             (when trace
               (format *trace-output* "  accept~:@_"))
             (return (values
                      (nreverse value-stack)
                      (if trace
                        (nreverse type-stack)
                        (grammar-user-start-action-types grammar))
                      (nreverse token-history)))))
          (when trace
            (format *trace-output* "!")))))))


; Simple JS2 read-eval-print loop.
(defun rep ()
  (loop
    (let ((s (read-line *terminal-io* t)))
      (format *terminal-io* "<~S>~%" s)
      (block success
        (handler-case
          (let ((exception
                 (catch :semantic-exception
                   (dolist (r (multiple-value-list (js-parse s)))
                     (write r :stream *terminal-io* :pretty t)
                     (terpri *terminal-io*))
                   (return-from success))))
            (format *terminal-io* "Exception: ~S~%" exception))
          (syntax-error (condition)
                        (format *terminal-io* "~A~%" condition)))))))


#|
(js-parse "1+2*/4*/
32")
(js-parse "1+2
+3" :trace t)
(js-parse "3
++" :trace t)
(js-parse "1+2
true
false")
(js-parse "32+abc//23e-a4*7e-2 3 id4 4ef;")

(js-parse "if (1) 5 else 8")
(js-parse "0x20")
(js-parse "2b")
(js-parse " 3.75" :trace t)
(js-parse "25" :trace :code)
(js-parse "32+abc//23e-a4*7e-2 3 id4 4ef;")
(js-parse "32+abc//23e-a4*7e-2 3 id4 4ef;
")
(js-parse "32+abc/ /23e-a4*7e-2 3 /*id4 4*-/ef;

fjds*/y//z")
(js-parse "3a+in'a+b\\147\"de'\"'\"")
(js-parse "3*/regexp*///x")
(js-parse "/regexp*///x")
(js-parse "if \\x69f \\u0069f")
(js-parse "if \\x69f z\\x20z")
(js-parse "3lbs,3in,3 in 3_in,3_lbs")
(js-parse "3a+b in'a+b\\040\\077\\700\\150\\15A\\69\"de'\"'\"")
|#

