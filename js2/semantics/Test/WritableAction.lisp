(progn
  (defparameter *waw*
    (generate-world
     "WA"
     '((lexer writable-action-lexer
              :lalr-1
              :main
              ((:digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                       ((value $digit-value))))
              (($digit-value integer digit-value digit-char-36)))
       
       (%charclass :digit)
       
       (deftype semantic-exception integer)
       
       (defvar g integer 0)
       
       (rule :expr ((serial-num (writable-cell integer)) (walk (-> () void)) (value (-> () (vector integer))))
         (production :expr (:digit) expr-digit
           ((walk)
            (<- g (+ g 1))
            (action<- (serial-num :expr 0) g))
           ((value)
            (return (vector (+ (* 10 (serial-num :expr 0)) (value :digit))))))
         (production :expr (:digit :expr) expr-more
           ((walk)
            ((walk :expr))
            (<- g (+ g 1))
            (action<- (serial-num :expr 0) g))
           ((value)
            (const i integer (+ (* 10 (serial-num :expr 0)) (value :digit)))
            (return (append (vector i) ((value :expr)))))))
       
       (rule :main ((value (vector integer)))
         (production :main (:expr) main-expr
           (value (begin
                   ((walk :expr))
                   (return ((value :expr)))))))
       (%print-actions)
       )))
  
  (defparameter *wal* (world-lexer *waw* 'writable-action-lexer))
  (defparameter *wag* (lexer-grammar *wal*)))

#|
(depict-rtf-to-local-file
 "Test/WritableActionSemantics.rtf"
 "Writable Action Semantics"
 #'(lambda (rtf-stream)
     (depict-world-commands rtf-stream *waw*)))

(depict-html-to-local-file
 "Test/WritableActionSemantics.html"
 "Writable Action Semantics"
 t
 #'(lambda (html-stream)
     (depict-world-commands html-stream *waw*))
 :external-link-base "")


(lexer-pparse *wal* "7")
(lexer-pparse *wal* "7500")
|#

(length (grammar-states *wag*))
