(progn
  (defparameter *ltw*
    (generate-world
     "LT"
     '((lexer loop-test-lexer
              :lalr-1
              :main
              ()
              ())
       
       (deftype semantic-exception integer)
       
       (rule :main ((value string))
         (production :main () main-expr
           (value (append (loop-test) (for-each-test)))))
       (%print-actions)
       
       (define (for-each-test) string
         (var s string "<")
         (for-each "abc" x
           (<- s (append s (vector #\- x))))
         (for-each (list-set-of character #\x #\y #\z) x
           (<- s (append s (vector #\- x))))
         (return (append s ">")))
       
       (define (loop-test) string
         (var i integer 7)
         (var s string "<")
         (while (> i 0)
           (<- s (append s (vector (code-to-character (+ 48 i)))))
           (<- i (- i 1)))
         (return (append s ">")))
       )))
  
  (defparameter *ltl* (world-lexer *ltw* 'loop-test-lexer))
  (defparameter *ltg* (lexer-grammar *ltl*)))

#|
(depict-rtf-to-local-file
 "Test/LoopTestSemantics.rtf"
 "Loop Test Semantics"
 #'(lambda (rtf-stream)
     (depict-world-commands rtf-stream *ltw* :heading-offset 1)))

(depict-html-to-local-file
 "Test/LoopTestSemantics.html"
 "Loop Test Semantics"
 t
 #'(lambda (html-stream)
     (depict-world-commands html-stream *ltw* :heading-offset 1))
 :external-link-base "")
|#

(lexer-pparse *ltl* "")
