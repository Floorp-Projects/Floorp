(progn
  (defparameter *tcw*
    (generate-world
     "TC"
     '((lexer throw-catch-lexer
              :lalr-1
              :main
              ((:digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                       ((value $digit-value))))
              (($digit-value integer digit-value digit-char-36)))
       
       (%charclass :digit)
       
       (deftype semantic-exception integer)
       
       (rule :expr ((value (-> () integer)))
         (production :expr (:digit) expr-digit
           ((value) (return (value :digit))))
         (production :expr (#\t :expr) expr-throw
           ((value) (throw ((value :expr)))))
         (production :expr (#\c #\{ :expr #\} :expr) expr-catch
           ((value) (catch ((return ((value :expr 1))))
                    (e) (return (+ (* e 10) ((value :expr 2))))))))
       
       (rule :main ((value integer))
         (production :main (:expr) main-expr
           (value ((value :expr)))))
       (%print-actions)
       )))
  
  (defparameter *tcl* (world-lexer *tcw* 'throw-catch-lexer))
  (defparameter *tcg* (lexer-grammar *tcl*)))

#|
(depict-rtf-to-local-file
 "Test/ThrowCatchSemantics.rtf"
 "Throw-Catch Semantics"
 #'(lambda (rtf-stream)
     (depict-world-commands rtf-stream *tcw*)))

(depict-html-to-local-file
 "Test/ThrowCatchSemantics.html"
 "Throw-Catch Semantics"
 t
 #'(lambda (html-stream)
     (depict-world-commands html-stream *tcw*))
 :external-link-base "")


(lexer-pparse *tcl* "7")
(lexer-pparse *tcl* "t3")
(lexer-pparse *tcl* "c{t6}5")

|#

(length (grammar-states *tcg*))
